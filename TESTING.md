# Validation and known limitations

## Evidence levels

Implemented, compiled, executed, hardware verified and decoded into recognizable
video are distinct claims. Driver v8/API5 reached each level on the one setup in
[HARDWARE.md](HARDWARE.md). Synthetic arithmetic/RISC fixtures are CPU tests only;
no generated samples are counted as hardware evidence. Private captures and logs
are intentionally excluded from Git.

## Automated tests

```sh
./scripts/test.sh
```

The RISC/ADC suite runs with AddressSanitizer and UndefinedBehaviorSanitizer. It
covers instruction encoding, scatter/gather boundaries, 32-bit address bounds,
alignment, page counters, interrupt cadence, ring advancement and option bounds.
Python tests cover analytical byte statistics, empty input and exact recognition
of pinned-decoder replacement-field metadata, including rejection of unexplained
changes. No automated test starts hardware capture.

Build/release-preparation regression results are recorded in
[the publication audit](docs/PUBLICATION-AUDIT.md). Hardware-test scripts are
opt-in, separate from this suite:

- `scripts/check-probe.py` checks the saved attachment snapshot; no new MMIO.
- `scripts/test-hardware-stop.py --hardware MODE` captures real samples and signals
  only its own child CLI. It is not a routine release-cleanup check.

## Actual hardware progression

All durations below use nominal 28.636363 MSPS unsigned 8-bit samples. Successful
captures had complete metadata, reported no DMA/overrun errors and verified safe
stop with PCI command restored to 0000. This is not a zero-sample-loss assertion.

| Test | Raw bytes | Observed result |
| --- | ---: | --- |
| Disconnected input,1 s |28,636,363 |111 IRQ callbacks;109 unique full 256 KiB blocks; codes 110–112 |
| Disconnected input,10 s |286,363,630 |1,094 callbacks;4 wraps;1,092 unique full blocks |
| Warm8 MiB ring,1 s |28,636,363 |3 ring wraps; cold-start limitation remains |
| PAL MiniDV CVBS,60 s |1,718,181,780 |6,556 callbacks;25 wraps;6,554 unique full blocks |
| AGC-disabled PAL CVBS,10 s |286,363,630 |498 decoded PAL fields;249 colour frames,9.96 s |
| First PAL VHS RF,1/10 s |28,636,363 /286,363,630 |48/498 decoded fields; recognizable colour video |
| PAL S-VHS RF,60 s |1,718,181,780 |2,998 fields using saved levels;59.96 s FFV1 |
| Post-C31 RF,60 s |1,718,181,780 |2,994 fields;59.88 s FFV1; matched comparison below |
| Level31 T7 preflight,60 s |1,718,181,780 |codes 36–181, no rail samples; first 5 s decoded to 250 fields |
| Full-tape level 31 RF,2,700 s |77,318,180,100 |294,947 callbacks;1,152 wraps; full decode/export verified |

### PCI, ADC and DMA bring-up

Initial attachment identified the independent `14f1:8800` endpoint behind the
ITE bridge. Bounded memory-decode enable/read/restore changed PCI command
0000→0002→0000. BAR0 is 16 MiB. Legacy interrupt routing appeared after Open;
actual callbacks later worked despite absent initial registry routing properties.

Static ADC initialization/readback and `--dma-check` were validated before
mastering. Device-bound IODMACommand mappings used32-bit IOVAs; sample mapping
was64 MiB and the RISC mapping contiguous. Mapping/CPU round-trip tests alone
were not counted as hardware DMA evidence. Real raw streaming subsequently
exercised RISC execution, interrupts, ring wraps and shutdown.

Observed startup/readback failures were resolved before the working v8 baseline:

- OUTPUT_FORMAT writes0f/read0e: reserved read-only bit0 is excluded from equality
  checks using Conexant datasheet 101069A p6-72.
- DEV_CNTRL2/RUN_RISC is write-only; readback cannot establish engine state.
- Live CNT/pointer values are not static latches; CNT2 begins CDT consumption.
  Static SRAM descriptors/commands are checked instead.
- The first cold 8 MiB capture hit conservative watchdog ambiguity and stopped
  safely with zero output. A warm small-ring test passed;64 MiB remains default.

Shutdown tests check engine/FIFO controls, transfer-in-progress zero, stable
counter observations, intact 16 KiB guard, mastering off, exact PCI command
restoration and CompleteDMA. Failure quarantines mappings rather than releasing
them unsafely. No CX23885 register semantics are assumed.

### Cancellation and explicit failure reporting

Real v7 CVBS hardware tests with 64 MiB ring:

