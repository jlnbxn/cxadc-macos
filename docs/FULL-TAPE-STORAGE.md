# Capture and decode storage

At nominal 28.636363 MSPS, unsigned 8-bit raw data uses28.636363 MB/s (decimal).
Metadata and logs are small compared with raw, TBC and export files. Allow space
for partial outputs and working headroom; compression ratios vary with content.

| Files for44 minutes | Approximate decimal GB |
| --- | ---: |
| Raw `.u8` |75.60 |
| Luma TBC,1135×313×16-bit at 50 fields/s |93.79 |
| Chroma TBC |93.79 |
| Lossless FFV1 export, extrapolated from early test |40.13 |
| All files together, before margin |303.31 |

Budget around 330 GB or more for that complete workflow. These estimates do not
include audio, duplicate comparison decodes or backups.

## Verified full-tape workflow

One 45-minute recording was saved to an external T7 APFS SSD and decoded fully.
It retained77.32 GB raw and 191.59 GB of TBCs on the external drive, with the40.07 GB
lossless export on the internal SSD. Export duration was44:56.52 after acquisition
and boundary handling. No raw samples or generated intermediates were deleted.

The small initial temporary256 MiB write/fsync benchmark measured about 774 MB/s;
it was only storage I/O, not a simulated capture or proof of long-run reliability.
A subsequent genuine60 s preflight and the45-minute RF capture provided actual
capture evidence. Historical free-space readings are not portable requirements.

## Optional workflow helper

`tools/decode_full_tape.py` supports the tested stock-rate PAL S-VHS workflow:

- Requires complete raw capture metadata and a matching byte count.
- Accepts decoder/exporter paths; see `--help` and [BUILDING.md](../BUILDING.md).
- Refuses output overwrite and records exact commands/state.
- Validates TBC byte counts, parity and field metadata before export.
- Enables standard dropout correction through the exporter default.
- Stops the child process if the active output volume falls below 20 GB free.
- Keeps raw, TBCs, metadata, logs and final outputs; no automatic cleanup.

`--export-existing` resumes from validated TBCs without repeating decode and
retains the previous workflow record. The guard reduces risk but does not replace
OS write-error handling or reserved space; its low-space path is not hardware
validated. Prevent idle sleep separately for long offline jobs.

A staged five-minute decode can reduce TBC scratch to about 21.3 GB while retaining
the full raw and final exports. Chunk-boundary overlap/continuity handling has
not been implemented or validated here. Do not discard intermediates until the
corresponding export and continuity have been checked. Private media is never a
suitable public test fixture.
