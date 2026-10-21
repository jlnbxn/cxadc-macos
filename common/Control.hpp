// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
namespace cxadc {
inline constexpr uint32_t controlVersion=5;
inline constexpr uint32_t driverBuild=8;
enum Selector : uint64_t { status=0, configure=1, dmaCheck=2, startCapture=3, pull=4, stopCapture=5 };
struct Settings { uint32_t vmux=1, level=0, sixdb=0, centerOffset=8, agcEnable=1; };
inline bool valid(Settings s) { return s.vmux<4 && s.level<32 && s.sixdb<2 && s.centerOffset<256 && s.agcEnable<2; }
struct StreamStats {
    uint64_t interrupts=0,completedPages=0,deliveredBytes=0,wraps=0;
    uint64_t startedNS=0,stoppedNS=0;
    uint32_t error=0,videoStatus=0,pciStatus=0,lastCounter=0;
    uint32_t ringBytes=0,running=0,stopSafe=0,commandRestored=0;
    uint32_t overruns=0,transferStatus=0,programWords=0,reserved=0;
};
struct Status {
    uint32_t version=controlVersion, build=driverBuild, initialized=0, quarantined=0, command=0;
    Settings settings{};
    uint32_t videoDma=0, deviceControl=0, videoStatus=0, pciStatus=0;
    uint32_t irqCreate=0, irqType=0, mappingResult=0, mapCount=0;
    uint64_t sampleIOVA=0, programIOVA=0, mappedBytes=0;
    uint32_t registerMismatch=0, expected=0, observed=0, restoredCommand=0;
    StreamStats stream{};
};
}
