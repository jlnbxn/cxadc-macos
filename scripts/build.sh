#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
xcrun clang++ -std=c++20 -pthread -arch arm64 -mmacosx-version-min=13.0 -fobjc-arc \
  -Wall -Wextra -Werror src/main.mm src/Control.mm src/Capture.mm -framework Foundation -framework IOKit -o build/cxadc-capture
codesign --force --sign - --timestamp=none --entitlements src/CLI.entitlements build/cxadc-capture
