# First full-tape storage plan

2026-10-21. User estimates about 44 minutes and authorizes a folder on the
external T7. No full-tape capture has started. Current driver v8 is idle,
PCI command 0000, DMA off, no quarantine. The current workflow captures video RF
only; audio is not part of the implemented acquisition path.

## Footprint at 44 minutes

| Retained file | Decimal GB | GiB | Basis |
| --- | ---: | ---: | --- |
| Raw unsigned 8-bit RF |75.60 |70.41 |28,636,363 bytes/s × 2640 s |
| Luma TBC |93.79 |87.35 |1135×313×2 bytes × 50 fields/s × 2640 s |
| Chroma TBC |93.79 |87.35 |Same field dimensions |
| Corrected FFV1 MKV |~40.13 |~37.37 |Measured 59.96 s / 911,456,004-byte export extrapolated |
| Nominal total |~303.31 |~282.48 |Before logs/metadata/scratch and margin |

Raw and TBC estimates follow the configured rates/formats; actual decode
field counts depend on acquisition and boundaries. FFV1 size varies with
content/noise. Budget roughly 330 GB or more if keeping every file at once.
This excludes duplicate exports, extra comparison decodes and an audio workflow.

Initial read-only inspection found 171.6 GB /160 GiB free on T7 and about 81 GB
/76 GiB internally, insufficient in combination for all intermediates. During
preflight T7 free space increased to about 323 GB /301 GiB; a second df check
confirmed the folder remains on the real T7 APFS filesystem. No user files
were deleted by the agent. The larger reported free space is not used as a
reason to change the user's selected staged workflow.

## Selected workflow and authorization

The user chose to preserve the full raw capture and decode/export in sections
later, discarding generated TBC intermediates only after verification. Preserve
raw u8, capture metadata, source/decode logs, export metadata and final exports.
Do not delete raw or final exports. Generated section TBCs may be removed only
after validating their corresponding export and boundary continuity. No such
cleanup has happened yet.

At 44 minutes, raw plus estimated final FFV1 is about 116 GB. A five-minute
decode section adds about 21.3 GB of luma/chroma TBC scratch; budget 150–160 GB
working space with margin for this approach. This is an estimate, not a
validated chunk-export implementation. Section overlap, frame/field boundary
matching and continuity must be verified before deleting any intermediate.
The staged decode orchestration is planned, not yet implemented or tested.

## Prepared location and storage preflight

Created only this new project folder and its raw/logs/exports/scratch subfolders:

```text
/Volumes/T7/CXADC/2026-10-21-tape-01/
```

T7 inspection: mounted read/write APFS on an external USB solid-state disk.
No formatting, repartitioning, security change or alteration of existing files.
A temporary 256 MiB write plus fsync took ~0.347 s (~774 MB/s), against the
nominal 28.636 MB/s capture requirement. That generated I/O test is not RF data,
uses a small workload and does not prove 44-minute sustained performance. The
temporary file was automatically removed; its JSON result is saved in logs/.
Actual capture to the T7 has not yet been verified.

A separate logs/session-plan.json records a prepared, unstarted 45-minute capture
limit to allow approximately one minute beyond the user's tape-duration estimate:
77,318,180,100 raw bytes / 77.32 GB at the nominal rate. Candidate settings:
VMUX1, level 31, sixdb 0, offset 8, AGC 1, 64 MiB ring. Level 31 passed only five seconds
without clipping; representative long-run headroom and reliability are unverified.
A short real T7/gain preflight is appropriate before rewinding for the full run.
The driver default remains conservative level 0.

No full raw file or capture sidecar has been created. The saved plan argv is
reviewable; it does not start the engine. Capture still requires the tape to
be cued/playing and must be monitored for rate, errors and disk space.
This will be the first capture beyond one minute, not an already-proven
44-minute archival workflow.

## Subsequent real-capture preflight

The user then cued playback and authorized a test. A real 60-second recording
to T7 completed at level31 with the planned settings: 1,718,181,780 bytes,
codes 36..181, no rail samples, no reported DMA/overrun/write errors and verified
shutdown. The first five seconds decoded into 250 PAL S-VHS fields. Detailed
results and limitations are in TESTING.md. This supersedes the earlier
"not yet verified" T7/one-minute gain status above, but does not establish
full-tape reliability or clipping headroom for every tape section.

## Full capture complete; full decode now underway

The 45-minute raw recording completed: 77.32 GB, matching nominal byte count,
no reported DMA/overrun/write errors, verified shutdown. User subsequently
authorized immediate full decoding if space allows. Measured free space is
244.17 GB on T7 and 81.03 GB internally. Full-length TBCs (up to ~192 GB at
45 minutes) fit on T7 with headroom; expected ~41–50 GB final lossless export
fits internally. This supersedes the staged-only plan: no chunk trimming or
intermediate deletion is currently necessary.

`tools/decode_full_tape.py` is running under `caffeinate -i`, writes full TBCs
to T7 `scratch/tape01-pal-svhs-full`, and will automatically export standard
dropout-corrected FFV1 internally at
`artifacts/tape01-pal-svhs-full-dropout-corrected.mkv` after TBC byte validation.
It records commands/state in the adjacent `.workflow.json` and stops its child
if the active output filesystem drops below 20 GB free. It does not delete any
file. Full decode/export completion and visual review remain pending.

The decode subsequently completed with 134,826 fields and matching 95.80 GB
luma/chroma files. Its initial automatic export was blocked by an overly strict
seqNo check around two decoder-inserted replacement fields. Exact pinned-source
metadata reuse was verified; no JSON renumbering or sample edits were made.
`--export-existing` now resumes from those validated TBCs, preserves the original
failed state file and records a new `.export-workflow.json`. The full standard-DOC
FFV1 export is underway internally; no intermediate cleanup has occurred.
