// SPDX-License-Identifier: GPL-2.0-or-later
// Native user-space adapter. The existing driver RPCs/DMA implementation are unchanged.
#include "cxadc_client.h"
#include "../common/Control.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach/mach.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>

struct cxadc_client {
    io_connect_t connection=0;
    std::mutex mutex;
    bool started=false;
    IOPMAssertionID awake=kIOPMNullAssertionID;
};
static thread_local char message[256];
static int32_t failure(IOReturn r,const char *operation) {
    std::snprintf(message,sizeof(message),"%s: 0x%08x%s",operation,r,
        r==kIOReturnBusy?" (card busy; stop the other capture first)":
        r==kIOReturnNotPermitted?" (check user-client entitlement/signing or another capture owner)":
        r==kIOReturnNotPrivileged?" (check executable signing and DriverKit user-client entitlement)":"");
    return int32_t(r);
}
static uint32_t property(io_registry_entry_t device,CFStringRef key) {
    auto value=IORegistryEntryCreateCFProperty(device,key,kCFAllocatorDefault,0);
    uint32_t result=UINT32_MAX;
    if(value && CFGetTypeID(value)==CFDataGetTypeID() && CFDataGetLength((CFDataRef)value)==4)
        std::memcpy(&result,CFDataGetBytePtr((CFDataRef)value),4);
    if(value)CFRelease(value);
    return result;
}
static io_service_t serviceFor(uint64_t id) {
    auto match=IOServiceMatching("IOUserService");
    const void *keys[]={CFSTR("IOUserClass")},*values[]={CFSTR("CXADCDriver")};
    auto props=CFDictionaryCreate(nullptr,keys,values,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(match,CFSTR("IOPropertyMatch"),props);CFRelease(props);
    io_iterator_t iterator=0;
    if(IOServiceGetMatchingServices(kIOMainPortDefault,match,&iterator))return 0;
    io_service_t found=0,service;
    while((service=IOIteratorNext(iterator))) {
        io_registry_entry_t parent=0;uint64_t parentID=0;
        if(!IORegistryEntryGetParentEntry(service,kIOServicePlane,&parent)) {
            IORegistryEntryGetRegistryEntryID(parent,&parentID);IOObjectRelease(parent);
        }
        if(parentID==id){found=service;break;}
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);return found;
}
static int32_t enumerate(cxadc_client_device *out,size_t capacity,size_t *count) {
    if(!count || (capacity && !out))return failure(kIOReturnBadArgument,"enumerate arguments");
    *count=0;io_iterator_t iterator=0;
    auto r=IOServiceGetMatchingServices(kIOMainPortDefault,IOServiceMatching("IOPCIDevice"),&iterator);
    if(r)return failure(r,"PCI enumeration");
    io_service_t device;
    while((device=IOIteratorNext(iterator))) {
        if(property(device,CFSTR("vendor-id"))==0x14f1 && property(device,CFSTR("device-id"))==0x8800) {
            cxadc_client_device info{};
            IORegistryEntryGetRegistryEntryID(device,&info.registry_id);
            info.vendor=0x14f1;info.device=0x8800;
            info.subsystem_vendor=property(device,CFSTR("subsystem-vendor-id"));
            info.subsystem_device=property(device,CFSTR("subsystem-id"));info.revision=property(device,CFSTR("revision-id"));
            auto service=serviceFor(info.registry_id);info.driver_attached=service!=0;if(service)IOObjectRelease(service);
            std::snprintf(info.name,sizeof(info.name),"CX23880 14f1:8800 [%04x:%04x]%s",info.subsystem_vendor,
                info.subsystem_device,info.driver_attached?" macOS":" (driver not attached)");
            if(*count<capacity)out[*count]=info;
            ++*count;
        }
        IOObjectRelease(device);
    }
    IOObjectRelease(iterator);return 0;
}
static IOReturn snapshot(cxadc_client *client,cxadc::Status &s) {
    size_t size=sizeof(s);
    auto r=IOConnectCallStructMethod(client->connection,cxadc::status,nullptr,0,&s,&size);
    if(!r && (size!=sizeof(s) || s.version!=cxadc::controlVersion))r=kIOReturnUnsupported;
    return r;
}
static int32_t openClient(uint64_t id,cxadc_client **out) {
    if(!out)return failure(kIOReturnBadArgument,"open arguments");*out=nullptr;
    auto service=serviceFor(id);
    if(!service)return failure(kIOReturnNotFound,"CXADC driver not attached; install/approve extension or reconnect enclosure");
    auto client=new(std::nothrow) cxadc_client;
    if(!client){IOObjectRelease(service);return failure(kIOReturnNoMemory,"allocate client");}
    auto r=IOServiceOpen(service,mach_task_self(),0,&client->connection);IOObjectRelease(service);
    cxadc::Status s;if(!r)r=snapshot(client,s);
    if(r){if(client->connection)IOServiceClose(client->connection);delete client;return failure(r,"open DriverKit client (API5 required)");}
    *out=client;return 0;
}
static int32_t status(cxadc_client *c,cxadc_client_state *out) {
    if(!c || !out)return failure(kIOReturnBadArgument,"status arguments");
    std::lock_guard lock(c->mutex);cxadc::Status s;auto r=snapshot(c,s);
    if(r)return failure(r,"driver status");
    *out={s.version,s.build,28636363,s.stream.running,s.stream.error,s.stream.overruns,
        s.stream.stopSafe,s.quarantined,s.command,s.restoredCommand,
        {s.settings.vmux,s.settings.level,s.settings.sixdb,s.settings.centerOffset,s.settings.agcEnable},
        s.stream.deliveredBytes,s.stream.interrupts,s.stream.wraps};return 0;
}
static int32_t configure(cxadc_client *c,const cxadc_client_settings *s) {
    if(!c || !s || !cxadc::valid({s->vmux,s->level,s->sixdb,s->center_offset,s->agc_enable}))
        return failure(kIOReturnBadArgument,"ADC settings");
    std::lock_guard lock(c->mutex);
    uint64_t inputs[]={s->vmux,s->level,s->sixdb,s->center_offset,s->agc_enable};
    auto r=IOConnectCallScalarMethod(c->connection,cxadc::configure,inputs,5,nullptr,nullptr);
    return r?failure(r,"configure ADC (stop capture before changing settings)"):0;
}
static int32_t start(cxadc_client *c) {
    if(!c)return failure(kIOReturnBadArgument,"start arguments");std::lock_guard lock(c->mutex);
    if(c->started)return failure(kIOReturnBusy,"client already capturing");
    auto r=IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep,kIOPMAssertionLevelOn,
        CFSTR("CXADC graphical raw capture"),&c->awake);
    uint64_t ring=64;if(!r)r=IOConnectCallScalarMethod(c->connection,cxadc::startCapture,&ring,1,nullptr,nullptr);
    if(r){if(c->awake!=kIOPMNullAssertionID)IOPMAssertionRelease(c->awake);c->awake=kIOPMNullAssertionID;return failure(r,"start capture");}
    c->started=true;return 0;
}
static int32_t readSamples(cxadc_client *c,void *data,size_t capacity,size_t *bytes) {
    if(!c || !data || !bytes || capacity<CXADC_CLIENT_CHUNK_BYTES)return failure(kIOReturnBadArgument,"read buffer");
    std::lock_guard lock(c->mutex);*bytes=CXADC_CLIENT_CHUNK_BYTES;
    if(!c->started){*bytes=0;return failure(kIOReturnNotOpen,"read before start");}
    auto r=IOConnectCallStructMethod(c->connection,cxadc::pull,nullptr,0,data,bytes);
    if(r==kIOReturnNotReady){*bytes=0;return 0;}
    if(!r && *bytes!=CXADC_CLIENT_CHUNK_BYTES)r=kIOReturnUnderrun;
    if(r){*bytes=0;return failure(r,"read ADC samples");}return 0;
}
static int32_t stop(cxadc_client *c) {
    if(!c)return failure(kIOReturnBadArgument,"stop arguments");std::lock_guard lock(c->mutex);
    if(!c->started)return 0;
    auto r=IOConnectCallScalarMethod(c->connection,cxadc::stopCapture,nullptr,0,nullptr,nullptr);
    cxadc::Status s;auto sr=snapshot(c,s);if(!r)r=sr;
    if(!r && (s.stream.running || !s.stream.stopSafe || s.quarantined || s.stream.error))r=kIOReturnIOError;
    if(c->awake!=kIOPMNullAssertionID)IOPMAssertionRelease(c->awake);c->awake=kIOPMNullAssertionID;
    c->started=false;return r?failure(r,"capture stopped with hardware/teardown error"):0;
}
static void closeClient(cxadc_client *c) { if(c){stop(c);IOServiceClose(c->connection);delete c;} }
static const char *error() { return message; }
extern "C" const cxadc_client_api *cxadc_client_get_api_v1() {
    static const cxadc_client_api api={CXADC_CLIENT_ABI,sizeof(cxadc_client_api),enumerate,openClient,closeClient,status,configure,start,readSamples,stop,error};
    return &api;
}
