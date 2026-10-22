# cxadc-macos

A native macOS CXADC implementation for Conexant CX2388x/CX25800-based raw RF
capture hardware. **Experimental and under active development.** Only the
CX23880-19 configuration below has been verified; the wider chip family is an
intended scope, not a compatibility claim.

cxadc-macos captures unsigned 8-bit ADC samples for
[VHS-Decode](https://github.com/oyvindln/vhs-decode) using a native arm64
PCIDriverKit extension and command-line utility. It performs no video decoding,
filtering, compression or resampling in the capture path. PAL composite and VHS
playback RF have both been captured and decoded into recognizable colour video
on real Apple Silicon hardware.

## Status

| Area | Status |
| --- | --- |
| PCI attachment, register access and ADC controls | Implemented and hardware verified on the matched endpoint |
| 32-bit device-visible DMA mappings, RISC ring and interrupts | Implemented and hardware verified |
| Raw `.u8` capture and JSON metadata | Verified through one 45-minute RF recording |
| PAL CVBS and PAL VHS/S-VHS offline decoding | Verified using external decode tools |
| Full-tape lossless export with dropout correction | Verified: 44:56.52 FFV1 output, full-file decoding check passed |
| Discontinuity detection | Partial: faults/overruns and ambiguous progress terminate capture; no authoritative lost-sample count |
| Signed distribution and installation on other Macs | Not validated; local development packaging only |
| Other cards, CX25800, Intel Macs, sleep/wake and active unplug | Unverified |
| Audio, stdout streaming, compression and graphical capture UI | Not implemented |

These results demonstrate one working setup, not general hardware compatibility
or archival reliability. See [test results and known failures](TESTING.md).

## Verified hardware

- Apple M3 Max, macOS 26.7.1; built with Xcode/DriverKit 27.0.
- White Y328 board with CX23880-19, PCI `14f1:8800`, subsystem `4e4f:4553`.
- ITE IT8893E marking reported on the board; PCI bridge `1283:8893` verified.
- OWC Express 1M2 USB4 enclosure with an M.2-to-PCIe adapter.
- JVC HR-S7851EU playback RF; a PAL MiniDV composite source was tested separately.

The test arrangement, power, RF tap, C31 comparison and measurement limits are
recorded in [HARDWARE.md](HARDWARE.md). The driver deliberately matches only the
observed endpoint/subsystem, never the bridge or CX23885.

## Build and installation

A full Xcode installation with the macOS and DriverKit SDKs is required. Native
builds use only Apple tools/frameworks; Python 3 is used for offline diagnostics
and tests. From the repository root:

```sh
./scripts/build.sh                 # arm64 CLI, development ad hoc signature
./scripts/build-driver.sh          # unsigned DriverKit extension
./scripts/build-local-host.sh      # ad hoc development host and embedded extension
./scripts/test.sh                  # CPU-only tests; no device access
```

These commands **build only**. They do not install or activate a driver or change
system security. [BUILDING.md](BUILDING.md) explains installation, the existing
placeholder bundle IDs, required Apple entitlements, signing and removal.

The development Mac already had special driver-development settings. That does
not establish that disabling SIP is required on other Macs, nor that an ad hoc
build will load there. No ready-to-install, notarized public binary is provided.

## Capture

After installing and approving an appropriately signed driver:

```sh
./build/cxadc-capture --list
./build/cxadc-capture --info         # attachment-time diagnostic snapshot
./build/cxadc-capture --status       # live state
./build/cxadc-capture --help

./build/cxadc-capture --duration 10 --output rf_test.u8 \
  --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 1
python3 tools/analyze_u8.py rf_test.u8 --sample-rate 28636363
python3 tools/check_capture.py rf_test.u8
```

Outputs refuse overwrite. Each raw file has a `.u8.json` sidecar recording
settings, byte counts, errors and shutdown state. Omit `--duration` for continuous
capture and press Ctrl+C to stop gracefully. Statistics are printed to stderr.
The CLI holds an idle-sleep assertion while capturing; forced sleep is untested.

Controls: VMUX 0–3, level 0–31, sixdb 0/1, center offset 0–255, AGC enable 0/1,
and ring size 8/64 MiB. Defaults are VMUX1, level 0, sixdb 0, offset 8, AGC 1 and
64 MiB. Keep the 64 MiB ring: a cold 8 MiB trial hit the conservative watchdog.
`--dma-check` prepares/tests mappings without starting capture.

VMUX1 works with RCA on the tested board; this mapping is not universal.
Level31/sixdb 0 passed the full-tape test, but level 0 remains the conservative
default. Check clipping on your own input. Gain span is not a calibrated SNR test.
Never connect a VCR RF point without establishing DC isolation, grounding and
loading for that specific deck; see the documented experimental tap.

### Offline decoding and export

Install VHS-Decode and TBC Tools separately; they are not bundled or needed to
build the driver. Tested revisions are pinned in
[references/manifest.json](references/manifest.json). Verify the installed tools'
help before using another revision.

For PAL S-VHS/S-VHS ET recordings tested here:

```sh
vhs-decode --frequency 28.636363 --system PAL --tape_format SVHS \
  --use_saved_levels --threads 2 rf_test.u8 rf_test
python3 tools/check_decode.py rf_test.tbc.json
tbc-video-export --profile ffv1 --threads 4 rf_test.tbc rf_test_export
```

Use `--tape_format VHS` for standard VHS recordings. The frequency is in MHz.
The exporter enables standard dropout correction by default. Earlier diagnostic
comparisons explicitly disabled it; correction conceals defects using nearby
picture information and cannot recover missing original detail.

PAL CVBS on the tested, initially unmodified card needed `--agc-enable 0`:

```sh
./build/cxadc-capture --duration 10 --output cvbs_test.u8 \
  --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 0
cvbs-decode --frequency 28.636363 --pal -A cvbs_test.u8 cvbs_test
```

Successful exit status alone does not prove a successful decode: inspect field
counts, diagnostics and the resulting picture. The driver always captures raw
ADC data, regardless of the signal carried by the input.

## Limitations and storage

At nominal 28.636363 MSPS, raw capture requires 28.6 MB/s, approximately 1.72 GB
per minute. A 45-minute recording uses 77.32 GB raw; both decoded TBC files added
191.59 GB and the verified lossless export added 40.07 GB. Plan for intermediates
and working headroom; see [storage planning](docs/FULL-TAPE-STORAGE.md).

The oscillator has not been independently measured. DMA mapping addresses are
validated for the chip's 32-bit engine; CPU and device addresses are not treated
as interchangeable. Interrupts are real; there is no polling completion fallback.
Error counters and block hashes cannot prove zero sample loss. Sleep/wake,
active unplug, disk-full recovery and repeated multi-hour operation remain
unverified. Audio is not acquired.

## Source layout

| Path | Contents |
| --- | --- |
| `driver/` | PCIDriverKit lifecycle, DMA and interrupts |
| `src/` | Native capture and diagnostic CLI |
| `common/`, `core/` | Shared ABI, raw configuration and RISC construction |
| `installer/` | System-extension activation/deactivation host |
| `scripts/`, `tests/`, `tools/` | Builds, CPU tests and diagnostics |
| `docs/` | Technical reports, provenance and publication audit |
| `references/manifest.json` | Pinned external reference revisions; no embedded clones |

See [architecture](ARCHITECTURE.md), [hardware](HARDWARE.md),
[testing](TESTING.md) and [milestones](PROGRESS.md).

## License and acknowledgments

Source is **GPL-2.0-or-later**; see [LICENSE](LICENSE),
[third-party notices](THIRD_PARTY_NOTICES.md) and
[source provenance](docs/REFERENCES.md). The raw hardware configuration and DMA
scheme derive from the CXADC community's work; this is not an independent
clean-room implementation.

Foundational and related projects:

- [CXADC Linux](https://github.com/happycube/cxadc-linux3)
- [CXADC Windows](https://github.com/JuniorIsAJitterbug/cxadc-win)
- [VHS-Decode](https://github.com/oyvindln/vhs-decode)
- [MISRC-GUI](https://github.com/harrypm/MISRC-GUI) — related community capture UI;
  integration with this driver has not been tested.
- [TBC Tools](https://github.com/harrypm/tbc-tools) — external export/analysis tools.

Development was substantially assisted by OpenAI Codex under human technical
direction, with hardware testing and validation on real equipment.
