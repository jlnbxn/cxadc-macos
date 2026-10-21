// SPDX-License-Identifier: GPL-2.0-or-later
#include <DriverKit/IOLib.h>
#include <DriverKit/IODispatchQueue.h>
#include "CXADCUserClient.h"
#include "CXADCDriver.h"
struct CXADCUserClient_IVars { CXADCDriver *driver=nullptr; };
bool CXADCUserClient::init(){if(!super::init())return false;ivars=new CXADCUserClient_IVars;return ivars!=nullptr;}
void CXADCUserClient::free(){if(ivars){if(ivars->driver)ivars->driver->release();delete ivars;}super::free();}
kern_return_t IMPL(CXADCUserClient, Start){
    auto r=Start(provider,SUPERDISPATCH);if(r)return r;
    auto driver=OSDynamicCast(CXADCDriver,provider);if(!driver)return kIOReturnBadArgument;
    driver->retain();ivars->driver=driver;
    IODispatchQueue *queue=nullptr;r=driver->CopyDispatchQueue(kIOServiceDefaultQueueName,&queue);
    if(r)return r;
    r=SetDispatchQueue(kIOServiceDefaultQueueName,queue);
    if(!r)r=SetDispatchQueue(kIOUserClientQueueNameExternalMethod,queue);queue->release();return r;
}
kern_return_t IMPL(CXADCUserClient, Stop){if(ivars->driver)ivars->driver->ClientClosed(this);return Stop(provider,SUPERDISPATCH);}
kern_return_t CXADCUserClient::ExternalMethod(uint64_t selector,IOUserClientMethodArguments *arguments,
 const IOUserClientMethodDispatch *,OSObject *,void *){
    if(!arguments || !ivars->driver)return kIOReturnBadArgument;
    return ivars->driver->Command(selector,arguments,this);
}
