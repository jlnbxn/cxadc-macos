# Sources and licensing

Reference clones are separate ignored Git repositories under `references/`.
Do not edit them. Clone acquisition date: 2026-10-21.

| Source | Exact revision | Review |
| --- | --- | --- |
| https://github.com/happycube/cxadc-linux3 | `68b4ddbc072301f68d376637534a039ce675f6ce` | cxadc.c and cx88-reg.h |
| https://github.com/JuniorIsAJitterbug/cxadc-win | `bbfc930bc5caa6bed9441974caec6c9ec062b8af` | driver/cx2388x.c, cxadc_win.c |
| https://github.com/oyvindln/vhs-decode | clone still fetching | pending |
| https://github.com/happycube/cxadc-linux3.wiki.git | clone still fetching | home page inspected over web |
| Local hvr1200-macos | `a4e28509264881b4d9b67ba285a0dcbb400265a4` | DriverKit lifecycle, build, DMA and panic notes |

The Linux source declares GPL-2.0-or-later, copyright Hew How Chee (2005–2007),
Chad Page (2013–2015), Adam Sampson (2019–2024), Tony Anderson (2020–2022).
Its register header carries its own authorship notices. Windows driver source
declares GPL-2.0-or-later; the capture server has separate GPL-3.0 terms and the
clock generator separate licenses. VHS-Decode identifies as GPL-3.0; no decoder
code is incorporated into this driver. Local HVR source declares GPL-2.0-or-later.
The DriverKit project, host and lifecycle pattern were adapted from that local
source; register names/offsets are transcribed from the Linux header. LICENSE
contains the GPL v2 text, reproduced unchanged from the Windows repository.
No Windows driver or VHS-Decode source is copied into this project's targets.

Linux source findings:

- Match vendor/device `14f1:8800`, any subsystem. macOS initially narrows this to
  the observed subsystem to avoid affecting unrelated boards.
- BAR0 is MMIO, offsets are bytes; Linux's uint32_t pointer arithmetic shifts by 2.
- Probe disables DMA/IRQ masks, constructs SRAM CDT and channel 24 commands, then
  starts RISC/video DMA before a character-device open. This ordering is unsuitable
  for our diagnostic-only attach and is deliberately not reproduced.
- The inspected Linux probe does not perform a general chip reset. AGC reset on
  removal restores a named register subset; do not invent a global reset sequence.
- Raw path uses eight 2048-byte SRAM clusters, channel 24 VBI DMA, 64 MiB host ring,
  RISC SYNC/counter reset, WRITE SOL/EOL, page counter increment/reset, IRQ1 every
  2 MiB, then JUMP to program+4. Instructions have single 32-bit address operands.
- Raw setup: OUTPUT_FORMAT `0x0f`, CONTR_BRIGHT `0xff00`, VBI_PACKET
  `(2048<<17)|(2<<11)`, COLOR_CTRL `0xee` (no byte swap), CAPTURE_CTRL `0x46`
  for 8-bit, AFECFG_IO `0x12`. These are upstream settings, not yet applied here.
- INPUT_FORMAT is `(vmux<<14)|(1<<13)|0x01|0x10|0x10000`, vmux 0..3.
  PAL-labelled control bits are part of upstream raw setup; they do not authorize
  decoding PAL video in this application. Connector-to-VMUX mapping is unverified.
- Gain: level 0..31 in bits 16..20, sixdb in bit 23; ADJ4 also sets `0xff<<8`.
  SYNC_TIP3 is `(0x1e48<<16)|(0xff<<8)|center_offset`, upstream default offset 8.
  Default level 16, boost off. Exact electrical gain/calibration still needs testing.
- Stock-clock mode writes SCONV=131072 and PLL=`0x11000000`. No oscillator frequency
  is measured by those writes. Other PLL modes and ten-bit packing are outside scope.
- Video interrupt mask `0x18888`, PCI video mask bit 0; status acknowledgement is
  write-one-to-clear in the reference interrupt handling. VBI completion bit `0x8`.
- Critically, upstream says GPCNT can advance before the corresponding memory is
  resident. It reads the counter after IRQ and rounds down to the IRQ period.
  Modulo progress alone cannot establish an authoritative dropped-sample count.
- Shutdown masks IRQ, clears video status, disables video FIFO/RISC and device RISC.
  Linux cleanup is not proof of safe teardown through Apple Silicon DART; macOS must
  verify quiescence before releasing mappings. Upstream error paths and lifetime
  assumptions should not be copied blindly.

Windows independently follows the same channel 24/CDT/SYNC-WRITE-JUMP scheme,
32-bit WDF packet DMA profile and common-buffer logical addresses. Its page-index
formula includes a factor of two tied to 4 KiB pages. Its ISR queues a DPC, which
uses the same IRQ-period-rounded counter and carries the same memory-visibility
warning. WDF, Windows events, logical-address APIs and bitfield layouts are not
portable macOS implementations.

Installed API evidence: Xcode DriverKit27.0.sdk under `System/DriverKit` contains
PCIDriverKit/IOPCIDevice.h (`Open`, `GetBARInfo`, config and memory access),
DriverKit/IODMACommand.h (`maxAddressBits`, PrepareForDMA, CompleteDMA,
PerformOperation; at most 32 returned segments), IOInterruptDispatchSource.h
(`GetInterruptType`, Create, enable/cancel) and memory descriptor interfaces.
The prepared DMA mapping must outlive the I/O; no bounce buffering is promised.

Primary Apple references:

- https://developer.apple.com/documentation/pcidriverkit/iopcidevice
- https://developer.apple.com/documentation/driverkit/requesting-entitlements-for-driverkit-development
- https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.driverkit.transport.pci

The CXADC wiki home describes nominal stock-clock capture at 28.6 MSPS. Its advice
is background; verified Linux source defines the register sequence, and this card's
clock/wiring still need measurement. No wiki prose or images are redistributed here.
