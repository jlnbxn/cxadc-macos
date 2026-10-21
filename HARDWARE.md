# Observed hardware — 2026-10-21

Read-only inspection on the development Mac. Unredacted command output stays in
ignored `artifacts/discovery/`; this report excludes serial numbers and host UUIDs.

| Item | Observed value |
| --- | --- |
| macOS | 26.7.1, build 25G241 |
| CPU architecture | arm64; Apple M3 Max; Mac15,8 (sysctl verified) |
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

V2 attachment temporarily changed PCI command `0000 → 0002 → 0000`; final readback
verified restoration. No MMIO writes or DMA occurred. Interrupt dword then read
`37140101` (line 01, pin 01). After Open, IORegistry now exposes
`ApplePCIECLegacyIntController-apciec1` with specifier `<0100000000000000>`.
Thus absent initial routing properties are not a proven interrupt limitation.

| Register | Offset | Actual v2 attachment read |
| --- | --- | --- |
| MO_DEV_CNTRL2 | 200034 | 45534e4f |
| MO_PCI_INTMSK | 200040 | 00000000 |
| MO_PCI_INTSTAT | 200044 | 00030000 |
| MO_VID_INTMSK | 200050 | 00000000 |
| MO_VID_INTSTAT | 200054 | 00000000 |
| MO_DEVICE_STATUS | 310100 | 00000214 |
| MO_INPUT_FORMAT | 310104 | 00004080 |
| MO_PLL_REG | 310168 | 00f15f18 |
| MO_SCONV_REG | 310170 | 00021f07 |
| MO_AGC_SYNC_TIP3 | 310210 | 1e48e040 |
| MO_AGC_GAIN_ADJ4 | 310220 | 006a2c34 |
| MO_VBI_GPCNT | 31c02c | 00000000 |
| MO_VID_DMACNTRL | 31c040 | 00000000 |

All offsets/values above are hexadecimal. These are power-on/attachment observations,
not the intended raw configuration. `MO_DEV_CNTRL2` notably equals the subsystem
ID dword; do not substitute this readback for a documented reset value or infer
undocumented bits. DEV_CNTRL2 is write-only, so its readback cannot establish RISC state. These
were pre-configuration observations; Stage B subsequently applied raw settings. `mmio_valid` means the bounded reads did not return the API's
all-ones failure sentinel; it does not prove analog signal fidelity or chip settings.


Stage B signal tests: the user confirmed disconnected inputs for initial captures,
then connected a known-good PAL MiniDV conventional CVBS output to RCA. VMUX1
changed from a narrow 110..112 disconnected distribution to a clear CVBS waveform,
70..159, with horizontal-sync periodicity and colour-carrier energy. VMUX0 also
receives CVBS with C31 intact; no exclusive mapping is inferred. No physical card
or VCR changes were made. Sixdb=0 follows the pinned CXADC control convention
(bit 23=0); the old datasheet's INIT_6DB_VAL polarity description conflicts with
the upstream off/on naming, so electrical boost calibration is still unverified.

With the documented INPUT_FORMAT AGCEN gate disabled, real PAL RCA raw captures
were decoded into recognizable video (225/498 fields from 5/10 s). VMUX1 is
verified usable for RCA, not uniquely mapped. No clock or electrical gain
calibration is inferred from the decoder result.

First RF test, 2026-10-21: after idle enclosure reconnection, the endpoint and v8
DriverKit driver reattached and reported PCI command 0000/DMAoff/no quarantine.
User reports JVC playback RF test point → coax → card RCA, with a 68 nF ceramic
series capacitor near the test point. C31 remains intact. Wiring is described as
loose. This tap construction is user-reported, not physically inspected or
electrically calibrated; VCR output loading remains unverified. A playing VHS
tape produced real RF samples that VHS-Decode decoded into recognizable colour
PAL video. This is distinct from the previous MiniDV CVBS test.

Second RF section was confirmed as PAL S-VHS/S-VHS ET by the user. With C31
still intact, a full 60 s raw capture and near-full-minute S-VHS decode/export
succeeded at the same level 0/sixdb 0 settings. The user plans C31 removal next.
That physical change remains pending until reported; the saved minute is the
before-modification baseline. Existing observations that RCA reached VMUX0/1
are consistent with the pinned white-card C31 input-bridging description.

Post-C31 state, 2026-10-21: user reports successful capacitor removal and card
reattachment. Physical removal is user-reported, not visually inspected by the
agent. Read-only enumeration confirms the same endpoint14f1:8800/subsystem
4e4f:4553 at4:0:0, BAR0 size16 MiB and driver v8/API5 attached. Live status:
PCI command0000, video DMA=0, running=0, no quarantine or reported error. ADC
initialized0 and cleared stream counters are consistent with fresh attachment;
no post-modification capture has run yet. Internal SSD has85 GiB available.
The existing intact-C31 captures remain the before-modification baseline.

Post-C31 real RF test completed with the same endpoint and unchanged settings:
60 s raw capture, 1,718,181,780 bytes, 25 wraps, no reported DMA/overrun errors and
verified idle teardown. User-reported removal is now associated with genuine
captured data; the physical component itself was not inspected by the agent.
ADC span stayed 97..120. Matched decoded-scene diagnostics are mixed; no clear
amplitude or overall quality improvement is established. See docs/C31-COMPARISON.md.
