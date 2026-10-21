// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace cxadc {
inline constexpr uint32_t dmaPage=16384, cluster=2048, chunkBytes=262144;
struct Segment { uint64_t address,length; };
// CX23880 WRITE is two words (not CX23885's three-word 64-bit variant).
// Explicit 16 KiB accounting pages, independent of the host's PAGE_SIZE macro.
inline bool buildRisc(const Segment *segments,unsigned count,uint64_t ringBytes,uint64_t program,
                      uint32_t *words,size_t capacity,size_t &used) {
    used=0;
    if(!segments || !count || count>32 || !words || ringBytes<chunkBytes*4 ||
       ringBytes%chunkBytes || ringBytes/dmaPage>65535 || program>UINT32_MAX ||
       (program&3))return false;
    const uint64_t required=1+ringBytes/cluster*2+2;
    if(required>capacity || required*4-1>UINT32_MAX-program)return false;
    for(unsigned i=0;i<count;++i)if(!segments[i].length || segments[i].address>UINT32_MAX ||
       segments[i].length-1>UINT32_MAX-segments[i].address || (segments[i].address&(cluster-1)) ||
       (segments[i].length&(cluster-1)))return false;
    words[used++]=0x80030000; // SYNC | CNT_RESET
    unsigned segment=0;uint64_t within=0;
    for(uint64_t offset=0;offset<ringBytes;offset+=cluster) {
        if(segment>=count || within+cluster>segments[segment].length)return false;
        uint32_t op=0x1c000800; // WRITE | SOL | EOL | 2048
        if((offset+cluster)%dmaPage==0)op|=offset+cluster==ringBytes?0x30000:0x10000;
        if((offset+cluster)%chunkBytes==0)op|=0x01000000; // IRQ1
        words[used++]=op;
        words[used++]=uint32_t(segments[segment].address+within);
        within+=cluster;
        if(within==segments[segment].length){++segment;within=0;}
    }
    words[used++]=0x70000000; // JUMP first WRITE; do not repeat startup SYNC
    words[used++]=uint32_t(program+4);
    return true;
}
// Modulo progression is meaningful only with an independent bounded observation
// interval. The runtime rejects observations separated by >= half a ring time.
inline bool advance(uint32_t previous,uint32_t current,uint32_t pages,uint32_t &delta) {
    if(!pages || previous>=pages || current>=pages)return false;
    delta=(current+pages-previous)%pages;
    return delta<=pages/2;
}
}
