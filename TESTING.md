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
