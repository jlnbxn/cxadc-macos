# CX23880 CXADC for macOS

Native arm64 PCIDriverKit raw ADC capture for Conexant `14f1:8800`. Driver v8 has
captured real unsigned 8-bit samples with disconnected inputs (1 and 10 seconds)
and PAL MiniDV composite on RCA (through 60 seconds). CVBS-Decode produced a
recognizable PAL picture from real captures. Genuine VHS playback RF has now
also been captured and decoded into recognizable colour PAL video with VHS-Decode.
This is development software, not archival validation.

```sh
./scripts/build.sh
./scripts/build-local-host.sh       # builds; does not install
./build/cxadc-capture --list
./build/cxadc-capture --info          # attachment snapshot, not live state
./build/cxadc-capture --status        # live hardware/user-client status
./build/cxadc-capture --dma-check     # mapping test; never starts hardware DMA
./build/cxadc-capture --configure --vmux 1 --level 0 --sixdb 0 --center-offset 8
./build/cxadc-capture --duration 1 --output test.u8 --vmux 1 --level 0
python3 tools/analyze_u8.py test.u8 --sample-rate 28636363
python3 tools/check_capture.py test.u8
```

Output is a genuine ADC byte stream, without PAL decoding, filtering, resampling,
compression or byte swapping. Stock-clock configuration is nominally 28.636363
MSPS; no calibrated oscillator measurement exists. Finite duration writes the exact
nominal-rate byte count from ordered full DMA blocks, trimming only the final
block prefix. Delivered and written counts are reported separately.

Omit `--duration` for continuous capture; Ctrl-C requests hardware stop followed by
writer-queue drain. The output and `FILE.u8.json` sidecar refuse overwrite. Failed
captures retain a visibly failed sidecar and possibly a valid data prefix. Abrupt
process death may leave the initial incomplete sidecar. Statistics go to stderr.
File output is implemented; stdout streaming is not yet supported.

Options: `--vmux 0..3`, `--level 0..31`, `--sixdb 0|1`, `--center-offset 0..255`,
`--agc-enable 0|1`, `--ring-mib 8|64`. Defaults: AGC enabled (Linux reference), VMUX1, level 0, sixdb 0, offset 8, 64 MiB ring. RCA
CVBS has been observed on VMUX1 and VMUX0 with C31 intact; no physical modification
is required for these tests. Sixdb follows Linux CXADC's logical control convention;
electrical boost polarity/calibration remains unverified.

The 8 MiB ring exercised three wraps in a warmed one-second run, but its first cold
start hit the conservative watchdog limit. Use the 64 MiB default. Capture stops
on hardware faults, consumer overrun, missing completion or ambiguous modulo-counter
progress. No authoritative dropped-sample count is available; metadata leaves it
null. A short run with no reported errors is not proof of zero sample loss.

The exact endpoint/subsystem match is `14f1:8800` / `4e4f:4553`. The driver never
binds to the ITE bridge, 8804 function or CX23885. All DMA mappings use device-bound
IODMACommand, validate full 32-bit ranges and stay prepared through verified stop.
Real interrupt callbacks work on the observed legacy endpoint; no polling fallback
is used. Shutdown failure quarantines mappings and blocks further capture.

Tested with macOS 26.7.1, Xcode/DriverKit 27.0. The development host is ad hoc signed;
normal distribution requires proper Apple signing/provisioning. Existing SIP and
system-extension developer settings were not changed. The host activation request
may require approval in System Settings. Activation success does not prove the new
instance attached; check `--status` driver build. Never replace/deactivate an active
capture. The existing HVR-1200 source and activated driver remain unchanged.

Native builds have no external dependencies. Tests:

```sh
xcrun clang++ -std=c++20 -fsanitize=address,undefined tests/risc_tests.cpp -o build/risc-tests
./build/risc-tests
python3 -m unittest discover -s tests -v
```

Reference clones are separate and pinned in [references/manifest.json](references/manifest.json).
See [HARDWARE.md](HARDWARE.md), [ARCHITECTURE.md](ARCHITECTURE.md),
[TESTING.md](TESTING.md), [PROGRESS.md](PROGRESS.md) and [source/license notes](docs/REFERENCES.md).
Project source is GPL-2.0-or-later. Builds, captures, decode outputs and dependency
installations stay ignored. No remote or code publication is configured.

Next: multi-minute/archival reliability, remaining failure paths and signal-level
quality assessment. The user supplied the RF tap; no physical changes were made
by this project.

