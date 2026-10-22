# Publication-readiness audit

Audit date:2026-10-22. Scope: source cleanup and publication preparation only.
No repository was created remotely, no push/release/visibility change occurred,
and no history, commit identity or existing branch was rewritten.

## Starting point and recoverability

- Clean starting tree: `185490d90a70c6115a65ec83fe149d450d6f0242`.
- Original branch preserved: `feature/cx23880-cxadc-macos`.
- Preparation branch: `chore/public-release-preparation`.
- Initial reachable history:17 commits,141 distinct source blobs, no tags/remotes.
- Largest historical blob:27,388 bytes; no historical binary blob or Git submodule.
- Separate HVR-1200 project remains at its original commit, clean and unchanged.
- Driver/CLI hardware behavior and entitlements are preserved. Native source
  edits are provenance comments; Info.plist usage prose now describes raw capture.

The existing feature branch and full history retain the working baseline.
Local raw RF/video/preview data, generated builds and upstream checkouts were
not deleted, moved or staged. This is not a destructive disk cleanup.

## Privacy findings: preserved history accepted

The owner subsequently accepted retaining the authentic history, including local
paths, after reviewing these findings. No history rewrite is requested. This
acceptance does not authorize publication, a push, or a visibility change.

**The latest source snapshot is sanitized; the preserved history is not private.**
Removing local paths from current files does not remove older copies from Git.
All 17 starting commits contain the configured personal author/committer name and
email. New preparation commits retain the existing configured identity rather
than silently changing authorship. Those identities will be public if this
history is pushed, including when only the preparation branch is pushed.
No email value or private home-directory value is repeated in this public report.

| Historical file or metadata | Finding | Affected starting commits |
| --- | --- | --- |
| Author/committer metadata | Personal identity and non-noreply email | All 17 starting commits; also preparation commits using existing identity |
| `PROGRESS.md` | Personal macOS home directory | All 17 starting commits, beginning `2d05277` |
| `PROGRESS.md` | Local volume/session layout | `095f29c`, `c6f5875`, `39e6964`, `e1773f4`, `185490d` |
| `TESTING.md` | Local volume/session layout | `c6f5875`, `39e6964`, `e1773f4`, `185490d` |
| `docs/FULL-TAPE-STORAGE.md` | Local volume/session layout | `095f29c`, `c6f5875`, `39e6964`, `e1773f4`, `185490d` |

All 17 starting commit IDs appear in [the milestone table](../PROGRESS.md).
The ignored local audit JSON maps each affected historical blob to all commit
IDs. To repeat the value-redacting source/history checks:

```sh
python3 scripts/audit-publication.py
python3 scripts/audit-publication.py --history
```

The current-source check should pass. The history check intentionally returns
nonzero for preserved historical path findings. It separately lists commits whose
identity metadata needs review, without printing names/emails or matched values.
These heuristic checks do not certify absence of every possible secret.

### Recorded publication options

The owner selected retaining history (option 1). Actual publication still needs
separate authorization.

1. Keep/publish history as-is, explicitly accepting the recorded identity and
   local-path exposure. Changing a GitHub email setting does not alter old commits.
2. Approve a separate, narrowly scoped history-sanitization operation covering
   all affected refs, file contents and selected author/committer fields. It would
   change commit IDs and needs an owner-provided replacement identity and private
   backup. This task has not performed or authorized it.
3. Approve publishing a sanitized source snapshot with a new public history while
   retaining the complete development repository privately. This preserves the
   local history but not the original public ancestry; it is a separate decision.

Until a choice is made, **do not publish this preserved history as privacy-clean**.
No actual credential requiring revocation/rotation was identified by this audit.

## Audit coverage and exclusions

Reviewed all reachable branches/tags/commit snapshots, messages and identities,
tracked/nonignored files, local build configuration, scripts, entitlements,
project references, license texts and the pinned hardware-reference sources.
Checks include common token/key/password markers, signing asset names, emails,
phone candidates, UUIDs, private paths, binary/oversize files and Git links.
Findings were manually reviewed; upstream author contacts in retained notices
are intentional attribution, not local secrets.

