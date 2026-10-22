#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
gui="${1:-$root/../MISRC-GUI}"
gui="$(cd "$gui" && pwd)"
test -f "$gui/misrc_tools/misrc_gui/input/gui_cxadc_macos.c" || {
  echo 'This checkout needs the macOS CXADC backend; see docs/MISRC-GUI.md.' >&2; exit 1;
}
"$root/scripts/build-client.sh"
cmp "$root/client/cxadc_client.h" "$gui/misrc_tools/misrc_gui/input/cxadc_client.h"
(cd "$gui" && scripts/build-local.sh)
cp "$root/build/libcxadc-client.dylib" "$gui/build-local/libcxadc-client.dylib"
codesign --force --sign - --timestamp=none --entitlements "$root/src/CLI.entitlements" "$gui/build-local/misrc_gui"
"$gui/build-local/misrc_gui" --smoke-test
echo "Ready: $gui/build-local/misrc_gui"
