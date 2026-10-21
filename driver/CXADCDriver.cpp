// SPDX-License-Identifier: GPL-2.0-or-later
// Lifecycle/build pattern adapted from hvr1200-macos (see docs/REFERENCES.md).
#include <DriverKit/IOLib.h>
#include <DriverKit/OSData.h>
#include <DriverKit/OSDictionary.h>
#include <DriverKit/IOInterruptDispatchSource.h>
#include <PCIDriverKit/IOPCIDevice.h>
#include <os/log.h>
#include "CXADCDriver.h"
#include "../common/Probe.hpp"
#include "../common/Control.hpp"
#include "../core/Raw.hpp"
#include "DmaMemory.hpp"
#include "Stream.hpp"
#include <DriverKit/IOTimerDispatchSource.h>
#include <DriverKit/IODispatchQueue.h>

struct CXADCDriver_IVars { IOPCIDevice *pci = nullptr; bool open = false; uint8_t index=0; cxadc::Status state{}; Stream *stream=nullptr; IOUserClient *owner=nullptr;
 IOInterruptDispatchSource *irq=nullptr; IOTimerDispatchSource *timer=nullptr; OSAction *irqAction=nullptr,*timerAction=nullptr; };
bool CXADCDriver::init() {
    if (!super::init()) return false;
    ivars = new CXADCDriver_IVars;
    return ivars != nullptr;
}
void CXADCDriver::free() {
    if (ivars) {
        if (ivars->pci) ivars->pci->release();
        delete ivars;
    }
    super::free();
}
kern_return_t IMPL(CXADCDriver, Start) {
    auto result = Start(provider, SUPERDISPATCH);
    if (result) return result;
    auto pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) return kIOReturnBadArgument;
    result = pci->Open(this, 0);
    if (result) return result;
    cxadc::Probe probe;
    pci->ConfigurationRead32(0, &probe.primary);
    pci->ConfigurationRead32(0x2c, &probe.subsystem);
    if (probe.primary != 0x880014f1 || probe.subsystem != 0x45534e4f) {
        pci->Close(this, 0);
        return kIOReturnUnsupported;
    }
    uint16_t command = 0;
    pci->ConfigurationRead16(4, &command);
    probe.command = command;
    pci->ConfigurationRead32(0x10, &probe.bar0);
    pci->ConfigurationRead32(0x3c, &probe.interruptConfig);
    uint8_t index = 0, type = 0;
    probe.barStatus = pci->GetBARInfo(kPCIMemoryRangeBAR0, &index, &probe.barSize, &type);
    probe.barIndex = index; probe.barType = type;
    probe.interruptStatus = IOInterruptDispatchSource::GetInterruptType(pci, 0, &probe.interruptType);
    os_log(OS_LOG_DEFAULT, "CXADC: primary=%08x subsystem=%08x command=%04x BAR0=%08x size=%llu irq-config=%08x irq-result=%08x",
           probe.primary, probe.subsystem, command, probe.bar0, probe.barSize, probe.interruptConfig, probe.interruptStatus);
    // The only hardware write in this bounded probe is temporarily setting
    // PCI command Memory Space Enable. Preserve every other command bit and
    // restore the original word before publishing. Never enable bus mastering.
    // No reset, MMIO writes, DMA mapping, interrupt enable or acknowledgement.
    probe.commandAfter = command;
    if (command != 0xffff && !(command & 4) &&
        !probe.barStatus && !(type & 1) && probe.barSize >= 0x31c044) {
        uint16_t enabled = command;
        if (!(command & 2)) {
            pci->ConfigurationWrite16(4, command | 2);
            pci->ConfigurationRead16(4, &enabled);
        }
        if (enabled == (command | 2)) {
            probe.mmioRead = 1; probe.mmioValid = 1;
            for (unsigned i = 0; i < cxadc::registerCount; ++i) {
                pci->MemoryRead32(index, cxadc::registers[i].offset, &probe.values[i]);
                os_log(OS_LOG_DEFAULT, "CXADC: BAR0[%06x]=%08x", cxadc::registers[i].offset, probe.values[i]);
                if (probe.values[i] == 0xffffffff) { probe.mmioValid = 0; break; }
            }
        }
        if (!(command & 2)) pci->ConfigurationWrite16(4, command);
        uint16_t restored = 0xffff;
        pci->ConfigurationRead16(4, &restored);
        probe.commandAfter = restored;
        os_log(OS_LOG_DEFAULT, "CXADC: PCI command restored=%04x expected=%04x", restored, command);
        if (restored != command) { pci->Close(this, 0); return kIOReturnIOError; }
    }
    auto properties = OSDictionary::withCapacity(1);
    auto data = OSData::withBytes(&probe, sizeof(probe));
    if (!properties || !data) result = kIOReturnNoMemory;
    else {
        properties->setObject("CXADCProbe", data);
        result = SetProperties(properties);
    }
    if (data) data->release();
    if (properties) properties->release();
    if (result) { pci->Close(this, 0); return result; }
    pci->retain(); ivars->pci = pci; ivars->open = true; ivars->index=index;
    SetupEvents();
    result = RegisterService();
    if (result) { pci->Close(this, 0); ivars->open = false; }
    os_log(OS_LOG_DEFAULT, "CXADC: attachment probe finished, mmio-read=%u valid=%u; engines remain off", probe.mmioRead, probe.mmioValid);
    return result;
}
kern_return_t IMPL(CXADCDriver, Stop) {
    StopStream();
    if(ivars->irq){auto source=ivars->irq;auto action=ivars->irqAction;ivars->irq=nullptr;ivars->irqAction=nullptr;source->Cancel(^{source->release();if(action)action->release();});}
    if(ivars->timer){auto source=ivars->timer;auto action=ivars->timerAction;ivars->timer=nullptr;ivars->timerAction=nullptr;source->Cancel(^{source->release();if(action)action->release();});}
    if (ivars->open) { ivars->pci->Close(this, 0); ivars->open = false; }
    return Stop(provider, SUPERDISPATCH);
}
kern_return_t IMPL(CXADCDriver, NewUserClient) {
    if(type)return kIOReturnUnsupported;
    IOService *service=nullptr;
    auto r=Create(this,"IOUserClientProperties",&service);if(r)return r;
    *userClient=OSDynamicCast(IOUserClient,service);
    if(!*userClient){service->release();return kIOReturnError;}
    return kIOReturnSuccess;
}
kern_return_t CXADCDriver::ClientClosed(IOUserClient *client) { return client==ivars->owner?StopStream():kIOReturnSuccess; }
void CXADCDriver::SetupEvents() {
    IODispatchQueue *queue=nullptr;
    auto r=CopyDispatchQueue(kIOServiceDefaultQueueName,&queue);
    if(r){ivars->state.irqCreate=r;return;}
    r=IOInterruptDispatchSource::Create(ivars->pci,0,queue,&ivars->irq);
    if(!r)r=CreateActionInterruptEvent(0,&ivars->irqAction);
    if(!r)r=ivars->irq->SetHandler(ivars->irqAction);
    ivars->state.irqCreate=r;
    if(!r)r=IOTimerDispatchSource::Create(queue,&ivars->timer);
    if(!r)r=CreateActionTimerEvent(0,&ivars->timerAction);
    if(!r)r=ivars->timer->SetHandler(ivars->timerAction);
    if(r)ivars->state.irqCreate=r;
    queue->release();
}
kern_return_t CXADCDriver::StartStream(IOUserClient *client,uint32_t ringMiB) {
    if(ringMiB!=8 && ringMiB!=64)return kIOReturnBadArgument;
    if(!ivars->state.initialized || ivars->state.quarantined || ivars->stream ||
       !ivars->irq || !ivars->timer || ivars->state.irqCreate)return kIOReturnNotReady;
    ivars->stream=new Stream;ivars->owner=client;
    if(!ivars->stream){ivars->owner=nullptr;return kIOReturnNoMemory;}
    auto r=ivars->stream->prepare(ivars->pci,ivars->index,ringMiB);
    os_log(OS_LOG_DEFAULT,"CXADC: capture prepare=%08x",r);
    if(!r){r=ivars->irq->SetEnable(true);os_log(OS_LOG_DEFAULT,"CXADC: interrupt enable=%08x",r);}
    if(!r){r=ivars->timer->SetEnable(true);os_log(OS_LOG_DEFAULT,"CXADC: watchdog enable=%08x",r);}
    if(!r){r=ivars->stream->start();os_log(OS_LOG_DEFAULT,"CXADC: engine start=%08x",r);}
    if(!r)r=ivars->timer->WakeAtTime(kIOTimerClockUptimeRaw,monotonicNS()+5000000,0);
    if(r){ivars->stream->stats.error=r;StopStream();}
    else {
        ivars->state.sampleIOVA=ivars->stream->samples.segments[0].address;
        ivars->state.programIOVA=ivars->stream->program.segments[0].address;
        ivars->state.mappedBytes=ivars->stream->samples.bytes;
        ivars->state.mapCount=ivars->stream->samples.count;
        os_log(OS_LOG_DEFAULT,"CXADC: raw DMA started ringMiB=%u sampleIOVA=%llx programIOVA=%llx",ringMiB,ivars->state.sampleIOVA,ivars->state.programIOVA);
    }
    return r;
}
kern_return_t CXADCDriver::StopStream() {
    if(!ivars->stream)return kIOReturnSuccess;
    if(ivars->irq)ivars->irq->SetEnable(false);
    if(ivars->timer)ivars->timer->SetEnable(false);
    auto r=ivars->stream->stop();
    ivars->state.stream=ivars->stream->stats;
    if(r) {
        ivars->state.quarantined=1;ivars->state.initialized=0;
        os_log(OS_LOG_DEFAULT,"CXADC: stop could not establish safe cleanup: %08x; mappings retained",r);
    } else {
        delete ivars->stream;ivars->stream=nullptr;
    }
    ivars->owner=nullptr;
    os_log(OS_LOG_DEFAULT,"CXADC: stop result=%08x safe=%u command=%04x IRQs=%llu delivered=%llu wraps=%llu stream-error=%08x",
           r,ivars->state.stream.stopSafe,ivars->state.stream.commandRestored,ivars->state.stream.interrupts,
           ivars->state.stream.deliveredBytes,ivars->state.stream.wraps,ivars->state.stream.error);
    return r;
}
void IMPL(CXADCDriver, InterruptEvent) {
    (void)action;(void)count;(void)time;
    if(!ivars->stream || !ivars->stream->stats.running)return;
    auto r=ivars->stream->observe(true);
    if(r){ivars->stream->stats.error=r;StopStream();}
    else ivars->irq->SetEnable(true);
}
void IMPL(CXADCDriver, TimerEvent) {
    (void)action;(void)time;
    if(!ivars->stream || !ivars->stream->stats.running)return;
    auto r=ivars->stream->observe(false);
    if(!r)r=ivars->timer->WakeAtTime(kIOTimerClockUptimeRaw,monotonicNS()+5000000,0);
    if(r){ivars->stream->stats.error=r;StopStream();}
}
kern_return_t CXADCDriver::Command(uint64_t selector,IOUserClientMethodArguments *args,IOUserClient *client) {
    if(!args || !ivars->open)return kIOReturnNotReady;
    if(selector>=cxadc::startCapture && selector<=cxadc::stopCapture) {
        if(args->completion || args->structureInput || args->structureInputDescriptor || args->scalarOutputCount)return kIOReturnBadArgument;
        if(selector==cxadc::startCapture) {
            if(args->scalarInputCount!=1 || !args->scalarInput || args->scalarInput[0]>64 || args->structureOutputMaximumSize || args->structureOutputDescriptor)return kIOReturnBadArgument;
            return StartStream(client,uint32_t(args->scalarInput[0]));
        }
        if(args->scalarInputCount)return kIOReturnBadArgument;
        if(ivars->owner && ivars->owner!=client)return kIOReturnNotPermitted;
        if(selector==cxadc::stopCapture) {
            if(args->structureOutputMaximumSize || args->structureOutputDescriptor)return kIOReturnBadArgument;
            return StopStream();
        }
        if(!args->structureOutputDescriptor || args->structureOutputMaximumSize!=cxadc::chunkBytes)return kIOReturnBadArgument;
        if(!ivars->stream)return ivars->state.stream.error?ivars->state.stream.error:kIOReturnNotReady;
        auto result=ivars->stream->copy(args->structureOutputDescriptor);
        ivars->state.stream=ivars->stream->stats;
        if(result && result!=kIOReturnNotReady){ivars->stream->stats.error=result;StopStream();}
        return result;
    }
    if(selector==cxadc::status && ((ivars->stream && ivars->stream->stats.running) || ivars->state.quarantined)) {
        if(args->completion || args->scalarInputCount || args->scalarOutputCount || args->structureInput || args->structureInputDescriptor ||
           args->structureOutputDescriptor || args->structureOutputMaximumSize!=sizeof(cxadc::Status))return kIOReturnBadArgument;
        if(ivars->stream){ivars->state.stream=ivars->stream->stats;
            ivars->state.command=ivars->stream->command();ivars->state.videoDma=ivars->stream->read(0x31c040);}
        args->structureOutput=OSData::withBytes(&ivars->state,sizeof(ivars->state));
        return args->structureOutput?kIOReturnSuccess:kIOReturnNoMemory;
    }
    if(args->completion || args->structureInput || args->structureInputDescriptor ||
       args->structureOutputDescriptor || args->scalarOutputCount)return kIOReturnBadArgument;
    bool output=selector==cxadc::status || selector==cxadc::dmaCheck;
    if(selector>cxadc::dmaCheck || args->scalarInputCount!=(selector==cxadc::configure?4:0) ||
       args->structureOutputMaximumSize!=(output?sizeof(cxadc::Status):0))return kIOReturnBadArgument;
    auto pci=ivars->pci; auto index=ivars->index;
    auto read=[&](uint32_t off){uint32_t value=~0u;pci->MemoryRead32(index,off,&value);return value;};
    auto write=[&](uint32_t off,uint32_t value){pci->MemoryWrite32(index,off,value);};
    uint16_t original=0xffff;pci->ConfigurationRead16(4,&original);
    if(original==0xffff || (original&4))return kIOReturnBusy;
    pci->ConfigurationWrite16(4,original|2);
    uint16_t enabled=0;pci->ConfigurationRead16(4,&enabled);
    kern_return_t r=enabled==(original|2)?kIOReturnSuccess:kIOReturnIOError;
    auto &state=ivars->state;
    state.command=original;
    if(!r) {
        state.videoDma=read(0x31c040);state.deviceControl=read(0x200034);
        state.videoStatus=read(0x200054);state.pciStatus=read(0x200044);
        // DEV_CNTRL2 is write-only (datasheet p6-47); its readback is undefined.
        if(state.videoDma ||
           read(0x200040) || read(0x200050))r=kIOReturnBusy;
    }
    if(!r && selector==cxadc::configure) {
        cxadc::Settings settings;
        auto input=args->scalarInput;
        if(!input || input[0]>3 || input[1]>31 || input[2]>1 || input[3]>255)r=kIOReturnBadArgument;
        else if(state.quarantined)r=kIOReturnNotReady;
        else {
            settings={uint32_t(input[0]),uint32_t(input[1]),uint32_t(input[2]),uint32_t(input[3])};
            cxadc::Write values[19];unsigned count=cxadc::rawSettings(settings,values);
            state.initialized=0;state.registerMismatch=0;
            for(unsigned i=0;i<count;++i) {
                write(values[i].offset,values[i].value);
                auto observed=read(values[i].offset);
                os_log(OS_LOG_DEFAULT,"CXADC: static ADC [%06x]=%08x read=%08x",values[i].offset,values[i].value,observed);
                if((observed&values[i].mask)!=(values[i].value&values[i].mask)) {
                    state.registerMismatch=values[i].offset;state.expected=values[i].value;state.observed=observed;
                    r=kIOReturnIOError;break;
                }
            }
            if(!r){state.settings=settings;state.initialized=1;}
        }
    }
    if(!r && selector==cxadc::dmaCheck) {
        if(state.quarantined)r=kIOReturnNotReady;
        else {
            IODispatchQueue *queue=nullptr;IOInterruptDispatchSource *source=nullptr;
            r=CopyDispatchQueue(kIOServiceDefaultQueueName,&queue);
            if(!r) {
                state.irqCreate=IOInterruptDispatchSource::Create(pci,0,queue,&source);
                if(source){source->Cancel(nullptr);source->release();}
                queue->release();
            }
            uint64_t irqType=0;IOInterruptDispatchSource::GetInterruptType(pci,0,&irqType);state.irqType=uint32_t(irqType);
            DmaMemory samples,program,staging;
            if(!r)r=samples.allocate(pci,64ull*1024*1024);
            if(!r)r=program.allocate(pci,272*1024);
            if(!r)r=staging.allocate(pci,16384);
            if(!r && program.count!=1)r=kIOReturnUnsupported;
            if(!r) {
                state.sampleIOVA=samples.segments[0].address;state.programIOVA=program.segments[0].address;
                state.mapCount=samples.count;state.mappedBytes=samples.bytes;
                IOAddressSegment cpu{};r=staging.memory->GetAddressRange(&cpu);
                if(!r && (!cpu.address || cpu.length<16384))r=kIOReturnVMError;
                if(!r) {
                    auto bytes=reinterpret_cast<uint8_t*>(cpu.address);
                    for(unsigned i=0;i<16384;++i)bytes[i]=uint8_t((i*13+(i>>8)*7)^0x5a);
                    r=samples.dma->PerformOperation(kIODMACommandPerformOperationOptionWrite,0,16384,0,staging.memory);
                    if(!r) {
                        memset(bytes,0xcc,16384);
                        r=samples.dma->PerformOperation(kIODMACommandPerformOperationOptionRead,0,16384,0,staging.memory);
                        for(unsigned i=0;!r && i<16384;++i)if(bytes[i]!=uint8_t((i*13+(i>>8)*7)^0x5a))r=kIOReturnIOError;
                    }
                }
            }
            auto a=samples.release(),b=program.release(),c=staging.release();
            if(a || b || c){state.quarantined=1;if(!r)r=a?a:b?b:c;}
            state.mappingResult=r;
            os_log(OS_LOG_DEFAULT,"CXADC: DMA memory test result=%08x sampleIOVA=%llx programIOVA=%llx segments=%u IRQ-create=%08x (bus mastering OFF)",r,state.sampleIOVA,state.programIOVA,state.mapCount,state.irqCreate);
        }
    }
    pci->ConfigurationWrite16(4,original);
    uint16_t restored=0xffff;pci->ConfigurationRead16(4,&restored);
    state.restoredCommand=restored;
    if(restored!=original){state.quarantined=1;r=kIOReturnIOError;}
    if(output) {
        args->structureOutput=OSData::withBytes(&state,sizeof(state));
        if(!args->structureOutput)return kIOReturnNoMemory;
    }
    return r;
}
