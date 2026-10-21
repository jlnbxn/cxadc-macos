# Progress

Branch: `feature/cx23880-cxadc-macos`. Workspace was empty and had no Git history.
No remote configured; no code published. The HVR-1200 repository at
`/Users/julianboxan/Developer/hvr1200-macos` was inspected read-only at
`a4e28509264881b4d9b67ba285a0dcbb400265a4` and remains unchanged.

Milestone 1: actual host/PCI discovery, isolated DriverKit target, native enumeration
CLI and local host build. Exact endpoint/bridge IDs and BAR ranges documented in
HARDWARE.md. Both native targets compile. Driver v1 attached and read PCI configuration/BAR successfully; MMIO correctly skipped with memory decoding off. The next test is bounded memory-decoding enable/read/restore.
Reference acquisition/review is in progress; Linux and Windows clones complete,
VHS-Decode and wiki full-history clones still fetching as of the initial build.

Primary uncertainty: no endpoint interrupt routing exposed; Linux requires more
than counter polling for trustworthy resident data. Safe 32-bit IOVA allocation,
coherency, engine cancellation and complete raw data integrity are untested.
No genuine ADC bytes, clock measurement, RF capture or decode result exist yet.
