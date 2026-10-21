# Validation status

2026-10-21:

| Component | Implemented | Compiles | Runs | Real hardware verified |
| --- | --- | --- | --- | --- |
| Native arm64 PCI CLI | yes | yes, warnings as errors | yes | identifies 14f1:8800 |
| Exact-match DriverKit probe | yes | yes | yes | PCI config/BAR and v2 MMIO |
| Ad hoc development host | yes | yes | yes | activated/enabled |
| ADC initialization | yes | yes | yes | readback verified |
| DMA/RISC capture | yes | yes | yes | genuine ADC, IRQ, wrap and stop |
| Streaming / file capture | yes | yes | yes | through 60 s CVBS |
| RF input / VHS-Decode | yes | decoder installed | yes | actual RF decoded to colour PAL; see RF report |

Before activation, CLI `--info` returned the real endpoint at `4:0:0`, no attached driver,
16 KiB mapper pages and `capture_available:false`. DriverKit build completed with
`BUILD SUCCEEDED`; host and dext ad hoc signatures verified with codesign.
Build logs and actual diagnostic output remain under ignored `artifacts/`.
An initial CLI compile error (void pointer conversion) was corrected before the
successful build. SDK paths include `System/DriverKit`; a first inspection at the
macOS-style framework path failed and was corrected.

Remaining acceptance work: multi-minute/30–60-minute captures, untested failure
paths and broader RF quality/format validation. First PAL VHS RF decoding is now
verified; see docs/RF-FIRST-CAPTURE.md. Earlier Stage A
observations below are historical; Stage B results follow.
No synthetic sample data may count as hardware evidence. Signal statistics cannot
prove correct analog capture. Electrical isolation/loading require their own evaluation; successful decoding
does not certify them.

Stage A v1 attached on the actual CX23880. PCI command `0000`, BAR0 `02000000`,
BAR size 16777216/type M32, interrupt config `37140100`; GetInterruptType(index 0)
succeeded with type 1 (level). MMIO was deliberately skipped because memory decode
was off. CLI JSON exposed the immutable snapshot. No DMA or config/MMIO writes.
Unsupported capture arguments returned usage and created no file.

V2: actual 13-register snapshot succeeded with PCI command `0000 → 0002 → 0000`,
video DMA control and both interrupt masks zero, RISC readback undefined (write-only). No
status acknowledgements, ADC initialization or DMA. After Open, legacy interrupt
controller/specifier properties appeared; delivery remains untested.
`python3 scripts/check-probe.py` checks the actual snapshot, including command
restoration and disabled engines/masks. `--info` continues to label it as attachment
time, not current state.

Offline analyzer tests: uniform 0..255 histogram with analytic mean/variance,
constant input with zero AC RMS, and empty-input rejection: all 3 passed. These
fixtures are synthetic arithmetic tests, not RF data or DMA evidence.

## Stage B — static ADC and mapping bring-up

2026-10-21, driver bundles v3/v4 (control API 3):

- Actual user-client status RPC works outside the agent sandbox. Sandbox access
  failed with `0xe00002e2`; the same signed CLI outside it succeeded. No new
  signing profile or security-setting change was needed.
- `--dma-check`: 64 MiB sample mapping, one segment at device IOVA `80000000`;
  contiguous RISC mapping at `84004000`; 32-bit bounds, lengths and alignment
  checked. Prepared driver-owned staging memory round-tripped a 16 KiB pattern
  using IODMACommand PerformOperation. CompleteDMA succeeded. Bus mastering stayed
  off and PCI command was restored to `0000`. This is not hardware DMA evidence.
- IOInterruptDispatchSource::Create on the actual legacy endpoint returned success,
  type level. Contrary to the SDK's MSI-only limitation statement, source creation
  works here; enable/actual interrupt delivery still requires testing.
- First raw ADC readback stopped at OUTPUT_FORMAT: Linux writes `0f`, chip reads
  `0e`. Manufacturer datasheet 101069A p6-72 marks bit 0 read-only/reserved zero.
  V4 preserves Linux's write but excludes that bit from equality validation.
- All 18 static raw ADC writes then passed readback: VMUX1, level 0, boost off,
  center offset 8, PLL 11000000, SCONV 20000, RAW8. No DMA started. The nominal
  rate is configured, not measured; no connector mapping is yet established.
- Manufacturer datasheet p6-47 defines DEV_CNTRL2 as **write-only**. Earlier Stage A
  interpretations of its readback as RISC-enable state were invalid. They must
  not be used for stop verification. P6-86 defines VID_XFER_STATUS bit 0 as video
  transfer-in-progress. Capture teardown will use that, writable engine control,
  stopped counter and PCI command; no CX23885 TC_REQ assumptions.
- Offline RISC tests (ASan/UBSan) pass: two-word 32-bit WRITE encoding, SG boundary,
  page counter increment/reset, interrupt period, backward wrap, invalid high/unaligned
  addresses and ADC option bounds. This does not validate the hardware engine.

Inputs confirmed disconnected by the user. No analog sample has been captured yet.


## Genuine raw capture — driver v7

