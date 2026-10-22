#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
xcrun clang++ -std=c++20 -arch arm64 -mmacosx-version-min=13.0 -dynamiclib \
  -Wall -Wextra -Werror client/cxadc_client.cpp -framework IOKit -framework CoreFoundation \
  -install_name @rpath/libcxadc-client.dylib -o build/libcxadc-client.dylib
codesign --force --sign - --timestamp=none build/libcxadc-client.dylib
