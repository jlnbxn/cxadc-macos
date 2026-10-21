# Progress

Branch: `feature/cx23880-cxadc-macos`. Workspace was empty and had no Git history.
No remote configured; no code published. The HVR-1200 repository at
`/Users/julianboxan/Developer/hvr1200-macos` was inspected read-only at
`a4e28509264881b4d9b67ba285a0dcbb400265a4` and remains unchanged.

Milestone 1: actual host/PCI discovery, isolated DriverKit target, native enumeration
CLI and local host build. Exact endpoint/bridge IDs and BAR ranges documented in
HARDWARE.md. Both native targets compile. Driver v1 attached and read PCI configuration/BAR successfully; MMIO correctly skipped with memory decoding off. V2 also verified bounded memory-decoding enable/read/restore and actual register access.
All four reference clones are complete, pinned and reviewed for the first milestone;
see references/manifest.json and docs/REFERENCES.md. Tracked upstream content is unchanged.

After Open, legacy interrupt routing is exposed and DriverKit reports level type;
actual delivery remains untested. Primary uncertainty: Linux requires more
than counter polling for trustworthy resident data. Safe 32-bit IOVA allocation,
coherency, engine cancellation and complete raw data integrity are untested.
No genuine ADC bytes, clock measurement, RF capture or decode result exist yet.

Discovery/PCI attachment commit: `2d05277`. V2 register-probe result is a separate
follow-up milestone. No DMA enabled. V1 replacement initially left the old instance
attached; explicit deactivation detached it. macOS reported reboot-required for
retirement, but v2 activation/attachment subsequently succeeded without reboot.
No driver processes were killed and no security setting changed.

Stage B: native user client, static ADC configuration and private 32-bit DMA mapping
self-test implemented and verified on real hardware with mastering off. V4 passed
all 18 ADC readbacks after correcting reserved output bit 0. A 64 MiB mapping at
IOVA 80000000 and RISC mapping at 84004000 passed the supported DriverKit memory
lifecycle and CPU round-trip. Actual RISC capture/interrupt delivery remains next.
The manufacturer datasheet corrects Stage A: DEV_CNTRL2 is write-only; its readback
cannot establish RISC state. No physical modifications or CX23885 edits.