ADC/mapping milestone commit: `1b78b14`. Native capture CLI and interrupt-driven
RISC ring now compile and have run on the card. No completion polling fallback
is used. IOInterruptDispatchSource enable and actual callbacks work on this macOS
with the legacy endpoint, despite the SDK's MSI-only description.

- v5/v6 pre-start attempts: zero-byte files, no bus mastering. An incorrect static
  equality check on live DMA counters/pointer caused rejection. Datasheet p6-85
  says CNT2 starts consuming CDT entries. V7 verifies static SRAM CDT/CMDS instead.
- First v7 8 MiB attempt: watchdog ambiguity limit fired before a completion event;
  stopped safely after ~112 ms, zero output, command restored. No silent retry.
- Disconnected, 64 MiB ring, 1 s: 28,636,363 bytes; 111 IRQ callbacks, codes 110..112,
  all 109 full 256 KiB blocks unique, no reported hardware/overrun errors.
- Disconnected, warmed 8 MiB ring, 1 s: same byte count, 111 callbacks and **3 ring
  wraps**; all 109 full blocks unique. This does not resolve the cold small-ring
  startup limitation. Default remains 64 MiB.
- Disconnected, 64 MiB ring, 10 s: 286,363,630 bytes; 1094 callbacks, 4 wraps; all
  1092 full blocks unique; no reported hardware/overrun errors.
- PAL MiniDV CVBS, RCA, VMUX1, level=0, sixdb=0, offset=8, 5 s: 143,181,815 bytes;
  548 callbacks, 2 wraps; all 546 full blocks unique; codes 70..159, mean 110.796,
  stddev 14.969, no samples at 0 or 255. Sync-like pulse spacing about 63.94 us
  (~15.64 kHz at nominal sample rate), strong spectral carrier near 4.4341 MHz.
  These are approximate uncalibrated diagnostics, not exact PAL clock measurements.
- RCA VMUX0 comparison, 1 s: genuine signal also present (codes 70..187), consistent
  with white-card C31 input sharing. VMUX1 responds compared to the disconnected
  baseline and is usable; exclusive connector-to-channel wiring is not claimed.

Every successful run verified disabled video DMA, transfer-in-progress zero,
unchanged counter across shutdown observations, untouched 16 KiB DMA guard,
PCI command restoration to 0000 and successful CompleteDMA cleanup. Hardware
register settings are read back before capture. Metadata separates delivered
full DMA blocks from the exact byte prefix written for the requested nominal
sample duration. Timing includes startup/teardown and does not calibrate the clock.

Hashes plus RISC counter observations detect several failure modes; they do not
prove analog sample order or zero loss. Dropped-sample count remains null. At this milestone RF had not been attempted;
CVBS-Decode and subsequent RF results follow below.


Hardware cancellation tests on v7 (real PAL CVBS data, 64 MiB ring):
SIGINT finalized 20,971,520 bytes with complete sidecar and exit 0; SIGKILL left
22,020,096 bytes and the initial incomplete sidecar, but user-client closure stopped
DMA. A three-second SIGSTOP of the child consumer triggered one explicit overrun
event, stopped the hardware, then returned failed metadata/exit 1 after SIGCONT.
All three independently verified command 0000, video DMA off, stop verification,
no quarantine, and transfer status zero. No unrelated process was signalled.
These are consumer cancellation/overrun tests, not disk-full or unplug tests.

## Sustained CVBS and decoder validation — driver v8 / API 5

Real RCA PAL input, card and C31 unmodified, nominal 28.636363 MSPS:

| Capture | Bytes | IRQ callbacks | Observed wraps | Result |
| --- | ---: | ---: | ---: | --- |
| v7 CVBS, 60 s | 1,718,181,780 | 6,556 | 25 | complete, no reported DMA/overrun errors |
| v8 AGC disabled, 5 s | 143,181,815 | 548 | 2 | decoded 225 PAL fields |
| v8 AGC disabled, 10 s | 286,363,630 | 1,094 | 4 | decoded 498 PAL fields |

All 6,554 full 256 KiB chunks in the 60 s capture and all 1,092 in the v8 10 s
capture were unique. This exposes stale blocks, but does not prove zero sample
loss. Stop verified, guard intact, transfer status zero, PCI command restored to
0000. The ten-second sample codes span 60..196; no rail clipping. No calibrated
sampling-clock measurement or authoritative dropped-sample counter exists.

Baseline CVBS settings VMUX1/level0/sixdb0 show sync-tip mean ~72.45, porch ~100.05,
threshold-estimated line period ~63.936 us (~15,640.5 Hz using the nominal clock),
and chroma-band peak ~4.43407 MHz. These are approximate first-second diagnostics,
not a precise PAL timing or clock calibration. VMUX0 also receives RCA. Sixdb1
comparison clipped low and did not achieve field lock; sixdb0 remains default.

The upstream-enabled ADC AGC configuration initially failed CVBS field lock
(0-byte TBC, despite decoder exit 0). Original Conexant INPUT_FORMAT bit13
AGCEN is explicitly documented: clearing only this gate via `--agc-enable 0`
produced recognizable decoded video. All other raw registers remain the pinned
Linux configuration. The optional setting is recorded in status/sidecar; default1
preserves upstream. No CVBS decoding/filtering is added to the driver.

