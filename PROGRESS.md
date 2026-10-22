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
historical private author email/home paths still need an explicit publication
decision. No history rewriting or publication is part of this branch.
