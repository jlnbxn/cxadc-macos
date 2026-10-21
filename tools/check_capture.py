#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Inspect a real capture and its sidecar; no sample generation or modification."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def inspect(path):
    metadata = json.loads(Path(str(path) + ".json").read_text())
    hashes = Counter()
    adjacent = 0
    previous = None
    histogram = Counter()
    chunks = []
    size = 0
    with path.open("rb") as stream:
        while block := stream.read(262144):
            size += len(block)
            histogram.update(block)
            if len(block) == 262144:
                digest = hashlib.sha256(block).hexdigest()
                hashes[digest] += 1
                adjacent += digest == previous
                previous = digest
            chunks.append({"bytes": len(block), "minimum": min(block), "maximum": max(block), "mean": sum(block) / len(block)})
    return {
        "file_bytes": size,
        "metadata_bytes_agree": size == metadata["written_bytes"],
        "capture_state": metadata["state"],
        "full_chunks": sum(hashes.values()),
        "unique_full_chunk_hashes": len(hashes),
        "repeated_full_chunks": sum(n - 1 for n in hashes.values()),
        "adjacent_identical_chunks": adjacent,
        "minimum": min(histogram) if histogram else None,
        "maximum": max(histogram) if histogram else None,
        "all_zero": bool(size) and histogram[0] == size,
        "mean": sum(code * n for code, n in histogram.items()) / size if size else None,
        "chunk_mean_min": min((c["mean"] for c in chunks), default=None),
        "chunk_mean_max": max((c["mean"] for c in chunks), default=None),
        "stream": metadata.get("driver", {}).get("stream"),
        "limits": "Hashes and counters can expose faults, but cannot prove analog sample continuity or calibrated sampling rate. Constant input can legitimately repeat blocks.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.file), indent=2))