Built pinned VHS-Decode 177b505 in an isolated Python 3.12 environment from a Git
archive under build/; original reference checkout unchanged. Verified help and
ran `cvbs-decode --frequency 28.636363 --pal -A INPUT.u8 OUTPUT`. The 5 s decode
contains 225 fields with decodeFaults=0, syncConf45; startup skipped ~0.486 s.
The 10 s decode contains 498 fields with decodeFaults=0; 52 have syncConf100 and
446 syncConf45. Both emitted a startup missing-data warning. This is a decoder
observation, not evidence establishing a DMA loss count. A single-field monochrome
preview was visually inspected and shows recognizable source imagery.

V8 DMA self-test succeeded using the existing interrupt source; a subsequent
10 s capture confirms the test no longer disturbs capture interrupt registration.
Native arm64 builds, ASan/UBSan RISC tests, three statistics unit tests and the
attachment snapshot check pass. A further real 2 s capture verified the CLI's
idle-sleep assertion appears during capture and is released after exit0. The first
agent-sandbox attempt was denied IOServiceOpen e00002e2 before creating samples;
the permitted outside-sandbox run succeeded. No security settings were changed.
Idle defaults restored to AGC1/VMUX1/level0/sixdb0/offset8, PCI command0000,
videoDMA0, running0, stop_verified1, quarantine0. Forced sleep, disconnection,
disk-full and archival durations remain untested. At this milestone VHS RF was
still pending; the subsequent successful RF test is recorded below.

Milestone commits: `1b78b14` (ADC/readback and 32-bit mappings), `4ea0fd5`
(genuine raw streaming, IRQs and teardown). Subsequent v8 CVBS validation is
recorded in the next focused commit; use Git history for its full hash.

The v8 AGC-disabled 10 s capture's first nominal second measures ~63.9946 us
(~15,626.3 Hz), sync tip ~71.70 and porch ~102.27 at threshold80; chroma peak
~4.43405 MHz. Unlike the earlier AGC-enabled baseline, this is close to PAL line
timing. These diagnostics still assume the nominal oscillator. Startup settling
and decoder confidence remain limitations; no precise frequency calibration.
V8 capture/decoder milestone committed as `8a4d842`.

## Normal colour export

Official TBC Tools v3.3.0 macOS arm64 release, tag commit
`9ce60e193e981b842889cca133f77126ce4c7954`; downloaded asset SHA256 matches the
GitHub release digest recorded in references/manifest.json. Mounted read-only,
copied into ignored build/tbc-tools.app, with no system installation or security
changes. Help verified before execution:

```sh
build/tbc-tools.app/Contents/MacOS/tbc-video-export --profile ffv1 --no-dropout-correct --threads 4 artifacts/cvbs-agc0-v8-10s-decoded.tbc artifacts/cvbs-agc0-v8-10s-export
```

Exit0, completed in ~4.2 s. Export used automatic PAL TRANSFORM3D colour decoding
and automatic field order (resolved TFF), no dropout correction. FFprobe counted
249 video frames: FFV1, 928×576, yuv422p10le, 25/1 fps, top-field-first, duration
9.96 s. File size 147,550,617 bytes. PNG frames at 1/5/9 s were visually inspected:
recognizable colour source video across the recording, with visible chroma/luma
artefacts. No perfect-colour, archival-quality or zero-loss assertion. Output is
`artifacts/cvbs-agc0-v8-10s-export.mkv`; comparison preview is
`artifacts/cvbs-agc0-v8-10s-contact.png`. All captures/exports remain ignored.

The current source clone uses tbc-analyse, the successor to ld-analyse. Desktop
inspection could not be completed: computer-use rejected the app path and denied
Finder access. No permission bypass attempted. Only decoded-file statistics and
exported image inspection are verified; interactive GUI inspection remains open.
The export runtime also needed outside-sandbox execution after its semaphore was
denied inside the agent sandbox. This is not a DriverKit signing/DMA blocker.

The first source-build attempt targeted stale VHS-Decode CMake paths (missing
C++ tool directories). Homebrew qwt/fftw were installed for that attempt; the
current TBC-tools source additionally requires ONNX Runtime/submodules. Instead
of extending those dependencies, the published native release was used. No claim
is made that the current TBC-tools source was locally compiled or CI-validated.

## First real VHS RF acceptance

See [docs/RF-FIRST-CAPTURE.md](docs/RF-FIRST-CAPTURE.md) for reproducible commands
and actual results. Driver v8 required no code changes: the general raw ADC engine
captured a real playing VHS RF source via RCA. The one-second sample decoded into
48 PAL fields, and the ten-second sample into 498 fields. Normal offline export
produced recognizable colour video, inspected at multiple points in the ten-second
recording. This establishes initial short-duration end-to-end RF success.
Multi-minute/archival reliability, exact clock, calibrated signal quality, loading,
forced sleep and active disconnection remain unverified. No zero-loss claim.
