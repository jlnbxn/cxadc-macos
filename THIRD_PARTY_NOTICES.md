# Third-party notices and provenance

cxadc-macos source is distributed under GPL-2.0-or-later. The unmodified GPLv2
license text is in [LICENSE](LICENSE). Existing SPDX declarations are retained.
This is a macOS adaptation, not an independently authored hardware specification.

## CXADC Linux and register definitions

Pinned source: [happycube/cxadc-linux3](https://github.com/happycube/cxadc-linux3),
commit `68b4ddbc072301f68d376637534a039ce675f6ce`.

The raw ADC initialization sequence/expressions, register subset, SRAM/CDT
layout and RISC instruction scheme inform or are adapted into `core/Raw.hpp`,
`common/Probe.hpp`, `core/Risc.hpp` and the hardware setup in
`driver/CXADCDriver.cpp` / `driver/Stream.hpp`. Linux-specific PCI/DMA/character
device APIs are replaced with DriverKit code. The port changes buffer accounting,
IRQ cadence, address validation, ownership and shutdown for this environment.
macOS adaptation performed 2026-10-21; attribution clarified 2026-10-22.

Retained notices from `cxadc.c`:

```text
Copyright (C) 2005-2007 Hew How Chee <how_chee@yahoo.com>
Copyright (C) 2013-2015 Chad Page <Chad.Page@gmail.com>
Copyright (C) 2019-2024 Adam Sampson <ats@offog.org>
Copyright (C) 2020-2022 Tony Anderson <tandersn@cs.washington.edu>
```

Retained notices from `cx88-reg.h`:

```text
Copyright (C) 1996,97,98 Ralph Metzler (rjkm@thp.uni-koeln.de)
              2001 Michael Eskin
              2002 Yurij Sysoev <yurij@naturesoft.net>
              2003 Gerd Knorr <kraxel@bytesex.org>
```

Retained license/warranty notice for both Linux-derived portions:

```text
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
```

The contact addresses above are published upstream attribution, not local user
identities; they are retained deliberately.

## CXADC Windows

[JuniorIsAJitterbug/cxadc-win](https://github.com/JuniorIsAJitterbug/cxadc-win),
commit `bbfc930bc5caa6bed9441974caec6c9ec062b8af`, was a technical cross-check for
32-bit DMA, RISC/CDT layout and capture sequencing. Its driver files declare
GPL-2.0-or-later and acknowledge the Linux origin. No Windows driver implementation,
WDF header, capture-server code or clock-generator firmware is embedded here.
Its repository supplied the unchanged GPLv2 license text.

Upstream driver notice:

```text
Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
```

The Windows capture server and clock-generator components have different
licenses; they are not part of this source tree or its native targets.

## Earlier macOS CX23885 project

The operator's separate `hvr1200-macos` project at
`a 4e28509264881b4d9b67ba285a0dcbb400265a4` supplied adapted lifecycle, user-client,
DMA/build and activation patterns. The inspected source carries
GPL-2.0-or-later SPDX declarations. It is not a checkout/build dependency and its
private local directory is not published here. CX23885 register/decoder logic
was not imported. The owner should confirm rights to that earlier source and
any contributions before publication; an SPDX label alone is not a complete
chain-of-title record. That repository is unchanged by this task.

## External tools and platform components

VHS-Decode and TBC Tools are GPLv3 external programs used for validation and
export. Their implementations and binaries are not incorporated or redistributed
in this repository. Python orchestration invokes their CLIs as separate processes;
metadata diagnostics recognize recorded output facts rather than copying decoder
functions. They retain their own license obligations if distributed separately.
Exact revisions/releases are in [the manifest](references/manifest.json).

Apple DriverKit/PCIDriverKit, IOKit, Foundation and SystemExtensions headers and
frameworks come from the separately installed Xcode/OS SDK. No Apple SDK header,
framework, generated IIG implementation, signing asset or binary is committed.
The native build otherwise uses the platform C++/Swift runtimes and tools.
Python diagnostics use the standard library, except optional NumPy CVBS analysis.

## License decision and remaining review

Retaining **GPL-2.0-or-later** is appropriate for the identified Linux-derived
work and existing project declarations; relicensing this adaptation as MIT/BSD
would not remove upstream obligations. No incompatible embedded dependency was
identified. GPL system-library provisions are relevant to platform linking; see
[GNU's FAQ](https://www.gnu.org/licenses/gpl-faq.html#SystemLibraryException).
A binary distributor must separately review Apple SDK/signing terms and provide
GPL-compliant corresponding source. This source audit is not a legal opinion or
approval of a future binary distribution agreement. No new linking exception is
invented and no license text has been changed.
