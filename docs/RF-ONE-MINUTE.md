# One-minute PAL S-VHS RF and time-base-correction test

2026-10-21, driver v8 / API5. Same general raw ADC engine and conservative settings
as the first RF test: VMUX1, level 0, sixdb 0, offset 8, AGC 1, 64 MiB ring, nominal
28.636363 MSPS. The user confirmed this new section as PAL S-VHS/S-VHS ET.
The RF tap/coupling and intact C31 are described in RF-FIRST-CAPTURE.md. No
physical or driver changes were made.

## Capture and storage

Internal SSD had 98 GiB available before capture. The actual 60 s stream contains
1,718,181,780 unsigned 8-bit samples/bytes. Metadata agrees; all 6,554 full 256 KiB
blocks have unique hashes. There were 6,556 interrupt callbacks, 25 observed ring
wraps, no reported DMA errors or overruns, verified stop, PCI command 0000,
video DMA off, transfer status 0 and no quarantine. Counter/hash checks do not prove
zero sample loss; authoritative dropped_sample_count remains unavailable.

ADC range 97..120, mean 111.0048, no rail clipping. Across nominal one-second
windows, standard deviation spans 3.885..4.326 codes. This is still a small
signal span; no higher internal gain or external amplifier was tested here.
The first-second averaged FFT has a 5–8 MHz peak near 6.55172 MHz, consistent
with using the S-VHS decoder parameters rather than the previous VHS setup.
A separate diagnostic FM demodulation detects approximately 64 us line pulses
throughout sampled sections, including the beginning. These estimates assume
the nominal clock and are not calibrated measurements or loss detection.

Raw SHA256:

```text
2987f8ade40f05c524793cce0767d55eda2d14b8a00542286a3cc20a54f06bb8  rf-second-v8-60s.u8
```

## Decoder acquisition investigation

A VHS-mode run was started before format clarification, then intentionally
interrupted when the user confirmed S-VHS/S-VHS ET. Its partial output is not
a successful final decode. The original raw file remains complete and unchanged.
Default S-VHS decoding processed the complete raw file but skipped the first
~7.712 s, producing 2,614 fields starting at source diskLoc 385.6. A bounded
S-VHS ET probe also acquired at 385.6, so switching that filter variant did not
recover the beginning. These are offline decoder acquisition failures, not
a measured count of missing DMA samples.

The pinned source's field.py/get_pulses estimates sync/blanking levels from
percentiles by default. The documented `--use_saved_levels` option permits
reuse of levels, beginning with the format defaults, with re-detection on
problems. A bounded S-VHS probe with that option acquired at source diskLoc 1.8
(~0.036 s), decoded 20 fields without errors, and produced recognizable colour
video from the previously skipped beginning. No upstream patch, driver change,
raw-file modification, forced field duplication or invented register setting
was needed. A full rerun with that same supported option follows.

## Reproduction

```sh
./build/cxadc-capture --duration 60 --output artifacts/rf-second-v8-60s.u8 --vmux 1 --level 0 --sixdb 0 --center-offset 8 --agc-enable 1
python3 tools/check_capture.py artifacts/rf-second-v8-60s.u8
MPLCONFIGDIR=build/mpl-cache build/decode-venv/bin/vhs-decode --frequency 28.636363 --system PAL --tape_format SVHS --use_saved_levels --threads 2 artifacts/rf-second-v8-60s.u8 artifacts/rf-second-v8-60s-svhs-levels
python3 tools/check_decode.py artifacts/rf-second-v8-60s-svhs-levels.tbc.json
```

Capture outputs refuse overwrite. Decoder/export binaries and source revisions
are the same pinned versions recorded in references/manifest.json. Time-base
correction is performed by VHS-Decode offline; the card produces only raw ADC
bytes. TBC intermediates are uniform 16-bit PAL luma/chroma fields, and exports
are separate derived files. The diagnostic tool checks field sequence, byte
counts, parity, decoder flags and rounded input-location gaps. Those diagnostics
do not certify perfect correction, calibrated SNR or zero hardware sample loss.

## Verified full-minute result

The complete S-VHS rerun with `--use_saved_levels` exited 0 and produced 2,998 PAL
fields. Its log has no WARNING/ERROR entries. Metadata sequence numbers are
contiguous, output parity alternates, no duplicate-field flags or nonzero
decodeFaults bitmaps are present. Rounded source diskLoc progresses from 1.8 to
2998.5, with neighboring gaps 0.9..1.1 and none outside 0.8..1.2. Default decoding
had discarded 7.7 s; this supported option recovered the beginning without
changing the original ADC bytes. The decoder's format-derived initial levels
and subsequent level reuse were sufficient for this test.

Luma/chroma TBC files each contain 2,130,108,980 bytes, exactly matching 2,998
1135×313 unsigned 16-bit fields. All fields have syncConf=45; this is the pinned
decoder's line-timing heuristic, not a 45% success rate or calibrated quality
metric. It detected 364 dropout segments across 199 fields. These can reflect
tape, RF path and detector behavior; they are not 364 lost DMA samples. Median
bPSNR 39.7 is retained only as an uncalibrated decoder diagnostic. No claim of
perfect time-base correction, optimal analog quality or zero sample loss.

Normal interlaced export completed successfully using official TBC Tools v3.3.0:

```sh
build/tbc-tools.app/Contents/MacOS/tbc-video-export --profile ffv1 --no-dropout-correct --threads 4 artifacts/rf-second-v8-60s-svhs-levels.tbc artifacts/rf-second-v8-60s-svhs-export
```

The exporter reports 1,499 frames. FFprobe verifies 59.96 s, 928×576, yuv422p10le,
25 fps, top-field-first, FFV1 MKV 911,492,237 bytes. Dropout correction was disabled
so the comparison does not conceal detected defects. The physical input remains
RCA RF; the exporter's Y+C label describes the decoder's two intermediate files.
Colour frames near 1/30/58 s were visually inspected and show recognizable video
with aligned line/frame boundaries. Static samples do not validate every moment
of motion. The user separately reports markedly improved stability over their
conventional S-Video capture, with occasional small tears. We have not correlated
those tear timestamps with decoder diagnostics, so weak signal is not established
as their cause.

The final MKV and contact image are under ignored artifacts/. The user cancelled
MP4 creation; no MP4 was generated and no export process remains running. The
SSD has 85 GiB available after retaining raw, comparison decodes and final outputs.
The existing CX23885 repository remains clean and unchanged.

## Next controlled comparison

The user intends to remove C31 manually, with the enclosure powered off and
disconnected, then replay approximately this section. Removal is pending, not
verified. Keep the RF tap, 68 nF coupling capacitor, VMUX1, level 0, sixdb 0, offset 8,
AGC1, nominal clock and `SVHS --use_saved_levels` settings unchanged for the first
comparison. Verify the specific board against the documented white-card C31
layout. The pinned CXADC wiki explains that C31 bridges VMUX0/VMUX1 and connects
extra termination/filtering; the current VHS-Decode CX-Cards guide agrees.
This is an analog-path comparison, not evidence that C31 causes the observed
tears or that an external amplifier is necessarily required. Compare level span,
clipping, dropout/sync diagnostics and matched scene stability before changing
gain or adding an amplifier.

