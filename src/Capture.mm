// SPDX-License-Identifier: GPL-2.0-or-later
#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>
#include <vector>
#include <chrono>
#include "../common/Control.hpp"
#include "../core/Risc.hpp"
io_connect_t connectDriver();
NSDictionary *statusJSON(const cxadc::Status &state);
static NSString *hexCapture(uint32_t value){return [NSString stringWithFormat:@"0x%x",value];}
static volatile sig_atomic_t requestedStop=0;
struct IdleSleepGuard {
    IOPMAssertionID assertion=kIOPMNullAssertionID;
    IOReturn result;
    IdleSleepGuard():result(IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep,
        kIOPMAssertionLevelOn,CFSTR("CXADC raw ADC capture"),&assertion)){}
    ~IdleSleepGuard(){if(!result)IOPMAssertionRelease(assertion);}
};
static void stopSignal(int){requestedStop=1;}
static bool writeAll(int fd,const void *data,size_t bytes) {
    const auto *p=static_cast<const uint8_t*>(data);
    while(bytes) {
        auto count=write(fd,p,bytes);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)return false;
        bytes-=size_t(count);p+=count;
    }
    return true;
}
static bool metadataWrite(int fd,NSDictionary *data) {
    NSData *json=[NSJSONSerialization dataWithJSONObject:data options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:nil];
    return json && lseek(fd,0,SEEK_SET)>=0 && ftruncate(fd,0)==0 && writeAll(fd,json.bytes,json.length) && fsync(fd)==0;
}
int runCapture(int argc,char **argv) {
    cxadc::Settings settings;
    double duration=0;uint32_t ringMiB=64;
    const char *output=nullptr;
    for(int i=1;i<argc;i+=2) {
        if(i+1>=argc)return 64;
        auto key=argv[i],value=argv[i+1];
        if(!strcmp(key,"--output")){output=value;continue;}
        char *end=nullptr;errno=0;
        if(!strcmp(key,"--duration")) {
            duration=strtod(value,&end);
            if(errno || !*value || *end || !std::isfinite(duration) || duration<=0 || duration>86400)return 64;
            continue;
        }
        auto number=strtoul(value,&end,10);
        if(errno || !*value || *end || number>255)return 64;
        if(!strcmp(key,"--vmux"))settings.vmux=uint32_t(number);
        else if(!strcmp(key,"--level"))settings.level=uint32_t(number);
        else if(!strcmp(key,"--sixdb"))settings.sixdb=uint32_t(number);
        else if(!strcmp(key,"--center-offset"))settings.centerOffset=uint32_t(number);
        else if(!strcmp(key,"--agc-enable"))settings.agcEnable=uint32_t(number);
        else if(!strcmp(key,"--ring-mib"))ringMiB=uint32_t(number);
        else return 64;
    }
    if(!output || !strcmp(output,"-") || !cxadc::valid(settings) || (ringMiB!=8 && ringMiB!=64))return 64;
    uint64_t target=duration?uint64_t(duration*28636363.0):UINT64_MAX;
    if(!target)return 64;
    auto connection=connectDriver();if(!connection)return 1;
    IdleSleepGuard awake;
    if(awake.result){fprintf(stderr,"Idle-sleep assertion failed: 0x%x\n",awake.result);IOServiceClose(connection);return 1;}
    int fd=open(output,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);
    if(fd<0){perror("create capture");IOServiceClose(connection);return 1;}
    std::string metadata=std::string(output)+".json";
    int meta=open(metadata.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);
    if(meta<0){perror("create metadata");close(fd);IOServiceClose(connection);return 1;}
    auto timestamp=[[NSISO8601DateFormatter new] stringFromDate:[NSDate date]];
    NSMutableDictionary *record=[@{@"schema":@1,@"timestamp_utc":timestamp,@"chip":@"Conexant CX23880",@"pci_id":@"14f1:8800",
        @"format":@"u8",@"nominal_sample_rate_hz":@28636363,@"sample_clock_measured":@NO,
        @"requested_duration_seconds":@(duration),@"vmux":@(settings.vmux),@"level":@(settings.level),@"sixdb":@(settings.sixdb),
        @"center_offset":@(settings.centerOffset),@"agc_enable":@(settings.agcEnable),@"state":@"incomplete",@"written_bytes":@0,
        @"idle_sleep_prevention":@YES,@"dropped_sample_count":[NSNull null],@"continuity_claim":@"No authoritative dropped-sample count; errors and ambiguity terminate capture."} mutableCopy];
    if(!metadataWrite(meta,record)){perror("write initial metadata");close(meta);close(fd);IOServiceClose(connection);return 1;}
    signal(SIGINT,stopSignal);signal(SIGTERM,stopSignal);signal(SIGPIPE,SIG_IGN);
    const uint64_t inputs[]={settings.vmux,settings.level,settings.sixdb,settings.centerOffset,settings.agcEnable};
    auto r=IOConnectCallMethod(connection,cxadc::configure,inputs,5,nullptr,0,nullptr,nullptr,nullptr,nullptr);
    cxadc::Status before;size_t stateSize=sizeof(before);
    if(!r)r=IOConnectCallStructMethod(connection,cxadc::status,nullptr,0,&before,&stateSize);
    if(!r && (stateSize!=sizeof(before) || before.version!=cxadc::controlVersion || !before.initialized))r=kIOReturnUnsupported;
    std::mutex mutex;std::condition_variable wake;
    constexpr unsigned slots=32;
    std::vector<std::unique_ptr<uint8_t[]>> buffers;
    for(unsigned i=0;i<slots;++i)buffers.emplace_back(new uint8_t[cxadc::chunkBytes]);
    struct Item { unsigned slot;size_t bytes; };
    std::deque<unsigned> free;for(unsigned i=0;i<slots;++i)free.push_back(i);
    std::deque<Item> pending;
    bool done=false;std::atomic<int> writerError{0};std::atomic<uint64_t> written{0};
    std::thread writer([&]{
        for(;;) {
            Item item;
            {std::unique_lock lock(mutex);wake.wait(lock,[&]{return done || !pending.empty();});
             if(pending.empty())break;item=pending.front();pending.pop_front();}
            const uint8_t *bytes=buffers[item.slot].get();size_t remaining=item.bytes;
            while(remaining) {
                auto count=write(fd,bytes,remaining);
                if(count<0 && errno==EINTR)continue;
                if(count<=0){writerError=errno?errno:EIO;break;}
                written+=uint64_t(count);bytes+=count;remaining-=size_t(count);
            }
            if(writerError)break;
            {std::lock_guard lock(mutex);free.push_back(item.slot);}
        }
    });
    bool started=false;
    if(!r) {
        uint64_t ring=ringMiB;
        r=IOConnectCallScalarMethod(connection,cxadc::startCapture,&ring,1,nullptr,nullptr);
        started=!r;
    }
    auto begin=std::chrono::steady_clock::now(),lastProgress=begin,lastPrint=begin;
    uint64_t queued=0;
    while(!r && !requestedStop && queued<target) {
        if(writerError){r=kIOReturnIOError;break;}
        unsigned slot=0;
        {std::lock_guard lock(mutex);if(free.empty()){r=kIOReturnOverrun;break;}slot=free.front();free.pop_front();}
        size_t bytes=cxadc::chunkBytes;
        r=IOConnectCallStructMethod(connection,cxadc::pull,nullptr,0,buffers[slot].get(),&bytes);
        if(r==kIOReturnNotReady) {
            {std::lock_guard lock(mutex);free.push_front(slot);}
            r=kIOReturnSuccess;
            if(std::chrono::steady_clock::now()-lastProgress>std::chrono::seconds(2)){r=kIOReturnTimeout;break;}
            usleep(1000);continue;
        }
        if(r)break;
        if(bytes!=cxadc::chunkBytes){r=kIOReturnUnderrun;break;}
        lastProgress=std::chrono::steady_clock::now();
        size_t keep=size_t(std::min<uint64_t>(bytes,target-queued));queued+=keep;
        {std::lock_guard lock(mutex);pending.push_back({slot,keep});}wake.notify_one();
        if(lastProgress-lastPrint>std::chrono::seconds(1)) {
            fprintf(stderr,"CXADC: written=%llu bytes queued=%llu\n",(unsigned long long)written.load(),(unsigned long long)queued);
            lastPrint=lastProgress;
        }
    }
    // Always stop hardware before waiting for storage to drain.
    auto stopResult=IOConnectCallScalarMethod(connection,cxadc::stopCapture,nullptr,0,nullptr,nullptr);
    if(!r)r=stopResult;
    {std::lock_guard lock(mutex);done=true;}wake.notify_one();writer.join();
    if(writerError){fprintf(stderr,"capture write failed: %s\n",strerror(writerError));if(!r)r=kIOReturnIOError;}
    if(fsync(fd) && !r)r=kIOReturnIOError;
    if(close(fd) && !r)r=kIOReturnIOError;
    cxadc::Status final;stateSize=sizeof(final);
    auto finalResult=IOConnectCallStructMethod(connection,cxadc::status,nullptr,0,&final,&stateSize);
    IOServiceClose(connection);
    if(!r)r=finalResult;
    if(!finalResult && stateSize==sizeof(final) && final.version==cxadc::controlVersion) {
        record[@"driver"]=statusJSON(final);
        if(!r && (final.stream.error || !final.stream.stopSafe || final.quarantined))r=final.stream.error?final.stream.error:kIOReturnIOError;
    } else if(!r)r=kIOReturnUnsupported;
    if(!r && (!started || written!=queued))r=kIOReturnIOError;
    record[@"written_bytes"]=@(written.load());record[@"queued_bytes"]=@(queued);
    record[@"host_elapsed_seconds"]=@(std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
    record[@"state"]=r?@"failed":@"complete";
    record[@"stopped_by_signal"]=@(requestedStop!=0);record[@"io_return"]=hexCapture(uint32_t(r));
    record[@"writer_errno"]=@(writerError.load());record[@"stop_io_return"]=hexCapture(uint32_t(stopResult));
    if(!metadataWrite(meta,record)){perror("final metadata");if(!r)r=kIOReturnIOError;}
    close(meta);
    fprintf(stderr,"CXADC: %s, written=%llu, result=0x%x, metadata=%s\n",r?"FAILED":"complete",(unsigned long long)written.load(),r,metadata.c_str());
    return r?1:0;
}
