# Implementation and next gates

The implemented vertical slice is IORegistry enumeration → exact endpoint matching
→ DriverKit PCI open → PCI configuration and bounded register reads → immutable
registry snapshot → native JSON CLI. It has no DMA allocation, sample generation,
ADC configuration or interrupt acknowledgement. All offsets are CX23880 definitions
from the pinned Linux source; no CX23885 register assumptions are carried across.

PCIDriverKit is the first choice: the installed SDK supplies Open/Close,
ConfigurationRead, GetBARInfo, MemoryRead32, IOInterruptDispatchSource and
IODMACommand. The same Mac already activates a custom PCI DriverKit driver.
The diagnostic target compiles against these actual installed headers. Apple’s
[PCI interface](https://developer.apple.com/documentation/pcidriverkit/iopcidevice)
and [entitlement requirements](https://developer.apple.com/documentation/driverkit/requesting-entitlements-for-driverkit-development)
remain applicable; ad hoc entitlements are not distribution approval.

Legacy IOKit headers are installed, including IOPCIDevice and DMA support. A kext
would require kernel approval/signing/AuxKC validation on Apple Silicon and would
not automatically repair missing PCI INTx routing or remove DART constraints.
There is currently no demonstrated DriverKit API blocker justifying that risk.
No kext build or security change is proposed at Stage A. Reevaluate only on concrete
DriverKit mapping/completion/throughput evidence.

Later modules should separate chip configuration, DMA mappings, completion accounting,
buffer ownership and userspace disk writing. Stage B must run only while mastering,
engines and interrupt masks are idle. Stock-clock 8-bit configuration comes from the
pinned Linux sequence, without its start-DMA-during-probe ordering.

DMA gates before Stage C:

1. Map both samples and RISC storage with IODMACommand bound to the actual endpoint,
   `IODMACommandSpecification.maxAddressBits=32`; retain prepared mappings until
   confirmed quiescence. Check every returned segment's entire range below 4 GiB,
   sum, alignment, direction and coverage. Never cast CPU pointers into RISC operands.
2. Use 2048-byte transfers from channel 24/VBI SRAM descriptors. Handle 16 KiB host
   pages and scatter/gather boundaries explicitly. Linux's 4 KiB PAGE_SIZE must not
   become an implicit macOS constant. Require contiguous mapped RISC fetch storage
   or explicitly verified jumps across mapped segments. Emit little-endian words.
3. Establish a trustworthy completion boundary. Linux warns GPCNT alone can lead
   actual memory visibility; its IRQ1 boundary is important. Missing legacy interrupt
   routing needs an evidence-based solution. Polling a counter, assuming a cache
   barrier flushes PCI writes, or borrowing the CX23885 ring is insufficient.
4. Validate coherency and CPU access using supported mapping APIs. Retain mappings
   through engine stop, bus-master disable, posted-write completion and verified
   request drain. If quiescence cannot be established, quarantine allocations;
   freeing possibly active DMA memory risks a system panic. HVR project panic notes
   motivate this gate, not an assertion of a proven CX23880 stop sequence.
5. Bound producer-consumer lag; a wrapping/coalesced counter cannot prove an exact
   dropped-sample total. Fail on ambiguous progress, timeout, hardware fault or
   consumer overrun. Sleep/unplug require cancellation design and separate tests.

Target ring: at least 64 MiB (~2.34 seconds at nominal rate), with a separate bounded
userspace writer queue. DMA must never wait for disk writes. Initial supported
transfer copies may be acceptable if measured at 28.6 MB/s; zero-copy is not claimed.
An hour is about 103 GB decimal. The future CLI must refuse overwrite, handle partial
writes/EINTR/EPIPE/ENOSPC, cleanly stop on signals, and emit a sidecar even on failure.
Metadata will distinguish configured/estimated/measured clock, delivered/written
bytes, duration, hardware faults, software-overrun events and unknown sample loss.
No stream resampling, filtering, PAL decoding or byte swapping is authorized by the
raw output format itself. The capture utility must preserve hardware byte order.
