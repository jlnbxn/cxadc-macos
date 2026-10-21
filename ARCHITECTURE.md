# Capture architecture — implemented through driver v8

The verified Stage A endpoint/BAR lifecycle remains in place. Attachment makes a
bounded register snapshot, then leaves PCI command restored. An entitled native
user client adds serialized static ADC configuration, mapping tests, status and
start/pull/stop operations. All hardware operations run on the driver's default
serial queue; user-client methods, interrupt callbacks and watchdog callbacks share
that queue. Only the observed Conexant endpoint/subsystem matches.

Static RAW8 configuration follows the pinned Linux CXADC sequence at stock PLL
11000000/SCONV 20000. GPIO, tuner and decoder firmware are untouched. VMUX, level,
sixdb, center offset and the documented global ADC AGC gate remain general raw
ADC controls. AGC defaults enabled to preserve Linux configuration; optional
disable achieved field lock in the actual CVBS test. The driver performs no
CVBS/RF-specific processing. Writable settings are read back; reserved output
bit 0 is excluded based on the manufacturer datasheet. RUN_RISC is write-only,
so its readback is not used to infer state.

Samples, the RISC program and driver-owned CPU staging memory use IODMACommand
with maxAddressBits=32 on the actual endpoint. Every device-visible segment's full
address range, alignment and coverage is checked before narrowing an operand.
Host pointers and BAR addresses are never used as DMA addresses. Program storage
must be one contiguous device-visible segment. The RISC generator supports sample
scatter/gather segments and uses explicit 16 KiB accounting pages / 2048-byte
WRITE transfers. Tests cover 32-bit boundaries, encoding, SG crossing and wrap.

Channel 24 uses the Linux eight-cluster SRAM CDT and CMDS layout. Dynamic DMA
counter/pointer registers are not static readback latches: writing CNT2 starts CDT
consumption. Static SRAM descriptors and command fields are checked instead. Bus
mastering is enabled only after mapping/program preparation and descriptor checks.

The default 64 MiB sample ring provides ~2.34 seconds at nominal rate. A 256 KiB
RISC IRQ1 cadence drives real IOInterruptDispatchSource callbacks. Completion
reads GPCNT only after the event and rounds to the IRQ boundary. One full completed
IRQ block is held back before exposing bytes. Pull checks producer distance both
before and after copying to prepared staging, then CPU-maps the caller descriptor;
caller memory is never passed to PerformOperation. There is no completion polling
fallback. Actual legacy interrupt delivery works on this observed macOS.

A 5 ms watchdog checks errors, missing progress and consumer lag. An observation
gap beyond half a ring time at a conservative 60 MSPS limit is ambiguous and fails
capture. Modulo counters do not establish an authoritative dropped-sample count.
The first cold 8 MiB run hit that conservative limit; later small-ring wrap tests
passed. Default 64 MiB captures passed one/ten/sixty-second and CVBS tests.

The native CLI transfers full 256 KiB blocks into a 32-slot bounded writer queue.
The disk thread is independent of hardware callbacks; queue exhaustion fails
visibly. Output is uncompressed u8. Finite capture trims only the final byte prefix
to the nominal-rate target; delivered versus written counts remain separate.
Ctrl-C stops hardware before the queue drains. SIGKILL user-client closure and a
stalled consumer have also been hardware-tested. Metadata starts as incomplete,
then records success/failure, requested settings, clock uncertainty, byte counts,
IOVAs, IRQ/counter diagnostics, errors and stop verification. A temporary CLI idle-sleep assertion spans the capture lifetime; forced sleep
is not thereby handled or verified. No stdout mode yet.

Stop masks interrupts, disables video FIFO/RISC, writes RUN_RISC=0, waits for the
documented VID_XFER_STATUS to clear, disables mastering, observes stable counters,
checks the untouched guard and restores the exact initial PCI command. Only then
are DMA mappings completed/released. Failed quiescence quarantines mappings and
blocks capture. The successful short tests verified this path; unplug, sleep/wake
and archival-duration operation are not yet validated. No CX23885 TC_REQ offsets
or reset semantics are reused.

PCIDriverKit remains viable: mappings, raw DMA and interrupts now run on hardware.
There is no demonstrated reason to use a legacy kext. No additional security
settings have changed; no code has been pushed or published.

Real RF validation now confirms that this same general ADC engine supports
VHS-Decode: VMUX1/level0/sixdb0/offset8/AGC1, 1/10/60-second actual RF captures.
Only the offline decoder changes between the CVBS and RF tests; no driver code or
stream transformation was added. The 10 s RF decode exported recognizable colour
PAL video. Details and limits: docs/RF-FIRST-CAPTURE.md.