PAL CVBS on this unmodified card required disabling the documented global ADC AGC
gate for reliable field lock. The default remains the pinned Linux configuration.
This is a hardware setting, not CVBS processing inside the capture path:

```sh
./build/cxadc-capture --duration 10 --output cvbs_test.u8 --vmux 1 --level 0 --sixdb 0 --agc-enable 0
cvbs-decode --frequency 28.636363 --pal -A cvbs_test.u8 cvbs_test
```

`--frequency` is MHz; these arguments were verified against the pinned decoder's
help and executed on genuine captures. Inspect the TBC and JSON, not just exit
status: unsuccessful field lock can still return exit 0. Decoder startup warnings
and reduced sync confidence remain; see TESTING.md. The CLI holds a temporary
idle-sleep assertion during capture. Forced sleep and hot unplug remain untested.

Normal colour export was also verified using official TBC Tools v3.3.0 arm64:

```sh
tbc-video-export --profile ffv1 --no-dropout-correct --threads 4 cvbs_test.tbc cvbs_test_export
```

The real ten-second recording exported 249 frames, 928×576 at 25 fps, 9.96 s,
yuv422p10le in FFV1 MKV. Source `.u8` is unchanged. Frame previews at 1, 5 and 9 s
show recognizable colour video, with remaining visible artefacts. This is an
external verification/export step; codecs are not part of the capture engine.
Current TBC Tools calls the GUI `tbc-analyse` (formerly `ld-analyse`). GUI inspection
was blocked by denied computer-use access and remains unverified.

First VHS RF validation used the same capture engine and Linux-reference AGC:

```sh
./build/cxadc-capture --duration 10 --output rf_test.u8 --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 1
vhs-decode --frequency 28.636363 --system PAL --tape_format VHS --threads 2 rf_test.u8 rf_test
tbc-video-export --profile ffv1 --no-dropout-correct --threads 4 rf_test.tbc rf_test_export
```

The real ten-second RF sample decoded to 498 PAL fields and exported 249 colour
frames. RF stream tests also reached 60 s. See [the RF test report](docs/RF-FIRST-CAPTURE.md)
for exact hardware context, results and limitations. The test validates this
short-duration end-to-end path; electrical loading and archival quality are not
thereby certified. The first recognizable RF result was obtained without increasing gain or
modifying C31.

For the subsequently tested PAL S-VHS recording, use the S-VHS decoder profile.
On that recording the supported level-reuse option recovered acquisition from
the beginning; it does not change the captured ADC samples:

```sh
vhs-decode --frequency 28.636363 --system PAL --tape_format SVHS --use_saved_levels --threads 2 rf_test.u8 rf_test_svhs
python3 tools/check_decode.py rf_test_svhs.tbc.json
```

Use the tape's actual recording format, not merely the cassette label. See
[the one-minute S-VHS/TBC report](docs/RF-ONE-MINUTE.md) for the complete test and
decoder diagnostics. `check_decode.py` checks metadata and TBC byte counts; its
flags are not an authoritative hardware loss count or a calibrated SNR test.

A subsequent real one-minute capture after user-reported C31 removal also decoded
successfully. Its [matched-scene comparison](docs/C31-COMPARISON.md) shows mixed
diagnostics and unchanged ADC span; a major quality gain or cure for rare tears
is not established. The general raw capture engine and decoder settings remained
unchanged. No MP4 was requested or generated.

The subsequent real RF gain sweep tested levels 0/4/8/16/24/31 with sixdb 0.
Level 31 reached codes 36–182 without clipping in a five-second sample and decoded
successfully. This is a candidate for the next longer trial, not a new universal
default. The CLI still defaults to conservative level 0. The user reported no
visible SCART disturbance during the sweep. See [gain test results](docs/RF-GAIN-SWEEP.md)
for the measurements and limits of the comparison.

The pictured bright horizontal streak was traced to a repeatable RF dropout.
Our earlier hardware-comparison exports deliberately disabled dropout correction.
A separate standard export concealed that exact streak using the saved data:

```sh
tbc-video-export --profile ffv1 --threads 4 rf_test.tbc rf_test_corrected
```

For normal viewing leave dropout correction enabled; retain the uncorrected
version for diagnosis. Correction conceals defects, without restoring original
missing detail. See [the exact-frame investigation](docs/DROPOUT-INVESTIGATION.md).
This result does not establish an amplifier requirement or diagnose every tear.
