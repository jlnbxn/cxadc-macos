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
10 s using VMUX1/level 0/sixdb 0/offset 8/AGC 1 without driver changes. VHS-Decode
PAL VHS at nominal 28.636363MHz decoded 48 and 498 fields respectively. Exported
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

C31 comparison setup: user reports C31 removed and card reattached. Actual
read-only PCI/driver checks passed on the same Conexant endpoint; v8/API5 is
idle, mastering/DMA off, no quarantine, fresh uninitialized ADC state. No
post-modification data yet and no improvement claim.85 GiB remains available.
Next: user cues the same S-VHS section, then capture60 s with unchanged settings
and decode using `SVHS --use_saved_levels`; no MP4 requested.

Post-C31 comparison completed: same v8 configuration captured another genuine
60 s RF file, 1,718,181,780 bytes, 25 wraps, no reported DMA/overrun errors, verified
stop and PCI command 0000. S-VHS level-reuse decode produced 2,994 fields and normal
59.88 s colour FFV1 output. Startup excluded ~0.114 s; no later decoder errors.
Matched scenes align by 4.44 s, covering 55.44 s common content. Dropout segments
318→295, bPSNR diagnostic 39.7→38.6, ADC span 97..120 unchanged: mixed evidence,
not a demonstrated major quality gain or identified cause of rare tears.
Report: docs/C31-COMPARISON.md. No MP4, no driver/CX23885 edits, 79 GiB free.

RF gain milestone: six genuine five-second captures at levels 0/4/8/16/24/31
passed DMA, byte/hash and teardown checks with boost off. ADC span 20→146 codes,
level 31 range 36..182, no clipping in the sample. The user monitored SCART and
reported no visible capture/gain-related disturbance for all tested steps.
Levels 0/16/31 each decoded 248 PAL S-VHS fields without synchronization errors
and exported recognizable colour video. Successive scenes differ, so noise and
quality improvements are not yet scene-matched or established. Level 31/sixdb 0
is the next longer-trial candidate; code defaults and idle state remain level 0.
See docs/RF-GAIN-SWEEP.md. No driver/CX23885 edits, physical changes or MP4.
Remaining: matched longer comparison, representative-tape clipping checks,
archival-duration reliability and separate RF-loading/buffer assessment.

Specific horizontal-streak diagnosis: matched the user's screenshot at 25.12 s
in the first decoded minute. The same tape moment at 29.56 s post-C31 shows
the same defect and matching RF-dropout coordinates. Standard dropout-corrected
export visibly conceals the streak, whereas our diagnostic exports had explicitly
disabled correction. Full corrected FFV1 minute retained; original raw unchanged.
This instance strongly suggests a repeatable tape/playback dropout rather than
random wiring/DMA corruption. Other artifacts remain separate; amplifier need
is not established by this screenshot. No new hardware actions or MP4.
Report: docs/DROPOUT-INVESTIGATION.md.

First full-tape preparation: 44-minute user estimate needs 75.6 GB raw plus 187.6 GB
luma/chroma TBCs and ~40 GB corrected FFV1 if all retained (~303 GB nominal).
User selected staged decode/export with verified generated-intermediate cleanup;
raw/final files and metadata are retained. No cleanup yet; staged boundary
handling remains to be implemented/verified.
T7 APFS folder prepared at /Volumes/T7/CXADC/2026-10-21-tape-01. Small temporary
write/fsync preflight passed (~774 MB/s); not a sustained or real capture test.
T7 free space increased during checks from 171.6 GB to ~323 GB; no user-file
deletion by agent. Unstarted 45-minute plan saved separately (77.32 GB raw,
VMUX1/level 31/sixdb0/offset8/AGC1). Driver idle and default remains level 0.
No full-tape capture started. Current acquisition is video RF only.
Report: docs/FULL-TAPE-STORAGE.md.

T7 real RF preflight passed: 60 seconds at level31/sixdb0, 1,718,181,780 bytes,
codes 36..181 with no rail samples, 6,554 unique blocks, no reported DMA/overrun
or disk errors. Verified shutdown/command restoration, restored idle level0.
Five-second PAL S-VHS excerpt decoded to 250 fields without duplicate flags,
parity errors or logged ERROR/WARNING. Raw and analysis retained on T7.
Level31 now verified for this minute; remaining tape headroom and 45-minute
stability remain unverified. Full-tape capture has not started.

Full-tape capture milestone: 2,700 seconds completed on T7, exactly77,318,180,100
bytes, 294,947 IRQ callbacks/1,152 wraps, no reported DMA/overrun/write errors,
verified shutdown and PCI command restoration. Idle level0 restored. Raw remains
intact; no authoritative zero-loss claim. Selected ADC windows across tape show
no rail clipping, but the entire raw file has not yet been analyzed/checksummed.

User authorized full decode if space permits. With 244.17 GB free on T7 and
81.03 GB internally, full TBCs are being written to T7 and final standard-DOC
FFV1 will be written internally. PAL/SVHS saved-level decode is running, locked
after the pre-play lead-in. New disk-guarded runner automatically validates TBC
byte counts then exports; full workflow has not yet completed/been verified.
No generated TBCs or raw files deleted. See TESTING.md and docs/FULL-TAPE-STORAGE.md.

Full decode completed: 134,826 fields/44:56.52, both TBCs95,795,221,260 bytes,
matching field counts and alternating parity. Automatic export initially stopped
on four seqNo mismatches caused by two pinned-decoder replacement-field events.
Confirmed exact old-metadata reuse in the source and actual JSON; no data edited.
Validation now recognizes only that exact pattern, with five focused tests.
User requested export; resumed using `--export-existing`, standard DOC/FFV1,
internal output and disk guard. Export currently running; final checks pending.
Raw/TBCs and original failed workflow record retained.

Full DOC export completed: 44:56.52, 67,413 frames, 40.067 GB lossless FFV1,
928×576/25 fps/yuv422p10le/top-field-first. Duration matches decoded field count;
exporter reports 22,637 concealments. Opening/middle/late programme stills show
recognizable colour images; tail still is noise after the recorded programme.
Raw and TBCs retained, no audio or MP4. Roughly 38 GiB internal/49 GiB T7 free.
Full-file FFmpeg video decode completed with exit0 and no logged errors.
Eight Python diagnostic tests passed. One real full-tape RF→VHS-Decode→DOC FFV1
workflow is now verified; no universal archival reliability or zero-loss claim.
