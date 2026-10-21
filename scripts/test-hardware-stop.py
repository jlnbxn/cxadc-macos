#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Explicit hardware test: captures real ADC bytes, then signals only its child CLI."""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("mode", choices=["interrupt", "kill", "stall"])
parser.add_argument("--hardware", action="store_true", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
cli = root / "build/cxadc-capture"
output = root / f"artifacts/stop-{args.mode}-{time.time_ns()}.u8"
with Path(str(output) + ".stderr").open("w") as log:
    child = subprocess.Popen([str(cli), "--output", str(output), "--vmux", "1", "--level", "0"], stderr=log)
    try:
        time.sleep(0.8)
        if child.poll() is not None:
            raise RuntimeError("capture stopped before test signal")
        if args.mode == "stall":
            child.send_signal(signal.SIGSTOP)
            time.sleep(3)
            child.send_signal(signal.SIGCONT)
        else:
            child.send_signal(signal.SIGINT if args.mode == "interrupt" else signal.SIGKILL)
        code = child.wait(timeout=10)
    finally:
        if child.poll() is None:
            child.send_signal(signal.SIGCONT)
            child.kill()
            child.wait(timeout=10)
time.sleep(0.15)
status = subprocess.run([str(cli), "--status"], capture_output=True, text=True, check=True)
state = json.loads(status.stdout)
assert state["command_before"] == "0x0" and state["command_restored"] == "0x0", state
assert state["video_dma"] == "0x0" and not state["quarantined"], state
assert state["stream"]["stop_verified"] and not state["stream"]["running"], state
metadata = json.loads(Path(str(output) + ".json").read_text())
if args.mode == "interrupt":
    assert code == 0 and metadata["state"] == "complete" and metadata["stopped_by_signal"], metadata
elif args.mode == "kill":
    assert code == -signal.SIGKILL and metadata["state"] == "incomplete", metadata
else:
    assert code != 0 and metadata["state"] == "failed" and state["stream"]["overrun_events"] > 0, metadata
print(json.dumps({"mode": args.mode, "child_exit": code, "file": str(output), "bytes": output.stat().st_size,
                  "metadata_state": metadata["state"], "stream": state["stream"]}, indent=2))
