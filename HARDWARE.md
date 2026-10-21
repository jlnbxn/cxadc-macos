# Observed hardware — 2026-10-21

Read-only inspection on the development Mac. Unredacted command output stays in
ignored `artifacts/discovery/`; this report excludes serial numbers and host UUIDs.

| Item | Observed value |
| --- | --- |
| macOS | 26.7.1, build 25G241 |
| CPU architecture | arm64; M3 Max supplied by user |
| Xcode | 27.0, build 27A266a |
| Installed SDKs | macOS 27.0, DriverKit 27.0 (also mobile SDKs) |
| Enclosure | OWC Express 1M2; USB4; link reported 40 Gb/s; firmware 12.34 |
| PCI path | USB4 / ASMedia `1b21:2463` bridges / ITE `1283:8893` / Conexant |
| ITE | `1283:8893`, revision `52`, PCI `3:0:0`, bridge class `060401` |
| Capture endpoint | `14f1:8800`, revision `05`, PCI `4:0:0`, class `040000` |
| Subsystem | `4e4f:4553` |
| Additional function | `14f1:8804`, PCI `4:0:4`, class `048000`, same subsystem |
| BAR0 assigned bus range (8800) | `0x02000000`, size `0x01000000` (16 MiB) |
| BAR0 CPU aperture (8800) | IODeviceMemory address `0xe02000000`, 16 MiB |
| BAR0 assigned bus range (8804) | `0x01000000`, size 16 MiB |
| BAR0 CPU aperture (8804) | IODeviceMemory address `0xe01000000`, 16 MiB |
| DMA mapper | IODARTMapper reported, mapper page size 16384 |
| Endpoint capabilities in IORegistry | power management `0x4c`, VPD `0x44` on 8800 |
| MSI | system_profiler says No; no MSI capability in endpoint registry |
| Interrupt routing | no IOInterruptControllers or IOInterruptSpecifiers exposed on either endpoint |
| Initial driver claim | system_profiler: Driver Installed No; no child driver in IORegistry |

Assigned addresses are decoded from little-endian PCI cells; these are not DMA
addresses. The CPU aperture above 4 GiB does not prove 32-bit DMA is impossible.
`IOPCIDeviceMemoryMapBase/Size` are opaque registry properties, not BAR byte sizes.
System Profiler's endpoint “Link down” corresponds to legacy non-PCIe link status
`ffff`; it does not by itself establish a failed USB4 link.

The bridge identity confirms an IT8893-family device, not the exact E package
suffix. The `CX23880-19` marking, board colour and connector wiring are user-supplied,
not independently read from silicon. The presumed 28.636363 MHz oscillator and
RCA/S-Video VMUX routing remain unverified. Enumeration cannot measure an oscillator.

SIP is observed disabled. `systemextensionsctl developer` reports developer mode
already on. Neither setting was changed. An unsandboxed signing query finds one
valid Apple Development identity; the sandbox incorrectly reported zero.
The HVR-1200 ad hoc signed DriverKit v22 is activated/enabled, and its project
supports local ad hoc development. This is evidence for trying the same path,
not proof of CXADC activation or of Apple-granted PCI provisioning. No provisioning
secrets were read or copied. Sandbox `systemextensionsctl list` failed with error 1;
the same read-only query outside the sandbox succeeded.

DriverKit v1 attached as com.example.CXADC.Driver with ad hoc signing and existing
security settings. GetBARInfo verified BAR0 M32/16 MiB. PCI command is `0000`;
interrupt config dword `37140100` (line 00, pin 01). GetInterruptType index 0
returns success/type 1 (level), despite missing registry interrupt properties.
This does not prove delivery; actual completion IRQs remain untested.
