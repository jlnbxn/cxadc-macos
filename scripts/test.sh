#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# CPU-only tests. No capture utility invocation or hardware access.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
xcrun clang++ -std=c++20 -fsanitize=address,undefined tests/risc_tests.cpp -o build/risc-tests
./build/risc-tests
xcrun clang++ -std=c++20 -Wall -Wextra -Werror tests/client_api_tests.cpp \
  client/cxadc_client.cpp -framework IOKit -framework CoreFoundation -o build/client-api-tests
./build/client-api-tests
python3 -m unittest discover -s tests -v
