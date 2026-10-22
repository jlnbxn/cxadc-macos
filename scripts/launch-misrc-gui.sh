#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
gui="${MISRC_GUI_SOURCE_DIR:-$root/../MISRC-GUI}"
exe="$gui/build-local/misrc_gui"
test -x "$exe" || { echo 'Build MISRC-GUI first; see docs/MISRC-GUI.md.' >&2; exit 1; }
mkdir -p "$root/build" "$root/artifacts/misrc-gui-recordings"
config="$root/build/misrc-gui-settings.json"
if test ! -f "$config"; then
  python3 - "$config" "$root/artifacts/misrc-gui-recordings" <<'PY'
import json, sys
with open(sys.argv[1], 'x') as stream:
    json.dump(dict(output_path=sys.argv[2], output_base_name='cxadc',
                   auto_names_enabled=True, append_timestamp_on_capture_start=True,
                   overwrite_files=False, capture_a=True, capture_b=False,
                   rf_bits_a=8, use_flac=False, enable_resample_a=False,
                   net_mode=0, stop_on_dropout=True), stream, indent=2)
PY
fi
export MISRC_GUI_NO_ADMIN=1
export CXADC_CLIENT_LIBRARY="$root/build/libcxadc-client.dylib"
exec "$exe" --config "$config" "$@"
