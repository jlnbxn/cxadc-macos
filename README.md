# CX23880 CXADC for macOS

Native arm64 PCIDriverKit raw ADC capture for Conexant `14f1:8800`. Driver v7 has
captured real unsigned 8-bit samples with disconnected inputs (1 and 10 seconds)
and PAL MiniDV composite input on RCA (5 seconds). CVBS decoding and VHS RF
acceptance are still pending. This is development software, not archival validation.

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
`--ring-mib 8|64`. Defaults: VMUX1, level 0, sixdb 0, offset 8, 64 MiB ring. RCA
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

Next: validate actual CVBS decoding and export, cancellation/failure paths and longer
streaming, then an appropriately isolated VHS RF input. No RF tap or VCR wiring
changes have been made.
