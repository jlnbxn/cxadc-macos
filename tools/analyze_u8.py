#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Offline unsigned-byte statistics; never opens a capture device."""
import argparse
from collections import Counter
import json
import math
import os
import sys


def analyze(path, sample_rate=None):
    histogram = [0] * 256
    with open(path, "rb") as stream:
        before = os.fstat(stream.fileno())
        while block := stream.read(1024 * 1024):
            for code, count in Counter(block).items():
                histogram[code] += count
        after = os.fstat(stream.fileno())
    count = sum(histogram)
    if not count:
        raise ValueError("empty input: no sample statistics")
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns) or count != after.st_size:
        raise ValueError("file changed during analysis or is not a stable regular sample file")
    low = next(i for i, n in enumerate(histogram) if n)
    high = next(i for i in range(255, -1, -1) if histogram[i])
    mean = sum(i * n for i, n in enumerate(histogram)) / count
    variance = sum((i - mean) ** 2 * n for i, n in enumerate(histogram)) / count
    result = {
        "file_size_bytes": count, "sample_count": count, "format": "unsigned_8_bit",
        "minimum": low, "maximum": high, "mean_codes": mean,
        "standard_deviation_codes": math.sqrt(variance),
        "signal_rms_ac_codes": math.sqrt(variance),
        "rms_including_dc_codes": math.sqrt(sum(i * i * n for i, n in enumerate(histogram)) / count),
        "histogram": histogram,
        "samples_at_0": histogram[0], "percent_at_0": 100 * histogram[0] / count,
        "samples_at_255": histogram[255], "percent_at_255": 100 * histogram[255] / count,
        "peak_to_peak_codes": high - low,
        "approximate_span_db_relative_to_one_code": 20 * math.log10(high - low) if high > low else None,
        "interpretation": "Code statistics only; span is not SNR, ENOB or calibrated analog dynamic range. Statistics do not prove correct RF capture.",
    }
    if sample_rate is not None:
        result["assumed_sample_rate_hz"] = sample_rate
        result["inferred_duration_seconds"] = count / sample_rate
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file")
    parser.add_argument("--sample-rate", type=float, help="explicit assumed rate in Hz, not a measurement")
    args = parser.parse_args()
    if args.sample_rate is not None and (not math.isfinite(args.sample_rate) or args.sample_rate <= 0):
        parser.error("sample rate must be finite and positive")
    try:
        json.dump(analyze(args.file, args.sample_rate), sys.stdout, indent=2, allow_nan=False)
        print()
        return 0
    except (OSError, ValueError) as error:
        print(f"analysis failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
