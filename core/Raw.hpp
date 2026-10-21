// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../common/Control.hpp"
namespace cxadc {
struct Write { uint32_t offset, value, mask=UINT32_MAX; };
// From pinned cxadc.c probe/open, stock-clock/8-bit branch. No GPIO, I2C,
// decoder firmware, or DMA enable is part of static ADC configuration.
inline unsigned rawSettings(Settings s, Write (&out)[19]) {
    if (!valid(s)) return 0;
    const Write writes[] = {
        {0x310104,(s.vmux<<14)|(1<<13)|0x01|0x10|0x10000},
        // Datasheet 101069A p6-72: bit 0 is read-only reserved, always 0.
        {0x310164,0x0f,0xfffffffe}, {0x310110,0xff00},
        {0x310188,(2048u<<17)|(2<<11)},
        {0x310184,0xee}, {0x310180,0x46}, {0x35c04c,0x12},
        {0x310170,131072}, {0x310168,0x11000000},
        {0x310204,0}, {0x310200,(1<<25)|(0x100<<16)|0xfff},
        {0x310220,(s.sixdb<<23)|(s.level<<16)|(0xff<<8)},
        {0x310208,(0x1c0<<17)|0xf}, {0x31020c,(0x20<<17)|0xf},
        {0x310210,(0x1e48u<<16)|(0xff<<8)|s.centerOffset},
        {0x310214,(0xe0<<17)|(0xe<<9)|7},
        {0x310218,(0x20<<17)|(2<<7)|0xf},
        {0x31021c,(0x28<<16)|(0x28<<8)|0x50},
    };
    for (unsigned i=0;i<sizeof(writes)/sizeof(writes[0]);++i) out[i]=writes[i];
    return sizeof(writes)/sizeof(writes[0]);
}
}
