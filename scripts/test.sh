#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# CPU-only tests. No capture utility invocation or hardware access.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
xcrun clang++ -std=c++20 -fsanitize=address,undefined tests/risc_tests.cpp -o build/risc-tests
./build/risc-tests
python3 -m unittest discover -s tests -v
