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

struct CXADCDriver_IVars { IOPCIDevice *pci = nullptr; bool open = false; };
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
    // Stage A is strictly read-only: no memory-decode enable, reset, status
    // acknowledgement, DMA mappings, interrupt enable or bus-master writes.
    if (command != 0xffff && (command & 2) && !(command & 4) &&
        !probe.barStatus && !(type & 1) && probe.barSize >= 0x31c044) {
        probe.mmioRead = 1; probe.mmioValid = 1;
        for (unsigned i = 0; i < cxadc::registerCount; ++i) {
            pci->MemoryRead32(index, cxadc::registers[i].offset, &probe.values[i]);
            os_log(OS_LOG_DEFAULT, "CXADC: BAR0[%06x]=%08x", cxadc::registers[i].offset, probe.values[i]);
            if (probe.values[i] == 0xffffffff) { probe.mmioValid = 0; break; }
        }
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
    pci->retain(); ivars->pci = pci; ivars->open = true;
    result = RegisterService();
    if (result) { pci->Close(this, 0); ivars->open = false; }
    os_log(OS_LOG_DEFAULT, "CXADC: read-only probe finished, mmio-read=%u valid=%u; no DMA implemented", probe.mmioRead, probe.mmioValid);
    return result;
}
kern_return_t IMPL(CXADCDriver, Stop) {
    if (ivars->open) { ivars->pci->Close(this, 0); ivars->open = false; }
    return Stop(provider, SUPERDISPATCH);
}
