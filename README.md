# CX23880 CXADC for macOS

Native arm64 PCIDriverKit development for unsigned 8-bit raw RF capture from
Conexant `14f1:8800`. **Stage A: enumeration and a bounded register probe.**
Capture, ADC initialization, DMA, streaming and VHS-Decode validation are not yet
implemented. The v2 probe has read real registers and restored PCI command 0000.
No sample file has been produced.

```sh
./scripts/build.sh
./build/cxadc-capture --list
./build/cxadc-capture --info
./scripts/build-driver.sh
./scripts/build-local-host.sh
python3 scripts/check-probe.py     # check the actual attached v2 probe
python3 tools/analyze_u8.py FILE.u8 --sample-rate 28636363
python3 -m unittest discover -s tests -v
```

Tested build environment: Apple Silicon, macOS 26.7.1, Xcode/DriverKit 27.0.
Builds create an unsigned dext and an ad hoc signed development host, without
installing anything. Normal distribution requires Apple signing and provisioning.
No external libraries are required. Reference clones live under ignored
`references/`; pinned revisions and licensing are in [docs/REFERENCES.md](docs/REFERENCES.md).

The CLI emits JSON from the actual IORegistry. Exit status 2 means no CX23880,
64 means invalid arguments, and 1 means an operational failure. Unsupported capture
arguments fail explicitly and do not create output files. `--info` currently includes
only a snapshot taken when the driver attached, not live ADC settings. No user client
or register-write interface is exposed.

The diagnostic extension matches only `14f1:8800`, subsystem `4e4f:4553`.
It never matches the ITE bridge, `8804` function or HVR-1200. It reads PCI configuration
and, only with bus mastering off, a fixed list of CX23880 registers. Version 2
temporarily enables only PCI memory decoding, performs no MMIO writes,
restores and checks the exact original PCI command, and enables neither interrupts
nor DMA. If bus mastering is already on, it skips MMIO entirely. An all-ones MMIO
result aborts further register reads.

The local host accepts `activate` and `deactivate`; these request macOS system-extension
operations and can require OS approval. They change no security settings. Activation
success alone does not establish attachment: inspect `cxadc-capture --info` afterward.
The existing HVR-1200 project and installed extension are separate and remain unchanged.

See [HARDWARE.md](HARDWARE.md), [ARCHITECTURE.md](ARCHITECTURE.md),
[TESTING.md](TESTING.md) and [PROGRESS.md](PROGRESS.md).

Project source is GPL-2.0-or-later. Upstream reference repositories retain their
own licenses. Raw RF connection remains a later milestone: VCR tap DC isolation,
input loading and safe levels must be resolved before connecting RF.

The offline analyzer reports a 256-code histogram, clipping-code counts/percentages,
mean, standard deviation, AC/DC RMS and code-span dB. It does not measure analog
dynamic range, sample clock or capture correctness. Its explicit sample-rate argument
is an assumption. No FFT is implemented. Tests use generated arithmetic fixtures only.
