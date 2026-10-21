// SPDX-License-Identifier: GPL-2.0-or-later
#include "../core/Risc.hpp"
#include "../core/Raw.hpp"
#include <cassert>
#include <vector>
int main() {
    constexpr uint64_t bytes=8*1024*1024;
    std::vector<uint32_t> words(bytes/2048*2+3);
    cxadc::Segment sg[]={{0x80000000,bytes/2},{0x90000000,bytes/2}};
    size_t used=0;
    assert(cxadc::buildRisc(sg,2,bytes,0x84000000,words.data(),words.size(),used));
    assert(used==words.size() && words[0]==0x80030000 && words.back()==0x84000004);
    unsigned irqs=0,incs=0,resets=0;
    for(size_t i=1;i<used-2;i+=2) {
        auto op=words[i];assert((op&0xf000ffff)==0x10000800 && (op&0x0c000000)==0x0c000000);
        irqs+=(op&0x01000000)!=0;incs+=(op&0x30000)==0x10000;resets+=(op&0x30000)==0x30000;
        auto index=(i-1)/2;auto expected=index*2048<bytes/2?0x80000000+index*2048:0x90000000+index*2048-bytes/2;
        assert(words[i+1]==expected);
    }
    assert(irqs==32 && incs==511 && resets==1);
    sg[1].address=0x100000000;assert(!cxadc::buildRisc(sg,2,bytes,0x84000000,words.data(),words.size(),used));
    sg[1].address=0xfffff800;assert(!cxadc::buildRisc(sg,2,bytes,0x84000000,words.data(),words.size(),used));
    sg[1].address=0x90000001;assert(!cxadc::buildRisc(sg,2,bytes,0x84000000,words.data(),words.size(),used));
    uint32_t delta=0;assert(cxadc::advance(496,16,512,delta)&&delta==32);
    assert(!cxadc::advance(16,496,512,delta));
    cxadc::Write settings[19];assert(cxadc::rawSettings({1,0,0,8},settings)==18);
    assert(settings[0].value==0x16011 && settings[1].mask==0xfffffffe);
    assert(!cxadc::rawSettings({4,0,0,8},settings));
}
