# First genuine VHS RF capture and decode

Historical test report for the stated milestone. Later verification and current
limitations are summarized in [TESTING.md](../TESTING.md). Media and diagnostic
artifacts mentioned here are private, ignored local evidence, not repository files.

2026-10-21, branch feature/cx23880-cxadc-macos, existing driver v8 / API5.
No driver code change was needed after the successful CVBS milestone.

## Signal and hardware context

The user reconnected the OWC enclosure and reported a playing VHS tape in the
JVC HR-S7851EU, using its RF test point through coax to the CX23880 card's RCA.
A 68 nF ceramic capacitor is in series near the test point; C31 is intact.
Wiring was described as loose. These construction details are user-reported,
not electrically inspected or measured. Successful decoding does not establish
safe VCR loading, optimal coupling, impedance, frequency response or gain. No
physical modification, VCR adjustment or system-security change was performed.

Read-only status after reconnection confirmed driver 8, endpoint 14f1:8800,
master off, DMA off and no quarantine. Existing DriverKit attachment survived
idle unplug/replug; this is not an active-unplug safety test.

## Actual capture results

All runs used VMUX1, level 0, sixdb 0, center_offset 8, AGC enabled 1 (Linux default),
64 MiB ring and nominal 28.636363 MSPS unsigned 8-bit. No resampling, filtering,
compression, byte swapping or CVBS processing occurred in the capture path.

| Run | Raw bytes | IRQ callbacks | Observed wraps | Unique full 256 KiB chunks | ADC range |
| --- | ---: | ---: | ---: | ---: | --- |
| 1 s | 28,636,363 | 111 | 0 | 109 / 109 | 97..120 |
| 10 s | 286,363,630 | 1,094 | 4 | 1,092 / 1,092 | 97..120 |
| 60 s | 1,718,181,780 | 6,556 | 25 | 6,554 / 6,554 | 96..120 |

Each run exited 0 with complete metadata, byte agreement, no reported hardware
DMA error or consumer overrun, stop_verified=1, transfer status 0, PCIcommand
restored0000, DMA off, running=0 and no quarantine. No repeated full chunks were
found. RISC completion and chunk hashes expose several faults but cannot prove
zero sample loss; dropped_sample_count remains null. The 60 s run delivered
1,718,353,920 full-block bytes, writing the requested 1,718,181,780-byte prefix.
The final status is retained under artifacts/rf-final-v8-idle-status.json.

First-sample mean 110.989 codes, standard deviation 4.750 codes, no samples at
0 or 255. Previous disconnected baseline standard deviation was 0.104 codes;
this is a comparison, not a calibrated SNR measurement. A mean-subtracted
Hann-windowed FFT (65,536 samples per averaged block, nominal clock) has a
3–5 MHz band peak at 4.29484 MHz, ~93.75% of non-DC FFT power in that band,
and a 100–900 kHz band peak at 623.10 kHz. These diagnostics suggested RF content;
actual VHS decoding and recognizable output provide the stronger evidence.
The small ADC span leaves room for later gain evaluation. No gain increase
was needed for the first decoded picture. The stock oscillator is not measured.

## Reproduction of the tested commands

Output files must not already exist. Capture duration was increased only after
the earlier run succeeded:

```sh
./build/cxadc-capture --duration 1 --output artifacts/rf-first-v8-1s.u8 --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 1
./build/cxadc-capture --duration 10 --output artifacts/rf-v8-10s.u8 --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 1
./build/cxadc-capture --duration 60 --output artifacts/rf-v8-60s.u8 --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 1
python3 tools/check_capture.py artifacts/rf-v8-60s.u8
python3 tools/analyze_u8.py artifacts/rf-first-v8-1s.u8 --sample-rate 28636363

MPLCONFIGDIR=build/mpl-cache build/decode-venv/bin/vhs-decode --frequency 28.636363 --system PAL --tape_format VHS --threads 2 artifacts/rf-v8-10s.u8 artifacts/rf-v8-10s-decoded
build/tbc-tools.app/Contents/MacOS/tbc-video-export --profile ffv1 --no-dropout-correct --threads 4 artifacts/rf-v8-10s-decoded.tbc artifacts/rf-v8-10s-export
```

The one-second file was decoded/exported identically using its own prefix.
Installed/pinned VHS-Decode help verified the MHz frequency, PAL system and VHS
format arguments before execution. Decoder revision 177b505 and external TBC Tools
v3.3.0 release/digest are recorded in references/manifest.json. The decoder and
exporter perform offline signal processing; original raw files are unchanged.

## Decode and visual verification

The 1 s RF input produced 48 PAL fields; the 10 s input produced 498 PAL fields,
with zero duplicate-field flags in either JSON. The short run has syncConf=45
throughout; the 10 s run has 494 fields at 45 and 4 at 100. This decoder output does
not provide a decodeFaults key, so no zero-decode-fault assertion is made. No
WARNING/ERROR entries were found in these decoder logs. Start/end handling may
exclude fields; decoded field count is not an authoritative DMA-loss measurement.
The tape was decoded as PAL VHS; no SVHS/RF-format comparison was performed.

Normal export merged the decoder's separate luma/chroma TBC intermediates, using
automatic MONO+PAL2D and automatic TFF field order, with dropout correction off.
The intermediate Y+C designation does not mean the physical input was S-Video.
FFprobe verified 249 frames, 928×576, yuv422p10le, 25/1 fps, top-field-first, 9.96 s,
FFV1 MKV 144,105,064 bytes. A one-second export and frames near 1/5/9 s in the
ten-second export were visually inspected: recognizable colour PAL source video
with remaining noise/artefacts, not merely a histogram or zero-exit decoder run.

Local, ignored artifacts:

- artifacts/rf-first-v8-1s-preview.png — first real RF decoded image.
- artifacts/rf-v8-10s-contact.png — three inspected frames across 10 s.
- artifacts/rf-v8-10s-export.mkv — normal colour video export.
- artifacts/rf-v8-60s.u8 and .u8.json — one-minute genuine RF stream and metadata.

Raw SHA256 provenance:

```text
4a9a11b09ce1a86d4dd0dd824dab48b1d28625e202b0367403bc9ea1991ae165  rf-first-v8-1s.u8
475b46cc11d233f3196bdbed1dd1890aec575c8d738192641a5b283419461a2a  rf-v8-10s.u8
ec7574456a141448aa58fb8994de9ed8e504a2788e3c83b2755cd73a3975a1f7  rf-v8-60s.u8
```

External analysis/export dependencies are not part of the native driver build.
No media, binaries or reference checkouts are committed.

## Scope of success and remaining work

The primary short-duration path is demonstrated: native macOS DriverKit captures
genuine playing VHS RF through the OWC enclosure into raw u8, and VHS-Decode
produces sensible colour PAL video. A minute of raw RF streaming also completed.
The full 60 s file has not been decoded; 1 s and 10 s were decoded. Multi-minute
and 30–60-minute reliability, accurate loss detection, calibrated clock/levels,
optimal analog quality, loading, forced sleep/wake, disk-full and active hot-unplug
remain unverified. No physical changes or higher gain should be inferred necessary
from this first result; assess them as separate, measured follow-up work.
