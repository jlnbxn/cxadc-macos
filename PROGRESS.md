# Progress

Branch: `feature/cx23880-cxadc-macos`. Workspace was empty and had no Git history.
No remote configured; no code published. The HVR-1200 repository at
`/Users/julianboxan/Developer/hvr1200-macos` was inspected read-only at
`a4e28509264881b4d9b67ba285a0dcbb400265a4` and remains unchanged.

Milestone 1: actual host/PCI discovery, isolated DriverKit target, native enumeration
CLI and local host build. Exact endpoint/bridge IDs and BAR ranges documented in
HARDWARE.md. Both native targets compile. Driver v1 attached and read PCI configuration/BAR successfully; MMIO correctly skipped with memory decoding off. V2 also verified bounded memory-decoding enable/read/restore and actual register access.
All four reference clones are complete, pinned and reviewed for the first milestone;
see references/manifest.json and docs/REFERENCES.md. Tracked upstream content is unchanged.

After Open, legacy interrupt routing is exposed and DriverKit reports level type;
at Stage A delivery was untested. Initial uncertainties: Linux requires more
than counter polling for trustworthy resident data. Safe 32-bit IOVA allocation,
coherency, engine cancellation and complete raw data integrity are untested.
These were Stage A limits; subsequent hardware results below supersede them.

Discovery/PCI attachment commit: `2d05277`. V2 register-probe result is a separate
follow-up milestone. No DMA enabled. V1 replacement initially left the old instance
attached; explicit deactivation detached it. macOS reported reboot-required for
retirement, but v2 activation/attachment subsequently succeeded without reboot.
No driver processes were killed and no security setting changed.

Stage B: native user client, static ADC configuration and private 32-bit DMA mapping
self-test implemented and verified on real hardware with mastering off. V4 passed
all 18 ADC readbacks after correcting reserved output bit 0. A 64 MiB mapping at
IOVA 80000000 and RISC mapping at 84004000 passed the supported DriverKit memory
lifecycle and CPU round-trip. Actual RISC capture/interrupt delivery remains next.
The manufacturer datasheet corrects Stage A: DEV_CNTRL2 is write-only; its readback
cannot establish RISC state. No physical modifications or CX23885 edits.


Stage B capture bring-up: v7 has now captured genuine ADC data at 1 s and 10 s
with no signal, then 5 s of real MiniDV PAL CVBS on RCA. Native arm64 CLI writes
u8 plus JSON, configurable VMUX/level/sixdb/offset, bounded disk queue and real
interrupt completion. Tested ring wrap (3 in one second with 8 MiB), 32-bit IOVAs,
DMA guard and command restoration. No authoritative dropped-sample claim.
See TESTING.md for exact counts, failures and cold small-ring startup limitation.
CVBS results follow below; multi-minute reliability remains unverified.

Milestones committed: `1b78b14` ADC/readback plus 32-bit DriverKit mapping lifecycle;
`4ea0fd5` genuine raw DMA streaming, interrupts, CLI and verified termination.
Driver v8 adds optional documented ADC AGC gate control. With AGC disabled, real
RCA MiniDV PAL captures decode: 225 fields from 5 s and 498 fields from 10 s,
recognizable image visually inspected. No driver-side signal processing.
A real 60 s capture wrote 1,718,181,780 bytes with 25 observed wraps and no reported
DMA/overrun errors. No authoritative zero-loss claim. Temporary idle-sleep assertion
is hardware-tested. Current idle reference defaults restored; master off.
V8 sustained capture and real PAL decoding committed as `8a4d842`.
Normal colour export now also verified with official native TBC Tools v3.3.0:
249 frames, 928×576, 25 fps, 9.96 s, FFV1 MKV. Export previews at 1/5/9 s show
recognizable real colour video with artefacts. GUI TBC inspection is blocked by
denied computer-use access, explicitly unverified.
The next RF milestone is recorded below. DriverKit remains viable; no signing or DMA
mapping blocker is present for the tested development setup.

First real VHS RF milestone, 2026-10-21: user supplied a coax RF tap with a 68 nF
series capacitor; C31 intact. Idle reconnection verified. V8 captured 1 s then
10 s using VMUX1/level 0/sixdb 0/offset8/AGC1 without driver changes. VHS-Decode
PAL VHS at nominal28.636363MHz decoded 48 and 498 fields respectively. Exported
colour video was visually verified, including frames near 1/5/9 s in the 10 s
recording. The primary end-to-end path is demonstrated for a short real RF capture.
Signal spans only codes97..120, so this is not optimal ADC range or quality proof.
No physical or CX23885 changes, no security changes, no publication.
A subsequent 60 s RF stability capture completed; final diagnostics are recorded
in docs/RF-FIRST-CAPTURE.md. Remaining work is multi-minute/archival reliability,
remaining failure paths, loading assessment and signal/gain quality evaluation.

Second one-minute RF test: new tape section was confirmed by the user as PAL
S-VHS/S-VHS ET. Raw capture completed at the same conservative v8 settings,
1,718,181,780 bytes, 25 wraps, no reported DMA errors or overruns, verified stop
and PCI command 0000. Full-frame decoding must use the appropriate tape format.
Default S-VHS decoding acquired only at source7.712 s; the documented
`--use_saved_levels` option recovered near-start acquisition in a bounded probe.
Full S-VHS rerun with `--use_saved_levels` completed: 2,998 fields, no decoder
errors/warnings or duplicate flags, 1,499 frames/59.96 s in the normal FFV1 export.
Colour samples near 1/30/58 s visually verified. 364 decoder-detected dropout
segments remain, and the user reports rare tears despite improved stability.
Those tears have not been attributed to weak signal, C31 or DMA.
Results: docs/RF-ONE-MINUTE.md. MP4 creation cancelled at the user's request.
User plans C31 removal for a controlled comparison; no modification yet verified.
The SSD retains 85 GiB available. New read-only check_decode.py validates TBC byte
counts, sequence/parity and diagnostic flags on actual generated TBC files. Original raw files and upstream references unchanged.
