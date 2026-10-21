// SPDX-License-Identifier: GPL-2.0-or-later
#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include "../common/Control.hpp"

static NSString *hexControl(uint64_t value){return [NSString stringWithFormat:@"0x%llx",(unsigned long long)value];}
io_connect_t connectDriver() {
    auto match=IOServiceMatching("IOUserService");
    CFMutableDictionaryRef props=CFDictionaryCreateMutable(nullptr,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(props,CFSTR("IOUserClass"),CFSTR("CXADCDriver"));
    CFDictionarySetValue(match,CFSTR("IOPropertyMatch"),props);CFRelease(props);
    auto service=IOServiceGetMatchingService(kIOMainPortDefault,match);
    if(!service){fprintf(stderr,"CXADC driver is not attached\n");return 0;}
    io_connect_t connection=0;
    auto r=IOServiceOpen(service,mach_task_self(),0,&connection);IOObjectRelease(service);
    if(r){fprintf(stderr,"IOServiceOpen failed: 0x%x\n",r);return 0;}
    return connection;
}
NSDictionary *statusJSON(const cxadc::Status &state) {
    return @{
        @"driver_api":@(state.version), @"driver_build":@(state.build), @"initialized":@(state.initialized), @"quarantined":@(state.quarantined),
        @"command_before":hexControl(state.command), @"command_restored":hexControl(state.restoredCommand),
        @"vmux":@(state.settings.vmux), @"level":@(state.settings.level), @"sixdb":@(state.settings.sixdb), @"center_offset":@(state.settings.centerOffset),
        @"nominal_sample_rate_hz":@28636363, @"sample_clock_measured":@NO, @"format":@"u8",
        @"video_dma":hexControl(state.videoDma), @"device_control_readback_undefined":hexControl(state.deviceControl),
        @"video_status":hexControl(state.videoStatus), @"pci_status":hexControl(state.pciStatus),
        @"irq_create_result":hexControl(state.irqCreate), @"irq_type":hexControl(state.irqType),
        @"mapping_result":hexControl(state.mappingResult), @"sample_iova":hexControl(state.sampleIOVA),
        @"program_iova":hexControl(state.programIOVA), @"sample_map_segments":@(state.mapCount), @"mapped_sample_bytes":@(state.mappedBytes),
        @"register_mismatch":hexControl(state.registerMismatch), @"expected":hexControl(state.expected), @"observed":hexControl(state.observed),
        @"stream":@{
            @"running":@(state.stream.running), @"error":hexControl(state.stream.error),
            @"interrupt_callbacks":@(state.stream.interrupts), @"completed_pages":@(state.stream.completedPages),
            @"delivered_bytes":@(state.stream.deliveredBytes), @"ring_wraps_observed":@(state.stream.wraps),
            @"ring_bytes":@(state.stream.ringBytes), @"overrun_events":@(state.stream.overruns),
            @"stop_verified":@(state.stream.stopSafe), @"command_restored":hexControl(state.stream.commandRestored),
            @"transfer_status":hexControl(state.stream.transferStatus), @"video_status":hexControl(state.stream.videoStatus),
            @"pci_status":hexControl(state.stream.pciStatus), @"last_counter":@(state.stream.lastCounter),
            @"started_monotonic_ns":@(state.stream.startedNS), @"stopped_monotonic_ns":@(state.stream.stoppedNS),
            @"risc_words":@(state.stream.programWords), @"dropped_sample_count": [NSNull null],
        },
    };
}
int runControl(int argc,char **argv) {
    cxadc::Settings settings;
    uint64_t selector=0;
    if(!strcmp(argv[1],"--configure"))selector=cxadc::configure;
    else if(!strcmp(argv[1],"--dma-check"))selector=cxadc::dmaCheck;
    else if(strcmp(argv[1],"--status"))return 64;
    for(int i=2;i<argc;i+=2) {
        if(selector!=cxadc::configure || i+1>=argc)return 64;
        errno=0;char *end=nullptr;auto value=strtoul(argv[i+1],&end,10);
        if(errno || !*argv[i+1] || *end || value>255)return 64;
        if(!strcmp(argv[i],"--vmux"))settings.vmux=uint32_t(value);
        else if(!strcmp(argv[i],"--level"))settings.level=uint32_t(value);
        else if(!strcmp(argv[i],"--sixdb"))settings.sixdb=uint32_t(value);
        else if(!strcmp(argv[i],"--center-offset"))settings.centerOffset=uint32_t(value);
        else return 64;
    }
    if(!cxadc::valid(settings))return 64;
    auto connection=connectDriver();if(!connection)return 1;
    kern_return_t r=0;
    if(selector==cxadc::configure) {
        const uint64_t input[]={settings.vmux,settings.level,settings.sixdb,settings.centerOffset};
        r=IOConnectCallMethod(connection,selector,input,4,nullptr,0,nullptr,nullptr,nullptr,nullptr);
    }
    cxadc::Status state;size_t size=sizeof(state);
    auto statusResult=IOConnectCallStructMethod(connection,selector==cxadc::dmaCheck?cxadc::dmaCheck:cxadc::status,nullptr,0,&state,&size);
    IOServiceClose(connection);
    if(!r)r=statusResult;
    if(r)fprintf(stderr,"CXADC operation failed: 0x%x; snapshot status=0x%x\n",r,statusResult);
    if(statusResult || size!=sizeof(state) || state.version!=cxadc::controlVersion)return 1;
    NSDictionary *report=statusJSON(state);
    NSData *json=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:nil];
    if(!json || fwrite(json.bytes,1,json.length,stdout)!=json.length || fputc('\n',stdout)==EOF || fflush(stdout))return 1;
    return r?1:0;
}
