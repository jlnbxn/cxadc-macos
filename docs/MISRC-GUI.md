# MISRC-GUI on macOS

Experimental native single-card integration, tested on 2026-10-22 with the
CX23880-19/OWC/M3 Max configuration in [HARDWARE.md](../HARDWARE.md).
The driver and standalone CLI remain independently usable and unchanged.

## Checkouts and build

Keep the repositories adjacent. The GUI integration branch is
`feature/macos-cxadc-backend`, based on upstream
`9c3894cd764db20c3eca835d041ad972cf986d37`. These local changes have not been
submitted upstream; an unmodified official release does not yet support this
backend. Driver-side integration is on `feature/misrc-gui-integration`.

Requirements: arm64 macOS, Xcode command-line tools, the already installed and
approved CXADC DriverKit extension (control API 5, tested driver build 8), and
the dependencies in MISRC-GUI's `INSTALLATION.md`. This machine used Homebrew
Meson, Ninja, pkgconf, CMake, FFTW, libusb, FLAC, libuvc, libsoxr and NASM.
The upstream local build also builds its pinned raylib/hsdaoh dependencies.

From the driver checkout:

```sh
scripts/prepare-misrc-gui.sh ../MISRC-GUI
scripts/launch-misrc-gui.sh
```

The preparation script builds the optional C client library, runs upstream's
`scripts/build-local.sh`, copies the library beside `build-local/misrc_gui`,
and ad-hoc signs the GUI with the existing DriverKit user-client entitlement.
Re-sign after rebuilding the executable. This does not install or activate an
extension, change security settings, or establish distribution signing.
See the main README for extension installation/approval requirements. Disabled
SIP on the development machine is not a universal installation requirement.

## Interactive use

1. Launch with `scripts/launch-misrc-gui.sh` from this checkout.
2. Select `[CXADC] CX23880 14f1:8800 [4e4f:4553] macOS` in the device selector.
   With no attached endpoint there is no fabricated device. A visible endpoint
   without an attached driver is labelled accordingly.
3. Press **F8** (Fn-F8 if macOS uses the key for a system function) for VMUX,
   level, sixdb, center offset and AGC controls. Changes require capture stopped,
   are sent to the driver immediately, and are read back before display updates.
   Our RF test used VMUX 1, level 31, sixdb 0, offset 8, AGC 1; level 31 is not a
   universal recommendation. Begin conservatively with an unfamiliar source.
4. Start capture with the existing capture button. Waveform and spectrum use
   the existing GUI visualization pipeline. Recording is native unsigned RAW8,
   one channel, stock nominal 28,636,363 Hz; resampling and audio are disabled.
5. Use the record button to start/stop saving; allow finalization to finish.
   Stop capture to release the card before opening the standalone CLI.

The launcher creates `build/misrc-gui-settings.json` only when absent, and saves
recordings under `artifacts/misrc-gui-recordings/` by default. Both are ignored.
Use the existing GUI output settings to choose another directory. Native RAW
recordings have `.u8` filenames. Existing settings are retained between launches.
Capture logs accompany recordings and include actual ADC settings and nominal
rate. Upstream logs can contain host/user information: do not publish them or
personal recordings without inspection. A GUI recording is not the CLI's JSON
sidecar format; use actual file length and logged settings for later decoding.

Direct launch, with the library installed beside the executable:

```sh
MISRC_GUI_NO_ADMIN=1 ../MISRC-GUI/build-local/misrc_gui \
  --config build/misrc-gui-settings.json
```

`MISRC_GUI_NO_ADMIN=1` bypasses upstream's administrator relaunch. This backend
uses the signed user-client entitlement and was tested as the normal user.

## Implementation

`client/cxadc_client.h` defines an optional, versioned C function table provided
by `build/libcxadc-client.dylib`. It discovers real PCI endpoints and uses the
existing DriverKit RPCs for configuration, status, start, pull and verified stop.
No new PCI, DMA or register code is introduced. A registry entry ID is a session
selector, not a persistent serial. The interface can enumerate multiple cards;
only single-card capture is implemented/tested in this GUI integration.

The isolated GUI macOS backend loads the library beside its executable or via
`CXADC_CLIENT_LIBRARY`. It stages the existing 256 KiB RPC result and serves
ordered 64 KiB chunks to the existing CXADC acquisition thread. The upstream
direct RAW writer receives those unsigned bytes before visualization packing.
Display conversion is separate and does not alter the saved stream. Driver
ownership prevents a competing process from configuring/stopping its stream.
Threads must stop/join before the C client is closed.

The existing server protocol was considered: its `/rf` path carries packed
internal display frames and its controls do not expose this ADC configuration.
A small native adapter reuses more of upstream's CXADC settings/RAW writer than
a new protocol server. No pipes, subprocess sample transport, Linux sysfs
emulation or embedded GUI source tree are used. Intended upstream submission is
separate from this local work; nothing has been pushed or submitted.

## Verification and limits

- Unmodified upstream arm64 build and smoke test passed before integration.
  `--smoke-test` exits before opening a window; it is not a visual test.
- Real device detection, OpenGL window initialization, RF/display worker activity,
  native RAW recording, ring wrapping and safe teardown were observed in logs.
- 10-second GUI recording: 286,523,392 bytes; 60-second: 1,718,091,776 bytes.
  RAW writer read/written byte counts matched, with zero reported writer drops
  or DMA overruns. Durations are GUI wall-clock limits with chunk/frame boundary
  granularity, not exact CLI sample limits. The clock remains nominal/unmeasured.
- Both files decoded with pinned VHS-Decode `177b505`, PAL/SVHS,
  `--frequency 28.636363 --use_saved_levels --threads 2`: 499 and 2,998 fields,
  respectively. Declared field counts, sequence continuity, alternating parity
  and TBC file sizes matched. No duplicate-field flags. Decoder dropout segments
  remain present; these are not a count of missing DMA samples.
- Hardware stop reported safe, no quarantine and restored PCI command 0 after
  both runs. These counters do not establish an authoritative zero-loss count.
- Existing deterministic direct-writer tests exercise unsigned bytes, ordering,
  wraparound, spill and final tails. Driver CPU/RISC/analysis tests also pass.
- Upstream settings-path, Hi-Fi monitor and preview-worker tests also fail on
  the untouched pinned revision on this machine; they are not fixed here.
  Upstream Darwin shared-memory creation failed with EINVAL and prevented RAW
  capture; the integration includes a narrowly scoped POSIX flag/descriptor fix.

No computer-use automation was used for acceptance. The user still needs to
verify the waveform's appearance and interact with the F8 panel/buttons. A
successful decoder run is not a fresh visual inspection of its video output.
Interactive window-close, physical unplug, full disk, suspend/resume and
multi-hour GUI recording are not hardware-validated. Source errors/timeouts,
native RAW buffer loss and write failures request visible capture stop; do not
consider an interrupted file complete. FLAC, 10-bit, alternate clocks, multiple
simultaneous cards, ClockGen and audio are outside this integration.

For offline inspection/decoding (requires the separate decoder environment):

```sh
python3 tools/analyze_u8.py recording.u8 --sample-rate 28636363
vhs-decode --frequency 28.636363 --system PAL --tape_format SVHS \
  --use_saved_levels --threads 2 recording.u8 decoded
python3 tools/check_decode.py decoded.tbc.json
```
