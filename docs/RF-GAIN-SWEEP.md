# RF gain sweep with live SCART observation

Historical test report for the stated milestone. Later verification and current
limitations are summarized in [TESTING.md](../TESTING.md). Media and diagnostic
artifacts mentioned here are private, ignored local evidence, not repository files.

2026-10-21, existing DriverKit v8/API5, C31 user-reported removed. The user
played the PAL S-VHS tape and watched the VCR's SCART output during testing.
The RF tap, 68 nF series capacitor, RCA/VMUX1, offset 8, AGC 1, nominal
28.636363 MSPS unsigned 8-bit and 64 MiB ring were unchanged. No new physical,
security or driver-code changes. Sixdb stayed 0 for every test.

## Actual raw captures

Each gain setting was announced before its five-second capture. The initial
sweep 0/4/8/16 remained far from clipping; after the user reported no visible
SCART disturbance, the sweep extended to documented levels 24 and 31.

| Level | ADC minimum | ADC maximum | Peak-to-peak span | AC standard deviation | Rail samples |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 | 99 | 119 | 20 | 3.463 | 0 |
| 4 | 96 | 122 | 26 | 4.485 | 0 |
| 8 | 93 | 125 | 32 | 5.707 | 0 |
| 16 | 82 | 135 | 53 | 10.120 | 0 |
| 24 | 64 | 154 | 90 | 17.282 | 0 |
| 31 | 36 | 182 | 146 | 28.338 | 0 |

All six raw files contain 143,181,815 bytes, complete sidecars, 546 unique full
256 KiB blocks, 548 interrupt callbacks and 2 observed wraps each. No reported
DMA error or overrun. All stops verified, transfer status 0, PCI command restored
0000, no quarantine. There is no authoritative dropped-sample count, and no
zero-loss claim. Raw files contain genuine ADC data, without byte scaling,
filtering, resampling or other capture-side transformations.

The level 31 sample spans 146 codes (about 57% of the full 255-code span), versus
20 codes at level 0 in this sweep. It has 36 codes of lower-rail headroom and 73
codes of upper-rail headroom in these five seconds. Different tape scenes can
have different RF envelopes/transients; this is not a universal clipping margin.
The gain setting is a register control value, not a calibrated dB scale.

## SCART observations and their limits

The user separately confirmed no visible SCART picture change during:

- Levels 0, 4, 8, 16, including capture start/stop.
- Additional levels 24 and 31.

This is the user's live observation, not an instrument measurement. The RF tap
remained connected throughout. Therefore the experiment does not compare a
connected versus disconnected tap and does not establish zero electrical loading
of the VCR head amplifier. It records no visible disturbance associated with
the capture/gain sequence.

## Decoder and exported pictures

Levels 0, 16, 31 were each decoded in full using the same pinned software and flags:

```sh
./build/cxadc-capture --duration 5 --output artifacts/rf-gain-v8-level31-5s.u8 --vmux 1 --level 31 --sixdb 0 --center-offset 8 --agc-enable 1
MPLCONFIGDIR=build/mpl-cache build/decode-venv/bin/vhs-decode --frequency 28.636363 --system PAL --tape_format SVHS --use_saved_levels --threads 2 artifacts/rf-gain-v8-level31-5s.u8 artifacts/rf-gain-v8-level31-decoded
python3 tools/check_decode.py artifacts/rf-gain-v8-level31-decoded.tbc.json
build/tbc-tools.app/Contents/MacOS/tbc-video-export --profile ffv1 --no-dropout-correct --threads 2 artifacts/rf-gain-v8-level31-decoded.tbc artifacts/rf-gain-v8-level31-export
```

The other two decodes/exports used their own level-specific prefixes. All three
finished with 248 PAL fields, contiguous sequence, alternating output parity,
no duplicate flags or nonzero fault bitmaps, and no decoder WARNING/ERROR log
entries. TBC byte counts match metadata. No full decodes of levels 4/8/24 were
needed for this bounded low/mid/high screening.

| Level | Decoder dropout segments | Fields with detected dropouts | Median bPSNR diagnostic |
| --- | ---: | ---: | ---: |
| 0 | 32 | 20 | 38.6 |
| 16 | 18 | 8 | 40.0 |
| 31 | 23 | 13 | 40.6 |

The clips contain successive, different tape content. These diagnostics are
not a scene-matched noise/quality comparison; bPSNR is not calibrated analog
SNR. Neither increasing ADC span nor these metrics prove improved effective
resolution, elimination of rare tears, or superiority on every tape.

Normal FFV1 exports for all three completed. Static colour frames near 2 s were
visually inspected and show recognizable S-VHS source video. The preview contact
image is ordered level 0/16/31 left-to-right; it is not a matched-scene comparison.
For level 31, the exporter reports 124 frames and FFprobe verifies 4.96 s, 928×576,
yuv422p10le, 25 fps, top-field-first, FFV1 MKV 68,231,977 bytes. No dropout correction
was applied and no MP4 was created.

## Result and next validation

Level31/sixdb 0 is the candidate for a longer trial on this tape: substantially
more code range, no clipping in the five-second sample, and successful decoding.
This is not yet an archival-duration or representative-tape qualification at
that setting. A repeated matched section at level 0/31 would be needed to
substantiate a picture/noise improvement. An external amplifier is not shown
necessary merely to expand code range; a properly configured buffer's loading
and noise benefits remain a separate hardware question.

After the sweep the reference idle settings were restored: level 0, VMUX1,
sixdb 0, offset 8, AGC 1. Live status confirms command 0000, DMA off, running=0,
stop_verified=1 and quarantine=0. No compiled defaults were changed. Internal
SSD has 76 GiB free. The CX23885 repository remains unchanged.

All raw files, metadata, integrity reports, decoder outputs/logs, FFV1 exports
and rf-gain-v8-previews.png remain ignored under artifacts/. Raw provenance:

```text
473f1bd037df5dca939827be9f854be061af4de81deedc4f51505db85d07b635  rf-gain-v8-level0-5s.u8
3f094d87cb2146cb9c10aadd615abcd9ca4db05f2a425c1365bee51869b31e01  rf-gain-v8-level4-5s.u8
f46bd649db6697f545d325013c8426c9e7b09a76d505856409ec0ae51d5943dd  rf-gain-v8-level8-5s.u8
b3d4ef540998b22b827fc278aa23c598ef608d35e1b0d819b414eb343226fc1f  rf-gain-v8-level16-5s.u8
ee542ecaa17e0f08bbe9818eb1be2cca3fd068a1b65aa4f14fc05f8d590dc6d5  rf-gain-v8-level24-5s.u8
337dca6ce74b7d2e2d50d75de4dc9e31f0fe424047b2cfe23233a0319b335852  rf-gain-v8-level31-5s.u8
```
