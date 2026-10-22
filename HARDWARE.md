# Verified hardware and experimental RF path

The results below describe one physical setup, tested on 2026-10-21. Software
observations and operator-reported construction details are distinguished.
No device serial numbers, host UUIDs or unredacted system logs are published.

## Host and PCI topology

| Item | Observation |
| --- | --- |
| Host | Apple M3 Max, arm64, Mac15,8 |
| Runtime OS | macOS 26.7.1, build25G241 |
| Toolchain | Xcode 27.0 build27A266a; macOS/DriverKit SDK 27.0 |
| Enclosure | OWC Express 1M2; USB4, reported 40 Gb/s link; firmware 12.34 |
| Topology | USB4 → ASMedia `1b21:2463` bridges → ITE `1283:8893` → Conexant |
| Bridge | `1283:8893`, PCI revision`52`, class`060401`, observed BDF 3:0:0 |
| ADC endpoint | `14f1:8800`, PCI revision`05`, class`040000`, observed BDF 4:0:0 |
| Endpoint subsystem | `4e4f:4553` |
| Other function | `14f1:8804`, class`048000`, observed BDF 4:0:4; not matched |
| BAR0, 8800 | 16 MiB; assigned bus address`0x02000000`, observed CPU aperture`0xe02000000` |
| BAR0, 8804 | 16 MiB; bus`0x01000000`, CPU aperture`0xe01000000` |
| DMA mapper | IODARTMapper; reported 16 KiB mapper page size |
| Interrupts | Legacy level interrupt, index0/type 1; actual callbacks verified; no MSI observed |

Addresses/BDFs are observations, not stable configuration or usable DMA addresses.
The driver obtains its own BAR mapping and device-bound DMA translations.
The bridge identity confirms the IT8893 family; the **IT8893E** suffix and
**CX23880-19** chip marking are operator-reported, not readable from PCI IDs.

The white board is identified by the operator as **Y328**. A further silkscreen
revision was not recorded; PCI revision 05 is a chip revision, not a PCB revision.
No other white-board variant or CX25800 has been tested.

### Early attachment register evidence

The initial endpoint exposed power-management capability at 0x4c and VPD at 0x44.
Before raw initialization, the v2 bounded register snapshot read the following
hexadecimal byte offsets/values, temporarily enabling memory decoding and
restoring PCI command 0000 afterwards:

| Register | Offset | Attachment readback |
| --- | --- | --- |
| MO_DEV_CNTRL2 |200034 |45534e4f |
| MO_PCI_INTMSK |200040 |00000000 |
| MO_PCI_INTSTAT |200044 |00030000 |
| MO_VID_INTMSK |200050 |00000000 |
| MO_VID_INTSTAT |200054 |00000000 |
| MO_DEVICE_STATUS |310100 |00000214 |
| MO_INPUT_FORMAT |310104 |00004080 |
| MO_PLL_REG |310168 |00f15f18 |
| MO_SCONV_REG |310170 |00021f07 |
| MO_AGC_SYNC_TIP3 |310210 |1e48e040 |
| MO_AGC_GAIN_ADJ4 |310220 |006a2c34 |
| MO_VBI_GPCNT |31c02c |00000000 |
| MO_VID_DMACNTRL |31c040 |00000000 |

These are observations, not recommended initialization values. DEV_CNTRL2 is
write-only: its coincidental subsystem-like readback is undefined and cannot
prove engine state. Interrupt config changed from 37140100 before Open to
37140101 afterwards; GetInterruptType returned index 0/type 1. Initial absence
of routing properties was superseded by actual legacy interrupt delivery.

## Enclosure, adapter and power

The operator reports an OWC Express 1M2 enclosure connected through an
**M.2-to-PCIe adapter** to the Y328 card. The exact adapter model and its power
routing have not been independently documented. The tested card ran **without an
auxiliary Molex power connection**. This does not establish that another card or
adapter can operate without auxiliary power, or that every M.2 adapter supports
this legacy endpoint through USB4.

No bridge firmware was flashed. Initial enumeration showed the endpoint
independently behind the ITE bridge and no existing endpoint driver. Opening the
endpoint established legacy interrupt routing even though initial IORegistry
properties did not expose it. “Link down” in System Profiler reflected legacy
link-status reads and did not prevent capture.