| Test | Result |
| --- | --- |
| SIGINT |20,971,520 bytes; complete sidecar; exit 0 |
| SIGKILL |22,020,096 bytes; incomplete sidecar; user-client closure stopped DMA |
| Consumer SIGSTOP for3 s, then SIGCONT |Explicit overrun; failed metadata/exit 1 |

Each independently verified DMA off, stop verification, command 0000 and no
quarantine. These are consumer tests, not disk-full, forced-sleep or unplug tests.
An idle-sleep assertion was observed during a real2 s capture and released after
exit. It does not validate forced sleep/wake.

### PAL CVBS

Before C31 removal, RCA/VMUX1 produced clear signal change from the disconnected
baseline, approximate64 µs line periodicity and a carrier near 4.434 MHz. VMUX0
also received it, so the channels were not assumed independent.

Default enabled ADC AGC failed CVBS field lock despite decoder exit 0. Clearing
the documented AGCEN gate (`--agc-enable 0`) produced recognizable PAL video;
RF kept the reference AGC-enabled setting. No CVBS processing is in the driver.
A 5 s CVBS decode produced 225 fields with startup exclusion;10 s produced 498 fields.
The 10 s FFV1 export is 928×576,25 fps, yuv422p10le/top-field-first. Offline decoder
warnings do not by themselves establish a hardware sample-loss count.

### RF gain, C31 and dropout observations

- Initial level 0 RF used only about 97–120 codes yet decoded successfully.
- After operator-reported C31 removal the same range remained. A matched55.44 s
  interval showed dropout segments318→295 and bPSNR diagnostic39.7→38.6: mixed
  evidence, not a proven amplitude/quality improvement.
- Six5 s captures at levels0/4/8/16/24/31, sixdb 0, passed. Range expanded from
  99–119 to 36–182, without rail samples. Levels0/16/31 decoded successfully.
  Scenes differed, so comparative quality/SNR improvement is not established.
- The operator reported unchanged live SCART during all gain steps; this did not
  measure tap-connected versus disconnected loading.
- A repeatable bright streak matched the same tape moment and dropout coordinates
  across two passes. Standard dropout correction visibly concealed it using the
  existing data. It does not prove an amplifier is unnecessary for other defects.

Detailed historical reports: [first RF](docs/RF-FIRST-CAPTURE.md),
[one-minute S-VHS](docs/RF-ONE-MINUTE.md), [C31](docs/C31-COMPARISON.md),
[gain](docs/RF-GAIN-SWEEP.md), [dropout](docs/DROPOUT-INVESTIGATION.md).
Those reports describe their own milestone, not later compatibility guarantees.

## Full-tape result

The 45-minute raw file is 77,318,180,100 bytes, matching its complete sidecar. Host
elapsed 2,700.284 s; no reported driver, overrun or writer errors. Stop verified,
DMA off, command restored0000, no quarantine. Idle level 0 restored afterwards.
A selection of 18 one-second windows across the capture had no rail samples;
this is not a complete histogram or full-file continuity proof.

PAL/SVHS decoding at 28.636363 MHz with `--use_saved_levels` produced 134,826 fields,
67,413 frames and 44:56.52 output. Both TBCs contain95,795,221,260 bytes, exactly
matching field dimensions/count; field parity alternates. The decoder reported
22,612 dropout segments in8,812 fields and two replacement-field events. Initial
pre-play lead-in produced acquisition errors; decoded source begins near 3.71 s.

The first automatic export was blocked by an overly strict sequential-number
check. Pinned decoder readfield()/lastvalidfield inserts earlier field data with
its old metadata. Four mismatching metadata positions are exactly explained by
two insertions; copied metadata equals that two entries earlier and the following
field is explicitly flagged. The validator now distinguishes this precise pattern
from unexplained mismatches. Original JSON/TBCs were not renumbered or edited.
This is not a count of hardware DMA discontinuities.

The standard dropout-corrected export is 40,067,321,047 bytes, FFV1 MKV,
928×576/25 fps/yuv422p10le/top-field-first. Duration2696.52 s matches decoded fields.
The exporter reports 22,637 concealments. A full-file FFmpeg video decode with
`-xerror` exited0 and logged no errors. Opening/middle/late programme stills were
visually checked; trailing noise beyond the programme was retained. Spot checks
do not establish every frame's visual quality. There is no captured audio.

## Unverified cases

- Calibrated sampling clock, analog gain/SNR, impedance and VCR loading.
- Authoritative dropped-sample detection; reported count remains null.
- Repeated long captures, other decks/cards/enclosures and other Macs/OS versions.
- Active hot-unplug, forced sleep/wake, disk-full recovery and faulted teardown.
- Signed/notarized distribution on default-security machines.
- GUI tbc-analyse inspection, stdout streaming, audio, compression and other modes.

One successful long capture is evidence of this configuration, not a universal
stability or archival-quality certification.
