// SPDX-License-Identifier: GPL-2.0-or-later
// Linux-derived hardware portions adapted for macOS, 2026-10-21.
// Upstream copyrights/license and modification scope: THIRD_PARTY_NOTICES.md.
#pragma once
#include <stdint.h>
namespace cxadc {
// Offsets and names transcribed from cxadc-linux3/cx88-reg.h at
// 68b4ddbc072301f68d376637534a039ce675f6ce. No CX23885 register reuse.
struct Register { const char *name; uint32_t offset; };
inline constexpr Register registers[] = {
    {"MO_DEV_CNTRL2", 0x200034},
    {"MO_PCI_INTMSK", 0x200040},
    {"MO_PCI_INTSTAT", 0x200044},
    {"MO_VID_INTMSK", 0x200050},
    {"MO_VID_INTSTAT", 0x200054},
    {"MO_DEVICE_STATUS", 0x310100},
    {"MO_INPUT_FORMAT", 0x310104},
    {"MO_PLL_REG", 0x310168},
    {"MO_SCONV_REG", 0x310170},
    {"MO_AGC_SYNC_TIP3", 0x310210},
    {"MO_AGC_GAIN_ADJ4", 0x310220},
    {"MO_VBI_GPCNT", 0x31c02c},
    {"MO_VID_DMACNTRL", 0x31c040},
};
inline constexpr unsigned registerCount = sizeof(registers) / sizeof(registers[0]);
// Fixed-width, local arm64 registry snapshot, not a streaming ABI.
struct Probe {
    uint32_t version = 2;
    uint32_t primary = 0, subsystem = 0, command = 0;
    uint32_t bar0 = 0, interruptConfig = 0;
    uint32_t barStatus = 0, barIndex = 0, barType = 0;
    uint32_t interruptStatus = 0;
    uint64_t barSize = 0, interruptType = 0;
    uint32_t mmioRead = 0, mmioValid = 0;
    uint32_t values[registerCount]{};
    uint32_t commandAfter = 0;
};
static_assert(sizeof(Probe) == 120);
}
