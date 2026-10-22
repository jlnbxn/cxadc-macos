# Development milestones

The working implementation is preserved on `feature/cx23880-cxadc-macos`.
Publication preparation starts from `185490d90a70c6115a65ec83fe149d450d6f0242`
on `chore/public-release-preparation`. No remotes are configured and nothing
has been pushed. Historical development notes below are summarized by milestone;
[TESTING.md](TESTING.md) records current evidence and limitations.

| Commit | Milestone |
| --- | --- |
| `2d05277` | Actual host/PCI discovery and initial native DriverKit attachment |
| `ba0c3ae` | Bounded register reads with PCI command restoration; pinned references |
| `1b78b14` | Raw ADC readback and 32-bit device-visible DMA mapping lifecycle |
| `4ea0fd5` | Genuine raw DMA streaming, real interrupts and clean teardown |
| `8a4d842` | Sustained PAL CVBS capture and supported AGC control |
| `0e2abf5` | Recognizable colour CVBS export; external tools pinned |
| `c0aa158` | Genuine VHS playback RF capture and colour decode |
| `46cda36` | Full-minute S-VHS decode with supported saved-level acquisition |
| `f800853`, `f9794cd` | C31 removal reported; post-removal capture and matched comparison |
| `fca4225` | Gain sweep and operator's live SCART observations |
| `2bd20fb` | Repeatable RF dropout diagnosed; standard concealment verified |
| `095f29c`, `c6f5875` | External T7 preparation and genuine one-minute level 31 preflight |
| `39e6964` |45-minute capture verified; full decode launched |
| `e1773f4` | Recognize exact decoder filler metadata; resume DOC export |
| `185490d` | Full44:56.52 dropout-corrected FFV1 export and integrity verification |

The prior HVR-1200/CX23885 project was inspected read-only at
`a 4e28509264881b4d9b67ba285a0dcbb400265a4`. Its lifecycle/build patterns informed
the port; its register architecture was not reused. It remains a separate project
and is not a build dependency. No changes were made to it during release cleanup.

## Current verified boundary

Driver v8/API5 has captured raw unsigned 8-bit CVBS and VHS RF on the specific
matched endpoint. One 45-minute recording completed without reported capture
faults and decoded/exported successfully. This does not establish zero lost
samples, calibrated clock/gain, arbitrary hardware compatibility or archival
reliability. There is no audio capture or stdout stream.

## Next development work

- Independently measure sampling clock and analog input loading/levels.
- Validate repeated long runs and remaining recovery cases on a spare setup.
- Test properly provisioned installation on another Mac with normal security.
- Broaden hardware support only after verified endpoint-specific tests.

## Publication preparation

See [the publication audit](docs/PUBLICATION-AUDIT.md). Source history and the
verified driver implementation are preserved. Current documentation is sanitized;
the owner accepted retaining historical author identities and local paths after
reviewing the findings. No history rewriting or publication is authorized.

Preparation commits `13d499f` and `8ee9540` introduce sanitized public docs,
upstream notices, expanded ignore rules, portable external-tool paths and audit/
test commands. A clean source-only clone builds all native outputs and passes
ASan/UBSan plus 11 Python tests; strict signatures verify. Driver implementation
is unchanged apart from provenance comments and user-facing usage prose in the
Info.plist. Current source/link checks pass; historical privacy findings remain.
The card is not presently enumerated, so no fresh hardware capture is claimed.

## Native MISRC-GUI integration — 2026-10-22

The preceding enumeration statement describes publication preparation only. The
card was reattached for this milestone. Work is on
`feature/misrc-gui-integration`, preserving cleanup baseline `7fee31f` and the
existing CLI/DriverKit capture implementation. See [the integration guide](docs/MISRC-GUI.md).

- Separate GUI checkout: upstream `9c3894cd764db20c3eca835d041ad972cf986d37`,
  local `feature/macos-cxadc-backend`. Unmodified arm64 build/smoke established.
- Optional versioned C client library exposes existing discovery, settings,
  status and owned streaming RPCs. No low-level driver or DMA changes.
  Client milestone: `b8623c7`.
- GUI uses actual endpoint enumeration, stock RAW8, existing waveform/FFT
  workers and byte-preserving native RAW writer. F8 exposes ADC controls.
- Real GUI RF captures: 10 seconds (286,523,392 bytes) and 60 seconds
  (1,718,091,776 bytes). Both decoded as PAL SVHS, yielding 499/2,998 fields
  with matching TBC sizes and contiguous alternating fields. No newly inspected
  video export or visual GUI acceptance is claimed.
- RAW read/write counts matched; reported writer drops/DMA overruns were zero;
  safe teardown, no quarantine and PCI command restoration were observed. No
  authoritative zero-dropped-sample assertion. Minute-long file has no repeated
  256 KiB blocks, no samples at 0/255, range 36–187, mean 119.81, stddev 25.31.
- A concurrent CLI start was rejected as busy during another five-second GUI
  recording; the GUI continued and stopped safely. Media stays ignored/private.
- Client/CLI/DriverKit builds and driver CPU tests pass. GUI targeted RAW,
  unsigned mapping, Darwin ring lifecycle tests and static guards pass. Three
  existing GUI suite failures reproduce on untouched upstream; documented.
- GUI commits: `59909e4` (Darwin shared memory), `916efd3` (native backend,
  tests, controls and guide). Nothing pushed, published or submitted upstream.

The user subsequently launched the GUI, reported success and supplied their own
eight-second recording. Decode produced 400 fields; dropout-corrected FFV1 export
produced 200 frames/8 seconds with 19 reported concealments and passed a full-file
FFmpeg decode. See TESTING.md. This completes the first user-operated capture/
decode/export slice. Individual control acceptance, same-window repeated cycles,
active unplug, full disk and long GUI capture remain unverified. No computer use
was performed after the user's instruction. The executable remains ready via
`scripts/launch-misrc-gui.sh`. Upstream submission and distribution are separate;
an official unmodified MISRC-GUI release does not yet support this backend.