Ignored local storage includes approximately 70 GB of private artifacts,1 GB of
build/dependency outputs and334 MB of reference working-tree files at initial
inventory (excluding each repository's Git object database). Generated logs/config
were also scanned for likely credential markers. Local logs contain private paths,
UUIDs and machine identifiers; they remain ignored and must not be uploaded.
Private media was classified by location/type/size and was not copied into public
fixtures. No photographs or screenshots were added, so no image metadata is
redistributed. No exhaustive forensic inspection of every third-party binary or
private video frame is claimed.

Five nested upstream Git directories exist locally under ignored `references/`;
none is a tracked gitlink or embedded repository. Only the manifest is tracked.
Do not publish a filesystem ZIP of the workspace: use reviewed Git content.
The ignore rules also cover Xcode user state, builds, media, logs, environment
files and signing assets, while keeping project source and entitlement plists.

## Provenance and licensing

Retain GPL-2.0-or-later, based on actual adapted Linux configuration/register/RISC
work and the existing SPDX declarations. LICENSE is unchanged. Required upstream
copyright/license/warranty notices are retained in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md), with source locations and
modification scope. MIT/BSD relicensing is not appropriate for this adaptation.

Windows code was a technical cross-check, not imported WDF implementation.
VHS-Decode/TBC Tools run separately; their GPLv3 code and vendor binaries are not
embedded in the native targets. Apple SDK headers/frameworks and generated IIG
code are external build inputs, not committed source. The earlier macOS source
has GPL-2.0-or-later labels but no complete ownership/third-party provenance
record was supplied with it; owner confirmation remains appropriate before
publication. No incompatible embedded license was identified.

A future signed binary release requires a separate Apple entitlement/signing/
notarization and corresponding-source review. This audit does not approve a
binary release or invent a GPL linking exception. See [source references](REFERENCES.md).

## Organization and verification

The existing source layout is retained: driver, native CLI, shared core, installer,
scripts/tests/tools, docs and reference manifest. README now describes verified
versus intended support, actual CLI commands and honest deployment limits.
BUILDING documents the exact local build and the unvalidated distribution path.
HARDWARE preserves PCI/BAR/clock/tap/C31 evidence, including operator-reported
construction details. Historical reports are labeled and private storage paths
removed from the current documentation. Public sample footage is not included.

Release checks so far:

| Check | Result |
| --- | --- |
| Native arm64 CLI | Build passed |
| DriverKit extension and local host | Build/signature checks passed; no activation |
| ASan/UBSan RISC/ADC tests | Passed |
| Python diagnostics/path tests |11 passed |
| CLI help/list | Interfaces intact; endpoint not presently enumerated |
| Current hardware status | Driver not attached; no new capture/ADC/DMA test attempted |
| Native configuration/logic comparison | Hardware source changes limited to comments; usage-description metadata only |
| Current source secret/path/binary audit | Passed, including staged/current source; heuristic and manual review |
| Markdown local links | Passed; main external project links also resolved |
| Clean source-only build | Passed: arm64 CLI, DriverKit extension, host and all CPU tests |

The clean local clone at preparation commit `8ee9540` included no capture data,
reference checkout or existing build output. All three native outputs built from
that source and passed strict signature verification where signed. CLI help was
byte-identical to the working-tree CLI's help. All 14 native implementation
files compare identically to `185490d` after removing line comments; build
configuration/entitlements are unchanged. LICENSE is byte-identical to baseline.
Only documentation/verification reporting changed after this clean-build check.

Preparation commits:

- `13d499f`: public documentation, privacy cleanup, ignore rules and provenance.
- `8ee9540`: portable offline-tool lookup, CPU test runner and redacting audit.
- The commit containing this final report records clean-checkout verification.

Current-source checks are clean. The all-history scan continues to identify the
deliberately preserved historical paths and identity metadata described above.
No remote/tag was added, no force operation occurred and the original feature
branch still points to the verified starting commit.

The cleanup does not modify/reinstall the active driver or change OS security.
Prior 45-minute evidence remains documented, but no new hardware regression is
claimed while the card is absent. CI was considered; no hosted DriverKit/signing
job is added without validating SDK availability. The dependency-free CPU test
script and audit command are ready to integrate into CI later.

## Assessment

Source snapshot preparation and clean-build verification are complete. The owner
accepted the preserved history's identity/path exposure. Publication still needs
explicit authorization and the stated provenance review. Normal-security installation and public binary
packaging are not verified. No publishing permission has been requested or used.
