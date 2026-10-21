# Validation status

2026-10-21:

| Component | Implemented | Compiles | Runs | Real hardware verified |
| --- | --- | --- | --- | --- |
| Native arm64 PCI CLI | yes | yes, warnings as errors | yes | identifies 14f1:8800 |
| Exact-match DriverKit probe | yes | yes | yes | PCI config/BAR query |
| Ad hoc development host | yes | yes | yes | activated/enabled |
| ADC initialization | no | — | — | no |
| DMA/RISC capture | no | — | — | no |
| Streaming / file capture | no | — | — | no |
| RF input / VHS-Decode | no | — | — | no |

CLI `--info` returned the real endpoint at `4:0:0`, no attached driver,
16 KiB mapper pages and `capture_available:false`. DriverKit build completed with
`BUILD SUCCEEDED`; host and dext ad hoc signatures verified with codesign.
Build logs and actual diagnostic output remain under ignored `artifacts/`.
An initial CLI compile error (void pointer conversion) was corrected before the
successful build. SDK paths include `System/DriverKit`; a first inspection at the
macOS-style framework path failed and was corrected.

Safe stages remaining: attachment/readback, static ADC initialization, private DMA
mapping while engines off, verified completion/teardown, one-second capture, 10 s,
60 s, 30–60 minutes, isolated RF input, then actual PAL VHS/S-VHS decoding.
No synthetic sample data may count as hardware evidence. Signal statistics cannot
prove correct analog capture. Real RF must await resolved isolation and loading.

Stage A v1 attached on the actual CX23880. PCI command `0000`, BAR0 `02000000`,
BAR size 16777216/type M32, interrupt config `37140100`; GetInterruptType(index 0)
succeeded with type 1 (level). MMIO was deliberately skipped because memory decode
was off. CLI JSON exposed the immutable snapshot. No DMA or config/MMIO writes.
Unsupported capture arguments returned usage and created no file.
