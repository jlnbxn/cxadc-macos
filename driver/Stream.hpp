// SPDX-License-Identifier: GPL-2.0-or-later
// Linux-derived hardware portions adapted for macOS, 2026-10-21.
// Upstream copyrights/license and modification scope: THIRD_PARTY_NOTICES.md.
#pragma once
#include "DmaMemory.hpp"
#include "../core/Risc.hpp"
#include <DriverKit/IOLib.h>
#include <DriverKit/IOMemoryMap.h>
#include <PCIDriverKit/IOPCIDevice.h>
#include <time.h>
#include <string.h>
#include <os/log.h>
inline uint64_t monotonicNS(){return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);}
struct Stream {
    IOPCIDevice *pci=nullptr;uint8_t index=0;uint16_t originalCommand=0;
    DmaMemory samples,program,staging;
    IOAddressSegment cpu{};
    cxadc::StreamStats stats{};
    uint64_t lastObservation=0,limitNS=0;
    bool armed=false,access=false;
    uint32_t read(uint32_t off){uint32_t v=~0u;pci->MemoryRead32(index,off,&v);return v;}
    void write(uint32_t off,uint32_t v){pci->MemoryWrite32(index,off,v);}
    uint16_t command(){uint16_t v=0xffff;pci->ConfigurationRead16(4,&v);return v;}
    kern_return_t release() {
        auto a=samples.release(),b=program.release(),c=staging.release();
        return a?a:b?b:c;
    }
    kern_return_t prepare(IOPCIDevice *device,uint8_t memoryIndex,uint32_t ringMiB) {
        pci=device;index=memoryIndex;
        if(ringMiB!=8 && ringMiB!=64)return kIOReturnBadArgument;
        stats.ringBytes=ringMiB*1024*1024;
        originalCommand=command();
        if(originalCommand==0xffff || (originalCommand&4))return kIOReturnBusy;
        pci->ConfigurationWrite16(4,originalCommand|2);
        if(command()!=(originalCommand|2))return kIOReturnIOError;
        access=true;
        if(read(0x31c040) || read(0x31c044) || read(0x200040) || read(0x200050))return kIOReturnBusy;
        auto r=samples.allocate(pci,uint64_t(stats.ringBytes)+16384);
        if(!r)r=program.allocate(pci,272*1024);
        if(!r)r=staging.allocate(pci,272*1024);
        if(!r && program.count!=1)r=kIOReturnUnsupported;
        if(!r)r=staging.memory->GetAddressRange(&cpu);
        if(!r && (!cpu.address || cpu.length<272*1024))r=kIOReturnVMError;
        cxadc::Segment segments[32];for(unsigned i=0;i<samples.count;++i)segments[i]={samples.segments[i].address,samples.segments[i].length};
        size_t words=0;
        if(!r && !cxadc::buildRisc(segments,samples.count,stats.ringBytes,program.segments[0].address,
                                 reinterpret_cast<uint32_t*>(cpu.address),cpu.length/4,words))r=kIOReturnUnsupported;
        if(!r)r=program.dma->PerformOperation(kIODMACommandPerformOperationOptionWrite,0,words*4,0,staging.memory);
        if(!r)r=samples.dma->PerformOperation(kIODMACommandPerformOperationOptionZero,0,samples.bytes,0,nullptr);
        if(!r) {
            memset(reinterpret_cast<void*>(cpu.address),0xa5,16384);
            r=samples.dma->PerformOperation(kIODMACommandPerformOperationOptionWrite,stats.ringBytes,16384,0,staging.memory);
        }
        stats.programWords=uint32_t(words);
        return r;
    }
    kern_return_t start() {
        // Every mapping, full IOVA range, address operand and program size has
        // been checked before the first bus-master enable below.
        write(0x200040,0);write(0x200050,0);write(0x31c040,0);write(0x200034,0);
        for(uint32_t i=0;i<8;++i) {
            write(0x181000+16*i,0x184000+2048*i);
            for(uint32_t j=4;j<16;j+=4)write(0x181000+16*i+j,0);
        }
        write(0x30010c,255);write(0x3000cc,0x181000);write(0x30014c,16);
        write(0x180100,uint32_t(program.segments[0].address));write(0x180104,0x181000);
        write(0x180108,16);write(0x18010c,0x180800);write(0x180110,0x40);
        for(uint32_t off=0x180114;off<0x180140;off+=4)write(off,0);
        const uint32_t cnt1=read(0x30010c),ptr2=read(0x3000cc),cnt2=read(0x30014c),base=read(0x180100);
        os_log(OS_LOG_DEFAULT,"CXADC: channel24 readback cnt1=%08x ptr2=%08x cnt2=%08x program=%08x expected=%08x",cnt1,ptr2,cnt2,base,uint32_t(program.segments[0].address));
        // Datasheet p6-85: writing CNT2 begins using the CDT. PTR2/CNT1/CNT2
        // are live hardware state, not static write/readback latches.
        if(cnt1==~0u || ptr2==~0u || cnt2==~0u || base!=uint32_t(program.segments[0].address) ||
           read(0x180104)!=0x181000 || read(0x180108)!=16 || read(0x18010c)!=0x180800 || read(0x180110)!=0x40)return kIOReturnIOError;
        for(uint32_t i=0;i<8;++i) {
            if(read(0x181000+16*i)!=0x184000+2048*i)return kIOReturnIOError;
            for(uint32_t j=4;j<16;j+=4)if(read(0x181000+16*i+j)!=0)return kIOReturnIOError;
        }
        write(0x200054,0xffffffff);write(0x200044,0xfc00);
        write(0x31c03c,3);
        write(0x200050,0xf8808);write(0x200040,0xfc01);
        if(command()!=(originalCommand|2))return kIOReturnBusy;
        armed=true;
        pci->ConfigurationWrite16(4,originalCommand|6);
        if(command()!=(originalCommand|6))return kIOReturnIOError;
        stats.running=1;stats.startedNS=lastObservation=monotonicNS();
        // Conservative ambiguity bound assumes up to 60 MSPS, not the nominal
        // oscillator rate. Never infer a full-lap count from a modulo counter.
        limitNS=uint64_t(stats.ringBytes)*1000000000/120000000;
        write(0x200034,0x20);write(0x31c040,0x88);
        if(read(0x31c040)!=0x88)return kIOReturnIOError;
        return kIOReturnSuccess;
    }
    kern_return_t observe(bool interrupt) {
        if(!stats.running)return stats.error;
        auto now=monotonicNS();
        auto video=read(0x200054),pciStatus=read(0x200044);
        stats.videoStatus|=video;stats.pciStatus|=pciStatus;
        if(video==~0u || pciStatus==~0u)return kIOReturnNoDevice;
        if((video&0xf8800) || (pciStatus&0xfc00))return kIOReturnIOError;
        if(now-lastObservation>=limitNS){++stats.overruns;return kIOReturnOverrun;}
        if(interrupt && (video&8)) {
            // Counter read only following the latched RISC IRQ1 event. Keep
            // one entire completed IRQ block back before exposing samples.
            auto count=read(0x31c02c);
            if(count==~0u)return kIOReturnNoDevice;
            count&=~15u;
            uint32_t delta=0,pages=stats.ringBytes/cxadc::dmaPage;
            if(!cxadc::advance(stats.lastCounter,count,pages,delta)){++stats.overruns;return kIOReturnOverrun;}
            if(count<stats.lastCounter)++stats.wraps;
            stats.completedPages+=delta;stats.lastCounter=count;lastObservation=now;
            if(interrupt)++stats.interrupts;
            write(0x200054,8);
        }
        if(stats.completedPages*cxadc::dmaPage-stats.deliveredBytes>=stats.ringBytes-2*cxadc::chunkBytes) {
            ++stats.overruns;return kIOReturnOverrun;
        }
        return kIOReturnSuccess;
    }
    kern_return_t copy(IOMemoryDescriptor *destination) {
        if(stats.error)return stats.error;
        if(!stats.running)return kIOReturnNotReady;
        // Pull never polls completion. The actual interrupt handler owns it.
        auto available=stats.completedPages*cxadc::dmaPage;
        if(available<stats.deliveredBytes+2*cxadc::chunkBytes)return kIOReturnNotReady;
        auto verify=[&]()->kern_return_t {
            if(monotonicNS()-lastObservation>=limitNS)return kIOReturnOverrun;
            auto counter=read(0x31c02c);uint32_t delta=0;
            if(counter==~0u)return kIOReturnNoDevice;
            if(!cxadc::advance(stats.lastCounter,counter,stats.ringBytes/cxadc::dmaPage,delta))return kIOReturnOverrun;
            uint64_t current=(stats.completedPages+delta)*cxadc::dmaPage;
            return current-stats.deliveredBytes>=stats.ringBytes-cxadc::chunkBytes?kIOReturnOverrun:kIOReturnSuccess;
        };
        auto r=verify();if(r)return r;
        r=samples.dma->PerformOperation(kIODMACommandPerformOperationOptionRead,
            stats.deliveredBytes%stats.ringBytes,cxadc::chunkBytes,0,staging.memory);
        if(r)return r;
        r=verify();if(r)return r;
        uint64_t size=0;r=destination->GetLength(&size);if(r)return r;
        if(size!=cxadc::chunkBytes)return kIOReturnBadArgument;
        IOMemoryMap *map=nullptr;r=destination->CreateMapping(0,0,0,size,0,&map);if(r)return r;
        if(!map || !map->GetAddress() || map->GetLength()<size){if(map)map->release();return kIOReturnVMError;}
        memcpy(reinterpret_cast<void*>(map->GetAddress()),reinterpret_cast<void*>(cpu.address),size);map->release();
        stats.deliveredBytes+=size;
        return kIOReturnSuccess;
    }
    kern_return_t stop() {
        stats.running=0;
        if(!pci)return kIOReturnNotReady;
        if(originalCommand==0xffff || (originalCommand&4))return kIOReturnBusy;
        if(!access) {
            pci->ConfigurationWrite16(4,originalCommand);
            stats.commandRestored=command();
            if(stats.commandRestored!=originalCommand)return kIOReturnIOError;
            return release();
        }
        write(0x200040,0);write(0x200050,0);write(0x31c040,0);write(0x200034,0);
        if(armed) {
            // Datasheet p6-86 defines XFER_IN_PROG. Do not use CX23885 TC_REQ
            // offsets, or pretend the write-only RUN_RISC has a readback.
            for(unsigned i=0;i<100 && read(0x31c044)!=0;++i)IOSleep(1);
            auto cmd=command();if(cmd!=0xffff)pci->ConfigurationWrite16(4,(cmd|2)&~4u);
            IOSleep(20);
            auto first=read(0x31c02c);IOSleep(20);
            stats.transferStatus=read(0x31c044);
            if(command()==0xffff || (command()&4) || read(0x31c040) ||
               stats.transferStatus || first==~0u || read(0x31c02c)!=first)return kIOReturnNotReady;
            auto r=samples.dma->PerformOperation(kIODMACommandPerformOperationOptionRead,stats.ringBytes,16384,0,staging.memory);
            auto guard=reinterpret_cast<uint8_t*>(cpu.address);
            for(unsigned i=0;!r && i<16384;++i)if(guard[i]!=0xa5)r=kIOReturnOverrun;
            if(r)return r;
        }
        // Clear only known interrupt status after engines and mastering stop.
        write(0x200054,0xffffffff);write(0x200044,0xfc00);
        pci->ConfigurationWrite16(4,originalCommand);
        stats.commandRestored=command();stats.stoppedNS=monotonicNS();
        if(stats.commandRestored!=originalCommand)return kIOReturnIOError;
        auto r=release();if(r)return r;
        stats.stopSafe=1;armed=false;
        return kIOReturnSuccess;
    }
};
