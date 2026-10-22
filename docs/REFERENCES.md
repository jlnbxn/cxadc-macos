# Technical references and source provenance

The source pins are in [references/manifest.json](../references/manifest.json).
Reference repositories are optional, separate ignored checkouts; no embedded Git
repository, third-party source tree or binary is tracked in cxadc-macos.
They are not native build dependencies.

| Reference | Pinned revision | Use |
| --- | --- | --- |
| [CXADC Linux](https://github.com/happycube/cxadc-linux3) | `68b4ddbc072301f68d376637534a039ce675f6ce` | Principal raw ADC, registers and RISC reference |
| [CXADC Windows](https://github.com/JuniorIsAJitterbug/cxadc-win) | `bbfc930bc5caa6bed9441974caec6c9ec062b8af` | Independent DMA/addressing and sequencing cross-check |
| [VHS-Decode](https://github.com/oyvindln/vhs-decode) | `177b505e3a433c454794aeb53f469599ddc8a168` | External u8 loader, CVBS/VHS/SVHS decode and metadata |
| [CXADC wiki](https://github.com/happycube/cxadc-linux3/wiki) | `5462e62158de4764d7e11bffcc7c8169012fe2e6` | Board variations, nominal clock and input background |
| [TBC Tools source](https://github.com/harrypm/tbc-tools) | `932ba5ae81e0ac44aa7c52ba5cd4e3c4a77d78cb` | External analysis/export architecture; not locally built |

The external exporter actually used was official TBC Tools v3.3.0 macOS arm64,
release commit `9ce60e193e981b842889cca133f77126ce4c7954`. Its DMG SHA256 matched
GitHub's release digest:

```text
10b833b47541267c89211385c1dfcd58da70014264b18d27fcca74fc3733c28a
```

Its internal exporter version string reports 0.0.0; the release tag/digest establish
provenance. The suite renamed ld-analyse to tbc-analyse. GUI inspection was not
performed. No DMG or tool binary is distributed here.

The earlier local HVR-1200 project's reusable macOS infrastructure is acknowledged
in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md), including its reference
commit. Its source is not required by this checkout. See that file for all
copyright notices, license reasoning and remaining ownership review.

## Important implementation findings

- Linux matches `14f1:8800`; this port narrows to tested subsystem`4e4f:4553`.
  The ITE bridge is not a capture endpoint.
- Linux BAR offsets are byte offsets. Raw initialization values and named masks
  were taken from the source, not inferred from CX23885 behavior.
- Channel 24 uses eight 2048-byte SRAM clusters, RISC SYNC/counter reset,
  WRITE SOL/EOL and a JUMP to the first WRITE. Operands are 32-bit device addresses.
- Completion counters may advance before memory is visible. This port handles
  IRQ-boundary accounting/holdback and rejects ambiguous progress; modulo counters
  cannot establish an authoritative dropped-sample count.
- Apple Silicon device addresses come from device-bound IODMACommand mappings.
  CPU virtual/physical/BAR addresses are not substituted for them.
- Pinned Linux probe enables capture earlier than this diagnostic lifecycle does.
  The macOS port does not reproduce that ordering or assume Linux cleanup proves
  safe DMA quiescence through DART.
- Conexant datasheet 101069A, August 2002, p6-47 defines RUN_RISC as write-only;
  p6-72 defines OUTPUT_FORMAT bit0 as reserved/read-only; p6-85 describes CDT
  consumption and p6-86 transfer-in-progress. Readback/stop checks respect these
  distinctions. [Manufacturer-authored datasheet copy](https://datasheet.octopart.com/CX23883-39-Conexant-datasheet-42899180.pdf).
- `.u8` is one unsigned byte per sample. Decode frequency is explicitly in MHz;
  the decoder's default 40 MHz would misinterpret this stock-clock capture.

## Reconstruct optional references

For example:

```sh
git clone https://github.com/happycube/cxadc-linux3 references/cxadc-linux3
git -C references/cxadc-linux3 checkout --detach 68b4ddbc072301f68d376637534a039ce675f6ce
```

Use the corresponding manifest URL/pin for the others; the wiki Git URL ends in
`.wiki.git`. Fetch the exact commit if a shallow clone lacks it. Do not edit or
commit the upstream checkouts into this repository. Their own licenses and assets
are not granted by this project's LICENSE.

## Apple API references

The installed DriverKit SDK was inspected before using IOPCIDevice BAR/config
access, IODMACommand PrepareForDMA/CompleteDMA/PerformOperation, memory descriptors
and IOInterruptDispatchSource. SDK headers are external, not redistributed.

- [IOPCIDevice](https://developer.apple.com/documentation/pcidriverkit/iopcidevice)
- [DriverKit entitlement requests](https://developer.apple.com/documentation/driverkit/requesting-entitlements-for-driverkit-development)
- [PCI entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.driverkit.transport.pci)

The observed legacy interrupt source works on this setup despite the installed
SDK's MSI-oriented description. That result is hardware evidence for this setup,
not a promise for every bridge or OS release.
