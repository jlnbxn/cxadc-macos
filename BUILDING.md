# Building and installing

## Prerequisites

The verified build host uses Apple Silicon, Xcode 27.0 (27A266a), macOS SDK 27.0
and DriverKit SDK 27.0. Runtime tests used macOS 26.7.1 (25G241). Other toolchains
and OS versions have not been validated. A full Xcode installation is required;
the Command Line Tools alone do not provide the tested DriverKit environment.

Inspect the selected toolchain before building:

```sh
xcodebuild -version
xcodebuild -showsdks
xcrun --sdk driverkit --show-sdk-path
```

The project sets arm64, DriverKit deployment target 23.0 and CLI/host deployment
target macOS 13.0. Those compile-time targets do not establish compatibility with
older operating systems. No home directory, Team ID or signing certificate is
hardcoded into the build scripts.

## Build from a source checkout

From the repository root:

```sh
./scripts/build.sh
./scripts/build-driver.sh
./scripts/build-local-host.sh
./scripts/test.sh
```

Outputs are under ignored `build/`:

- `cxadc-capture`: native CLI with the user-client entitlement, ad hoc signed.
- `DerivedData/Build/Products/Debug-driverkit/CXADCDriver.dext`: unsigned Xcode build.
- `CXADCHost.app`: ad hoc signed local host with an embedded development extension.

The host script also builds the driver. No reference clone or offline decode
package is required. CPU tests use Xcode clang with AddressSanitizer/UBSan and
Python3's standard-library unittest. Tests do not start DMA or access hardware.

## Signing and entitlements

The checked-in entitlements are required project configuration, not provisioning
profiles or secrets:

| Component | Entitlements |
| --- | --- |
| Dext | `com.apple.developer.driverkit`, `com.apple.developer.driverkit.transport.pci`, app sandbox |
| Host | `com.apple.developer.system-extension.install` |
| CLI | `com.apple.developer.driverkit.userclient-access` for the dext bundle ID |

Apple controls permission to use restricted DriverKit entitlements. Obtain the
appropriate capabilities and provisioning for your own development/distribution
identity; writing an entitlement into a plist does not grant it. See Apple's
[DriverKit entitlement guidance](https://developer.apple.com/documentation/driverkit/requesting-entitlements-for-driverkit-development)
and [macOS distribution-signing guidance](https://developer.apple.com/documentation/xcode/creating-distribution-signed-code-for-the-mac/).

The identifiers `com.example.CXADC.Driver` and `com.example.CXADC` are existing
development placeholders. The verified setup keeps them unchanged. A distributor
must select identifiers it controls and update the Xcode project, both Info.plists,
the CLI entitlement, host request identifier and development-host packaging path
together. Keep the driver class matching consistent with its Info.plist.
Check for all occurrences before doing so:

```sh
rg 'com\.example\.CXADC' driver installer src scripts CXADC.xcodeproj
```

The scripts intentionally produce development/ad hoc builds; they are not a
complete Developer ID signing/notarization workflow. Public binary distribution
is not validated. No certificates, profiles, Team IDs or private keys belong in Git.

On the verified development Mac, SIP was already disabled and system-extension
developer mode already enabled. This is an observation, not an installation
requirement or instruction to change security. We have not tested installation
on a Mac with default security settings. Apple Silicon approval/provisioning and
PCI DMA requirements still apply. This project does not use a legacy kext.

## Activation and removal

First resolve signing/provisioning and ensure no capture is running. Put the
appropriately signed host in `/Applications/CXADCHost.app`; the command submits a
SystemExtensions request, and macOS may require approval or a restart:

```sh
/Applications/CXADCHost.app/Contents/MacOS/CXADCHost activate
systemextensionsctl list
./build/cxadc-capture --list
./build/cxadc-capture --status
```

The activation/deactivation interface was exercised during development. A host
request completing does not prove PCI attachment: inspect the actual driver and
live status. Do not replace or deactivate a running capture. Activation can
replace an existing extension with the same identifier.

To request removal, after stopping capture:

```sh
/Applications/CXADCHost.app/Contents/MacOS/CXADCHost deactivate
systemextensionsctl list
```

macOS may defer removal until restart. Remove the app only after deactivation has
completed. Never use system-extension reset or security changes as routine
installation steps. This cleanup does not activate/deactivate the working driver.

## Offline decode tools

VHS-Decode and TBC Tools are external programs with their own dependencies,
licenses and installation instructions. See [the pinned manifest](references/manifest.json)
and [provenance](docs/REFERENCES.md). They are not linked into the driver or
redistributed in this source repository. Historical reports mention local,
ignored `build/decode-venv` and `build/tbc-tools.app`; a fresh checkout does not
contain them.

`tools/decode_full_tape.py --help` documents the optional PAL S-VHS workflow.
It accepts explicit decoder/exporter paths, otherwise searches PATH and then the
historical local build locations. It needs `ffprobe` on PATH and does not install
any dependencies. It preserves the raw input and TBCs, refuses output overwrite
and guards free space. It is not a general VHS/CVBS format selector.

## Troubleshooting

- `IOServiceOpen` failing inside a sandbox does not alone prove entitlement or
  hardware failure; compare with an appropriately entitled, unsandboxed invocation.
- Check the active driver build/API and exact PCI/subsystem match before capturing.
- `--info` is an attachment-time snapshot; use `--status` for current state.
- A failed hardware stop quarantines mappings; do not force-release them.
- An 8 MiB cold-ring trial hit the watchdog. Use the default 64 MiB ring.
- Live unplug, forced sleep/wake and disk-full recovery remain unverified.
