# C31 removal: real RF comparison

Historical test report for the stated milestone. Later verification and current
limitations are summarized in [TESTING.md](../TESTING.md). Media and diagnostic
artifacts mentioned here are private, ignored local evidence, not repository files.

2026-10-21, DriverKit v8/API5, existing feature branch. The user reports C31
removed and card reattached. Read-only reattachment verification was committed
as f800853. The agent did not physically inspect or modify the board.

A new 60 s PAL S-VHS RF sample was captured with the same VMUX1, level 0, sixdb 0,
center_offset 8, AGC 1, nominal 28.636363 MSPS, unsigned 8-bit and 64 MiB ring settings.
The coax RF tap/68 nF series capacitor remained as previously reported. No ADC
gain increase, clock change, driver change, decoder patch or MP4 export.

## Capture and teardown

Actual output: 1,718,181,780 bytes, complete sidecar, 6,556 interrupt callbacks,
25 observed ring wraps and all 6,554 full 256 KiB blocks unique. No reported DMA
fault or overrun. Final status verifies running=0, video DMA=0, transfer status 0,
stop_verified=1, command restored 0000 and quarantine=0. There is no authoritative
dropped-sample count. No zero-loss assertion follows from these diagnostics.

Raw SHA256:

```text
254797bca1b0f1fb6a589bb3d76b03a2142cd2249b1ff550ca83fe74c2feee7d  rf-c31-removed-v8-60s.u8
```

## Decode and export

Reused the exact proven decoder profile:

```sh
./build/cxadc-capture --duration 60 --output artifacts/rf-c31-removed-v8-60s.u8 --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 1
MPLCONFIGDIR=build/mpl-cache build/decode-venv/bin/vhs-decode --frequency 28.636363 --system PAL --tape_format SVHS --use_saved_levels --threads 2 artifacts/rf-c31-removed-v8-60s.u8 artifacts/rf-c31-removed-v8-60s-svhs
python3 tools/check_capture.py artifacts/rf-c31-removed-v8-60s.u8
python3 tools/check_decode.py artifacts/rf-c31-removed-v8-60s-svhs.tbc.json
build/tbc-tools.app/Contents/MacOS/tbc-video-export --profile ffv1 --no-dropout-correct --threads 4 artifacts/rf-c31-removed-v8-60s-svhs.tbc artifacts/rf-c31-removed-v8-60s-export
```

Full raw file processed, decoder exit 0. There were 18 startup acquisition errors
before first lock at rounded source diskLoc 5.7 (~0.114 s). No subsequent
WARNING/ERROR entries. The output contains 2,994 PAL fields with contiguous
sequence numbers, alternating parity, no duplicate flags and no nonzero
decodeFaults bitmaps. Rounded neighboring source-location gaps are 0.9..1.1.
All fields report syncConf=45, the same heuristic score as the baseline.
Detector results: 319 dropout segments in 172 fields; median bPSNR 38.6. These
are decoder diagnostics, not calibrated SNR or lost DMA sample measurements.

Each luma/chroma TBC is 2,127,266,940 bytes, exactly matching the metadata.
Normal export completed successfully in ~13.9 s. It reports 1,497 frames; FFprobe
verifies 59.88 s, 928×576, yuv422p10le, 25 fps, top-field-first, FFV1 MKV 918,300,104
bytes. Dropout correction remained disabled. Source u8 is unchanged. The initial
acquisition exclusion is kept explicit; this is not 60 s of decoded output.

## Scene alignment and comparison

The new recording starts earlier than the baseline. To avoid comparing unrelated
content, normalized 32×32 block-mean luma features were extracted from the active
field crop (rows 22:310, columns 192:1088), sampled once per second. An integer
second offset search was refined to 0.02 s. The best offset is 4.44 s:

```text
before_elapsed_seconds = after_elapsed_seconds - 4.44
after_field_index = before_field_index + 222
```

Mean coarse-image correlation is 0.99991 over 55 sampled pairs. This is a
scene-alignment diagnostic, not a quality score or raw sample alignment. The
shared decoded interval contains 2,772 field pairs / 55.44 s. Matching exported
images at before 1/30/54 s and after 5.44/34.44/58.44 s were visually inspected.
The retained contact image has before on the top row, after on the bottom.
They show closely matching colour images and aligned picture boundaries.
Static inspection does not establish that rare temporal tears are absent.

| Measurement | C31 intact | C31 removed |
| --- | ---: | ---: |
| ADC range, entire minute |97..120 |97..120 |
| Mean one-second AC standard deviation |4.144 codes |3.864 codes |
| Rail samples (0 or 255) |0 |0 |
| Matched interval |2,772 fields |2,772 fields |
| Matched detector dropout segments |318 |295 |
| Matched fields with detected dropouts |173 |160 |
| Matched median bPSNR diagnostic |39.7 |38.6 |
| Matched duplicate flags/nonzero fault bitmaps |0 /0 |0 /0 |

Whole-minute AC standard deviation is about 6.8% lower after removal (-0.61 dB
ratio), but the initial/trailing scenes differ; this is not electrical gain or
SNR. Matched dropout segments decrease while the bPSNR diagnostic decreases.
These are mixed results, not clear evidence of a major quality improvement.
Two tape passes/reconnections can differ in tracking, head contact and wiring;
this experiment does not establish causation or explain the previously reported
rare tears. No claim is made that C31 removal increased ADC signal amplitude,
removed all tears, or established the need for an external amplifier.

## Retained results and scope

Ignored artifacts include the raw u8/sidecar, integrity and per-second level
reports, SHA256, decoder metadata/logs, scene-match report, matched diagnostics,
normal FFV1 export and rf-c31-matched-comparison.png. Existing baseline files
remain intact. Native driver code and CX23885 project remain unchanged. No media files are distributed with this report.

This verifies another real one-minute RF capture and recognizable S-VHS decode
after user-reported C31 removal. It does not prove archival-duration reliability,
calibrated clock/gain/SNR, zero sample loss, or a cure for the rare tears.
