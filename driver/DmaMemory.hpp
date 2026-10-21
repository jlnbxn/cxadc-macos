// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <DriverKit/IODMACommand.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
// Adapted from hvr1200-macos DmaBuffer. No automatic cleanup after device I/O:
// the capture owner must first establish quiescence, then call release().
struct DmaMemory {
    IOBufferMemoryDescriptor *memory=nullptr;
    IODMACommand *dma=nullptr;
    IOAddressSegment segments[32]{};
    uint32_t count=0;
    uint64_t bytes=0;
    bool prepared=false;
    DmaMemory()=default;
    DmaMemory(const DmaMemory&)=delete;
    DmaMemory& operator=(const DmaMemory&)=delete;
    kern_return_t allocate(IOService *device,uint64_t size) {
        if(memory || !size)return kIOReturnBadArgument;
        bytes=size;
        auto r=IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOutIn,size,16384,&memory);
        if(r)return r;
        r=memory->SetLength(size);if(r)return r;
        IODMACommandSpecification spec{};spec.maxAddressBits=32;
        r=IODMACommand::Create(device,0,&spec,&dma);if(r)return r;
        uint64_t flags=0;count=32;
        r=dma->PrepareForDMA(0,memory,0,size,&flags,&count,segments);if(r)return r;
        prepared=true;
        if(!count || count>32 || (flags&kIOMemoryDirectionOutIn)!=kIOMemoryDirectionOutIn)return kIOReturnUnsupported;
        uint64_t total=0;
        for(unsigned i=0;i<count;++i) {
            auto s=segments[i];
            if(!s.length || s.address>UINT32_MAX || s.length-1>UINT32_MAX-s.address ||
               (s.address&2047) || (s.length&2047) || s.length>size-total)return kIOReturnUnsupported;
            total+=s.length;
        }
        return total==size?kIOReturnSuccess:kIOReturnUnsupported;
    }
    kern_return_t release() {
        if(prepared) { auto r=dma->CompleteDMA(0);if(r)return r;prepared=false; }
        if(dma){dma->release();dma=nullptr;}
        if(memory){memory->release();memory=nullptr;}
        return kIOReturnSuccess;
    }
};
