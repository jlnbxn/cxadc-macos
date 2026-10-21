# Diagnosis of the screenshot's horizontal streak

2026-10-21. The user supplied a screenshot from the first minute-long decoded
S-VHS recording and reported occasional similar artifacts absent from normal
SCART capture. This investigation used existing saved data only: no hardware
write, new capture, gain change, physical modification or MP4.

## Exact frame and metadata

Visual luma-feature matching located the screenshot near 25.12 s in
rf-second-v8-60s-svhs-export.mkv. The decoded frame was extracted and manually
checked: the subject position and bright horizontal streak match the screenshot.
The original export deliberately disabled dropout correction for hardware
comparison (`--no-dropout-correct`). RF dropout detection was still enabled.

The frame contains zero-based field indices 1256/1257 (metadata seqNo 1257/1258).
Relevant detector metadata in nearby fields:

| Before field index | C31-intact dropout coordinates (field line, startx..endx) | Matched C31-removed field index / coordinates |
| --- | --- | --- |
| 1255 |141,27..867 |1477:141,21..873 |
| 1256 |142,1092..1135;143,0..869 |1478:142,1084..1135;143,0..866 |
| 1257 |143,1094..1104;144,53..787 |1479:144,33..706 |

The same source-tape moment appears at 29.56 s in the post-C31 export, using
the previously established 222-field / 4.44 s alignment. Its extracted frame
shows the same bright streak, and detector coordinates closely agree. Nearby
fields 1254 and 1258..1261, and their matched counterparts, have no flags.
Different playback timestamps in these files do not mean a different tape
moment. This repeatability strongly supports a tape/playback-related RF dropout
over random loose-connection or DMA corruption as the explanation for this
particular example. It does not identify a specific physical tape defect,
exclude marginal analog capture, or diagnose every artifact in every file.

## Corrected export test

A separate normal FFV1 export used standard dropout correction, retaining the
original raw/TBC data and uncorrected export:

```sh
build/tbc-tools.app/Contents/MacOS/tbc-video-export --profile ffv1 --threads 4 artifacts/rf-second-v8-60s-svhs-levels.tbc artifacts/rf-second-v8-60s-dropout-corrected
```

Exit 0. The exporter reports 346 dropout concealments and 1,499 frames. FFprobe
verifies 59.96 s, 928×576, 25 fps, top-field-first, FFV1 MKV 911,456,004 bytes.
The exact 25.12 s frame was extracted from both versions and visually inspected:
**the bright streak is absent from the corrected version**. This is concealment
using surrounding picture information, not recovery of original missing detail
and not proof that every defect has been removed.

Retained ignored artifacts:

- dropout-before-25_12.png — original diagnostic export.
- dropout-after-25_12.png — standard dropout-corrected export.
- dropout-before-after-25_12.png — original left, corrected right.
- dropout-post-c31-matched.png — same defect on the second tape pass.
- dropout-matched-field-diagnostics.json — both sets of detector coordinates.
- rf-second-v8-60s-dropout-corrected.mkv — corrected full-minute viewing version.

Original raw SHA256 was rechecked against the recorded baseline:

```text
2987f8ade40f05c524793cce0767d55eda2d14b8a00542286a3cc20a54f06bb8  rf-second-v8-60s.u8
```

## Interpretation and normal use

The VCR's conventional output includes its own video processing and may conceal
dropouts that remain exposed in an uncorrected RF export. That is a plausible
explanation for clean SCART video here; the specific internal JVC correction
path has not been instrumented. Standard software dropout correction now
visibly conceals the pictured defect using the already captured data.

An amplifier can still be evaluated for RF loading/noise and marginal signal
recovery, but this screenshot is not evidence that one is required. It cannot
guarantee recovery of genuinely missing tape information. Do not attribute this
example solely to the small ADC span, C31 or loose cables. Other streaks/tears
should be matched by timestamp and checked separately if they survive correction.

For hardware diagnosis retain uncorrected exports. For ordinary viewing use
tbc-video-export without `--no-dropout-correct`, its standard default. Detection
and correction are different stages; no driver-side processing is introduced.
Technical background: the current VHS-Decode
[FAQ](https://github.com/oyvindln/vhs-decode/wiki/FAQ) and
[command list](https://github.com/oyvindln/vhs-decode/wiki/Command-List) describe
processed VCR output, RF-envelope dropout detection and later correction.
