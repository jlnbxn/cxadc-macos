# Validation status

2026-10-21:

| Component | Implemented | Compiles | Runs | Real hardware verified |
| --- | --- | --- | --- | --- |
| Native arm64 PCI CLI | yes | yes, warnings as errors | yes | identifies 14f1:8800 |
| Exact-match DriverKit probe | yes | yes | yes | PCI config/BAR and v2 MMIO |
| Ad hoc development host | yes | yes | yes | activated/enabled |
| ADC initialization | no | — | — | no |
| DMA/RISC capture | no | — | — | no |
| Streaming / file capture | no | — | — | no |
| RF input / VHS-Decode | no | — | — | no |

Before activation, CLI `--info` returned the real endpoint at `4:0:0`, no attached driver,
16 KiB mapper pages and `capture_available:false`. DriverKit build completed with
`BUILD SUCCEEDED`; host and dext ad hoc signatures verified with codesign.
Build logs and actual diagnostic output remain under ignored `artifacts/`.
An initial CLI compile error (void pointer conversion) was corrected before the
successful build. SDK paths include `System/DriverKit`; a first inspection at the
macOS-style framework path failed and was corrected.

Safe stages remaining: static ADC initialization, private DMA
mapping while engines off, verified completion/teardown, one-second capture, 10 s,
60 s, 30–60 minutes, isolated RF input, then actual PAL VHS/S-VHS decoding.
No synthetic sample data may count as hardware evidence. Signal statistics cannot
prove correct analog capture. Real RF must await resolved isolation and loading.

Stage A v1 attached on the actual CX23880. PCI command `0000`, BAR0 `02000000`,
BAR size 16777216/type M32, interrupt config `37140100`; GetInterruptType(index 0)
succeeded with type 1 (level). MMIO was deliberately skipped because memory decode
was off. CLI JSON exposed the immutable snapshot. No DMA or config/MMIO writes.
Unsupported capture arguments returned usage and created no file.

V2: actual 13-register snapshot succeeded with PCI command `0000 → 0002 → 0000`,
video DMA control and both interrupt masks zero, known RISC enable clear. No
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
prove analog sample order or zero loss. Dropped-sample count remains null. No RF
input or VHS-Decode acceptance test has been attempted. CVBS-Decode is next.


Hardware cancellation tests on v7 (real PAL CVBS data, 64 MiB ring):
SIGINT finalized 20,971,520 bytes with complete sidecar and exit 0; SIGKILL left
22,020,096 bytes and the initial incomplete sidecar, but user-client closure stopped
DMA. A three-second SIGSTOP of the child consumer triggered one explicit overrun
event, stopped the hardware, then returned failed metadata/exit 1 after SIGCONT.
All three independently verified command 0000, video DMA off, stop verification,
no quarantine, and transfer status zero. No unrelated process was signalled.
These are consumer cancellation/overrun tests, not disk-full or unplug tests.
