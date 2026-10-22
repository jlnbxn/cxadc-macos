#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Decode stock-rate PAL S-VHS with disk guards; never delete source/TBCs."""
import argparse
import datetime
import json
import math
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

from check_decode import inspect

ROOT = Path(__file__).resolve().parents[1]
RESERVE = 20_000_000_000


def executable(requested, name, local_fallback):
    candidate = shutil.which(requested or name)
    if candidate is None and not requested and local_fallback.is_file():
        candidate = str(local_fallback)
    if candidate is None or not os.access(candidate, os.X_OK):
        raise ValueError(f"cannot find executable {requested or name}; install it or supply its explicit path")
    return str(Path(candidate).resolve())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw", type=Path)
    parser.add_argument("tbc_prefix", type=Path)
    parser.add_argument("export_prefix", type=Path)
    parser.add_argument("--export-existing", action="store_true",
                        help="validate existing TBCs and export without repeating decode; keep previous workflow state")
    parser.add_argument("--decoder", help="vhs-decode executable; default PATH, then optional local build environment")
    parser.add_argument("--exporter", help="tbc-video-export executable; default PATH, then optional local app bundle")
    args = parser.parse_args()
    raw, tbc, export = (p.resolve() for p in (args.raw, args.tbc_prefix, args.export_prefix))
    metadata = json.loads(Path(str(raw) + ".json").read_text())
    raw_stat = raw.stat()
    if metadata["state"] != "complete" or metadata["written_bytes"] != raw_stat.st_size:
        raise ValueError("raw capture is incomplete or its byte count disagrees")
    if metadata.get("format") != "u8" or metadata["nominal_sample_rate_hz"] != 28636363:
        raise ValueError("this runner requires stock-rate 28.636363 MHz unsigned 8-bit input")
    decoder = None if args.export_existing else executable(
        args.decoder, "vhs-decode", ROOT / "build/decode-venv/bin/vhs-decode")
    exporter = executable(args.exporter, "tbc-video-export", ROOT / "build/tbc-tools.app/Contents/MacOS/tbc-video-export")
    if shutil.which("ffprobe") is None:
        raise ValueError("ffprobe must be installed separately and available on PATH")
    duration = raw_stat.st_size / metadata["nominal_sample_rate_hz"]
    for prefix in (tbc, export):
        if not prefix.parent.is_dir():
            raise ValueError(f"output directory missing: {prefix.parent}")
    state_path = Path(str(export) + (".export-workflow.json" if args.export_existing else ".workflow.json"))
    new_outputs = [str(export) + ".mkv", str(state_path), str(export) + ".console.txt"]
    if not args.export_existing:
        new_outputs += [str(tbc) + suffix for suffix in (".tbc", "_chroma.tbc", ".tbc.json", ".log", ".console.txt")]
    for name in new_outputs:
        if Path(name).exists():
            raise ValueError(f"refusing existing output: {name}")
    tbc_budget = 0 if args.export_existing else math.ceil(duration * 50 + 2000) * 1135 * 313 * 2 * 2
    export_budget = math.ceil(raw_stat.st_size * .65)
    if tbc.parent.stat().st_dev == export.parent.stat().st_dev:
        required = [(tbc.parent, tbc_budget + export_budget + RESERVE)]
    else:
        required = [(tbc.parent, tbc_budget + RESERVE), (export.parent, export_budget + RESERVE)]
    for directory, needed in required:
        if shutil.disk_usage(directory).free < needed:
            raise ValueError(f"insufficient preflight headroom on {directory}: need {needed} bytes")
    state = {"state": "prepared", "raw": str(raw), "raw_bytes": raw_stat.st_size,
             "nominal_duration_seconds": duration, "tbc_prefix": str(tbc),
             "export_prefix": str(export), "reserve_bytes": RESERVE,
             "dropout_correction": "standard exporter default; no --no-dropout-correct",
             "cleanup": "none; raw, TBCs, metadata and exports retained",
             "commands": [], "pid": os.getpid()}
    child = None

    def save(phase):
        state["state"] = phase
        state["updated_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        temp = state_path.with_suffix(".tmp")
        temp.write_text(json.dumps(state, indent=2) + "\n")
        temp.replace(state_path)

    def interrupt(signum, frame):
        raise KeyboardInterrupt(f"workflow received signal {signum}")

    signal.signal(signal.SIGTERM, interrupt)

    def run(command, logfile, phase, output_dir):
        nonlocal child
        state["commands"].append(command)
        save(phase)
        with logfile.open("x") as log:
            child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                     cwd=ROOT, env={**os.environ, "MPLCONFIGDIR": str(ROOT / "build/mpl-cache")},
                                     start_new_session=True)
            while child.poll() is None:
                if shutil.disk_usage(output_dir).free < RESERVE:
                    raise RuntimeError(f"disk guard: less than {RESERVE} bytes free on {output_dir}")
                time.sleep(5)
            code = child.returncode
            child = None
            if code:
                raise RuntimeError(f"{phase} exited {code}; see {logfile}")

    try:
        if not args.export_existing:
            run([decoder, "--frequency", "28.636363",
                 "--system", "PAL", "--tape_format", "SVHS", "--use_saved_levels",
                 "--threads", "2", str(raw), str(tbc)],
                Path(str(tbc) + ".console.txt"), "decoding", tbc.parent)
        result = inspect(Path(str(tbc) + ".tbc.json"))
        state["decode_check"] = result
        if not result["declared_count_matches"] or result["unexplained_sequence_mismatch_positions"]:
            raise RuntimeError("decoded field count or unexplained sequence mismatch validation failed")
        if result["same_output_parity_neighbors"]:
            raise RuntimeError("decoded field parity does not alternate")
        if len(result["tbc_files"]) != 2 or not all(v["bytes_match_field_count"] for v in result["tbc_files"].values()):
            raise RuntimeError("luma/chroma TBC byte-count validation failed")
        # Dropout correction is enabled by omitting the diagnostic disable flag.
        run([exporter,
             "--profile", "ffv1", "--threads", "4", str(tbc) + ".tbc", str(export)],
            Path(str(export) + ".console.txt"), "exporting", export.parent)
        probe = json.loads(subprocess.check_output([
            "ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
            "stream=width,height,r_frame_rate,codec_name:format=duration,size", "-of", "json",
            str(export) + ".mkv"], text=True))
        state["export_probe"] = probe
        expected = result["field_count"] / 50
        if not probe["streams"] or abs(float(probe["format"]["duration"]) - expected) > .1:
            raise RuntimeError("export duration does not match decoded PAL field count")
        if (raw.stat().st_size, raw.stat().st_mtime_ns) != (raw_stat.st_size, raw_stat.st_mtime_ns):
            raise RuntimeError("raw file size or modification time changed")
        save("complete_requires_visual_review")
    except BaseException as error:
        if child is not None and child.poll() is None:
            os.killpg(child.pid, signal.SIGINT)
            try:
                child.wait(timeout=30)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
        state["error"] = str(error)
        save("failed")
        raise


if __name__ == "__main__":
    main()
