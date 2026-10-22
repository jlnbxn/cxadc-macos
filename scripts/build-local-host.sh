#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Local ad hoc development build. Does not activate or change security settings.
set -euo pipefail
cd "$(dirname "$0")/.."
./scripts/build-driver.sh
app="$PWD/build/CXADCHost.app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Library/SystemExtensions" build/swift-cache
cp installer/Info.plist "$app/Contents/Info.plist"
xcrun swiftc -target arm64-apple-macos13.0 -module-cache-path "$PWD/build/swift-cache" installer/main.swift -framework SystemExtensions -o "$app/Contents/MacOS/CXADCHost"
# ditto preserves the DriverKit bundle layout produced by Xcode.
dext="$app/Contents/Library/SystemExtensions/com.example.CXADC.Driver.dext"
ditto build/DerivedData/Build/Products/Debug-driverkit/CXADCDriver.dext "$dext"
codesign --force --sign - --timestamp=none --entitlements driver/Driver.entitlements "$dext"
codesign --force --sign - --timestamp=none --entitlements installer/Host.entitlements "$app"
codesign --verify --strict "$dext"
codesign --verify --strict "$app"
printf 'Built local development host: %s\n' "$app"
