#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check the actual attachment snapshot; does not initiate hardware reads/writes."""
import json
from pathlib import Path
import subprocess
import sys

try:
    cli = Path(__file__).resolve().parents[1] / "build/cxadc-capture"
    output = subprocess.run([str(cli), "--info"], capture_output=True, text=True, check=True)
    devices = json.loads(output.stdout)["devices"]
    if len(devices) != 1:
        raise ValueError("expected exactly one compatible endpoint")
    device = devices[0]
    probe = device.get("attachment_probe", {})
    if not device["cxadc_probe_attached"] or not probe.get("mmio_valid") or probe.get("api_version") != 2:
        raise ValueError("v2 register probe is not attached with valid reads")
    command = int(probe["pci_command"], 16)
    if command & 4 or command != int(probe["pci_command_after_probe"], 16):
        raise ValueError("bus mastering was enabled or command restoration failed")
    regs = {k: int(v["value"], 16) for k, v in probe["registers"].items()}
    if regs["MO_VID_DMACNTRL"] or regs["MO_PCI_INTMSK"] or regs["MO_VID_INTMSK"]:
        raise ValueError("snapshot shows video DMA or interrupt masks enabled")
    print("Actual attachment snapshot: register reads passed; command restored; bus mastering, video DMA and interrupt masks off.")
    print("DEV_CNTRL2 is write-only; its readback cannot establish RISC state.")
    print("This is an attachment-time snapshot, not live state or a DMA/interrupt delivery test.")
except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
    print(f"probe check failed: {error}", file=sys.stderr)
    sys.exit(1)
