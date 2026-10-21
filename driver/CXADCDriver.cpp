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
#include <DriverKit/IODispatchQueue.h>

struct CXADCDriver_IVars { IOPCIDevice *pci = nullptr; bool open = false; uint8_t index=0; cxadc::Status state{}; };
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
    result = RegisterService();
    if (result) { pci->Close(this, 0); ivars->open = false; }
    os_log(OS_LOG_DEFAULT, "CXADC: attachment probe finished, mmio-read=%u valid=%u; engines remain off", probe.mmioRead, probe.mmioValid);
    return result;
}
kern_return_t IMPL(CXADCDriver, Stop) {
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
kern_return_t CXADCDriver::ClientClosed(IOUserClient *) { return kIOReturnSuccess; }
kern_return_t CXADCDriver::Command(uint64_t selector,IOUserClientMethodArguments *args,IOUserClient *) {
    if(!args || !ivars->open)return kIOReturnNotReady;
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
