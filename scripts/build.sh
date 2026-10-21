#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
xcrun clang++ -std=c++20 -arch arm64 -mmacosx-version-min=13.0 -fobjc-arc \
  -Wall -Wextra -Werror src/main.mm -framework Foundation -framework IOKit -o build/cxadc-capture
codesign --force --sign - --timestamp=none build/cxadc-capture
