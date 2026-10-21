#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read-only TBC/JSON checks; decoder flags are not DMA loss measurements."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import statistics


def sequence_diagnostics(fields):
    """Recognize pinned lddecode's reuse of old metadata for inserted filler.

    readfield() writes a prior lastvalidfield tuple before the current field.
    Its metadata retains the prior seqNo; the following flagged field's seqNo
    was calculated before filler insertion. Preserve these facts, not renumber.
    """
    mismatches = {i for i, f in enumerate(fields) if f["seqNo"] != i + 1}
    explained = set()
    insertions = []
    for i in sorted(mismatches):
        if i in explained or i < 2 or i + 1 >= len(fields):
            continue
        filler, current = fields[i:i + 2]
        if (filler == fields[i - 2] and filler["seqNo"] == i - 1
                and current["seqNo"] == i + 1 and current.get("isDuplicateField")
                and current.get("decodeFaults", 0) & 4
                and filler["isFirstField"] != current["isFirstField"]):
            explained.update((i, i + 1))
            insertions.append({"inserted_output_position": i + 1,
                               "reused_seqNo": filler["seqNo"],
                               "following_output_position": i + 2})
    return {"sequence_number_mismatch_count": len(mismatches),
            "recognized_decoder_filler_insertions": insertions,
            "unexplained_sequence_mismatch_positions": [i + 1 for i in sorted(mismatches - explained)]}


def inspect(path):
    data = json.loads(path.read_text())
    video, fields = data["videoParameters"], data["fields"]
    if not fields or not str(path).endswith(".tbc.json"):
        raise ValueError("nonempty .tbc.json metadata required")
    expected_bytes = int(video["fieldWidth"]) * int(video["fieldHeight"]) * 2 * len(fields)
    tbc = Path(str(path)[:-5])
    if not tbc.is_file():
        raise ValueError(f"TBC data file missing: {tbc}")
    chroma = tbc.with_name(tbc.stem + "_chroma.tbc")
    locations = [float(f["diskLoc"]) for f in fields]
    gaps = [b - a for a, b in zip(locations, locations[1:])]
    metrics = [f.get("vitsMetrics", {}).get("bPSNR") for f in fields]
    metrics = [x for x in metrics if isinstance(x, (int, float)) and math.isfinite(x)]
    faults = [{"seqNo": f["seqNo"], "bitmap": f["decodeFaults"]}
              for f in fields if f.get("decodeFaults", 0)]
    files = {}
    for file in (tbc, chroma):
        if file.exists():
            size = file.stat().st_size
            files[file.name] = {"bytes": size, "expected_bytes": expected_bytes,
                                "bytes_match_field_count": size == expected_bytes}
    return {
        "video_parameters": video,
        "field_count": len(fields),
        "declared_count_matches": video.get("numberOfSequentialFields") == len(fields),
        "sequence_numbers_contiguous": [f["seqNo"] for f in fields] == list(range(1, len(fields) + 1)),
        **sequence_diagnostics(fields),
        "sync_confidence_histogram": dict(Counter(f.get("syncConf") for f in fields)),
        "duplicate_field_flags": sum(bool(f.get("isDuplicateField")) for f in fields),
        "nonzero_decode_fault_bitmaps": faults,
        "decode_fault_keys_present": sum("decodeFaults" in f for f in fields),
        "same_output_parity_neighbors": sum(a["isFirstField"] == b["isFirstField"]
                                            for a, b in zip(fields, fields[1:])),
        "source_location_first": locations[0], "source_location_last": locations[-1],
        "source_location_gap_min": min(gaps) if gaps else None,
        "source_location_gap_max": max(gaps) if gaps else None,
        "source_location_gaps_outside_0_8_to_1_2": sum(x < .8 - 1e-6 or x > 1.2 + 1e-6 for x in gaps),
        "fields_with_detected_dropouts": sum(bool(f.get("dropOuts", {}).get("fieldLine")) for f in fields),
        "detected_dropout_segments": sum(len(f.get("dropOuts", {}).get("fieldLine", [])) for f in fields),
        "bPSNR_metric_median": statistics.median(metrics) if metrics else None,
        "tbc_files": files,
        "limits": "Rounded source locations, decoder confidence, dropout detection and bPSNR are decoder diagnostics; they do not prove zero DMA loss, calibrated SNR or perfect time-base correction.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("metadata", type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.metadata), indent=2))