## Clock and raw settings

The original stock oscillator was retained. Raw capture is configured for
**nominal 28.636363 MSPS**, unsigned 8-bit, using the Linux reference's stock PLL
and SCONV settings. The oscillator frequency has **not** been independently
measured; byte counts and successful decoding do not calibrate it. No crystal
modification or alternate sampling mode was tested.

RCA works with VMUX1. Before C31 removal, VMUX0 also received the same composite
source; no exclusive connector map is inferred. PAL CVBS acquired reliably with
AGC disabled; RF tests retained AGC enabled. Level0/sixdb 0/offset 8 remains the
conservative default. Level31/sixdb 0 was used in the full-tape RF test.
Sixdb uses the reference driver's naming; actual analog gain/boost calibration
and the old datasheet's polarity wording remain unresolved.

## JVC and temporary RF tap

The JVC **HR-S7851EU remains electrically unmodified**, according to the operator:
no internal soldered modification, firmware change or adjustment was performed.
Temporary test-hook access is distinct from a permanent deck modification.
The operator identifies the connections as **TP106 for playback RF** and
**W1203 for the return/ground reference**. These are the reported connections on
this deck, not an independently verified service-manual pinout for other models.

The reported temporary arrangement uses miniature test hooks, PAL antenna coax,
screw terminals and RCA adapters, with a **68 nF ceramic series coupling
capacitor near the RF test point**, feeding the capture card's RCA socket.
It was improvised and initially loose; no construction photograph is included.

This arrangement produced decodable RF, but its input loading, impedance,
frequency response, DC isolation under fault and grounding have not been
instrumented. The 68 nF value is a record of this experiment, not a universal
recommended tap design. A larger capacitor, buffer or amplifier was not tested.
Verify the deck-specific RF/ground points and electrical isolation before
reproducing a connection. Successful capture alone does not establish electrical
safety or optimal signal quality.

The operator monitored live SCART during gain0/4/8/16/24/31 tests and reported no
visible change at capture start/stop or during those settings. The tap remained
connected throughout, so this does not measure loading versus an untapped deck.

## C31 comparison

Initial PAL CVBS and first RF captures succeeded with C31 intact. The operator
later removed C31 from the capture card and reattached it; this was not a
software-controlled modification. The JVC itself remained unmodified.

Matched-scene level 0 comparisons before/after removal found:

| Diagnostic | C31 intact | C31 removed |
| --- | ---: | ---: |
| Entire-minute ADC codes |97–120 |97–120 |
| Matched dropout segments |318 |295 |
| Matched median bPSNR diagnostic |39.7 |38.6 |
| Matched interval |2,772 fields |2,772 fields |

These mixed results show no established major improvement or amplitude increase.
Tape passes, tracking and reconnection can differ. The bPSNR value is not a
calibrated SNR measurement. See [the comparison report](docs/C31-COMPARISON.md).
C31 removal is not a driver installation requirement and is not a universal
recommendation for other board layouts.

## Actual capture evidence and limits

Disconnected-input tests passed 1/10 seconds. PAL MiniDV CVBS tests reached 60
seconds and produced recognizable colour output through CVBS-Decode. Real
VHS/S-VHS RF tests passed 1/10/60 seconds, followed by one **45-minute** T7 capture:
77,318,180,100 raw bytes, 294,947 interrupt callbacks, 1,152 ring wraps, no reported
DMA/overrun/write errors and verified shutdown/PCI command restoration.

The full RF decode produced 134,826 fields and a 44:56.52 lossless FFV1 export with
standard dropout correction. A full-file FFmpeg decode passed; programme frames
were spot-checked. This is one verified run, not a zero-loss or archival-quality
certification. No family footage, raw samples or preview images are in Git.

During the gain sweep, level 0 codes 99–119 expanded to 36–182 at level 31 without
rail samples; a later minute at level 31 measured 36–181. Selected windows across
the long capture also had no rail samples, but its entire histogram was not
computed. An amplifier requirement is not established by ADC span alone.
A repeatable horizontal streak was concealed by standard software dropout
correction; see [dropout investigation](docs/DROPOUT-INVESTIGATION.md).

Clock calibration, input loading/noise, sleep/wake, active unplug, disk-full
recovery and reproducibility across other machines/cards remain unverified.
