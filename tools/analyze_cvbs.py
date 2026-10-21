#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read-only, approximate CVBS timing/spectrum diagnostics. Requires NumPy."""
import argparse
import json
from pathlib import Path
import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("file", type=Path)
parser.add_argument("--sample-rate", type=float, default=28636363)
parser.add_argument("--sync-threshold", type=float, default=80)
parser.add_argument("--plot", type=Path)
args = parser.parse_args()
fs = args.sample_rate
if not np.isfinite(fs) or fs <= 0 or not np.isfinite(args.sync_threshold) or not 0 <= args.sync_threshold <= 255:
    parser.error("positive finite rate and threshold in 0..255 required")
x = np.fromfile(args.file, np.uint8, count=int(fs))
if len(x) < 100000:
    parser.error("at least 100000 samples required")
below = x < args.sync_threshold
edges = np.diff(np.r_[False, below, False].astype(np.int8))
starts, ends = np.flatnonzero(edges == 1), np.flatnonzero(edges == -1)
widths = ends - starts
horizontal = starts[(widths > fs * 3e-6) & (widths < fs * 6e-6)]
gaps = np.diff(horizontal)
ordinary = gaps[(gaps > fs * 60e-6) & (gaps < fs * 68e-6)]
n = min(2**22, len(x))
freq = np.fft.rfftfreq(n, 1 / fs)
power = np.abs(np.fft.rfft((x[:n].astype(float) - np.mean(x[:n])) * np.hanning(n))) ** 2
def peak(low, high):
    indices = np.flatnonzero((freq >= low) & (freq <= high))
    index = indices[np.argmax(power[indices])]
    return float(freq[index])
positions = np.flatnonzero((widths > fs * 3e-6) & (widths < fs * 6e-6))
sync = [np.mean(x[starts[i]+20:ends[i]-20]) for i in positions[:2000] if widths[i] > 40]
blank = [np.mean(x[ends[i]+8:ends[i]+24]) for i in positions[:2000] if ends[i]+24 < len(x)]
report = {
    "assumed_sample_rate_hz": fs, "threshold_code": args.sync_threshold,
    "samples_analyzed": len(x), "horizontal_candidates": len(horizontal),
    "mean_line_gap_samples": float(np.mean(ordinary)) if len(ordinary) else None,
    "mean_line_period_us": float(np.mean(ordinary) / fs * 1e6) if len(ordinary) else None,
    "inferred_horizontal_hz": float(fs / np.mean(ordinary)) if len(ordinary) else None,
    "horizontal_spectral_peak_hz": peak(15000, 16500),
    "chroma_band_peak_hz": peak(4.3e6, 4.55e6),
    "sync_tip_window_mean_code": float(np.mean(sync)) if sync else None,
    "back_porch_window_mean_code": float(np.mean(blank)) if blank else None,
    "max_low_pulse_us": float(np.max(widths) / fs * 1e6) if len(widths) else None,
    "limits": "Manual threshold; assumed clock; diagnostic windows are not a calibrated PAL measurement or continuity proof.",
}
print(json.dumps(report, indent=2))
if args.plot:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 2, figsize=(12, 7), layout="constrained")
    axes[0, 0].plot(np.arange(12000)/fs*1e6, x[:12000], linewidth=.65)
    axes[0, 0].axhline(args.sync_threshold, color="red", linestyle="--", linewidth=.6)
    axes[0, 0].set(xlabel="Time (us)", ylabel="ADC code", title="Raw samples: horizontal timing")
    broad = np.flatnonzero(widths > fs*20e-6)
    begin = max(0, int(starts[broad[0]])-5000) if len(broad) else 0
    end = min(len(x), begin+30000)
    axes[0, 1].plot(np.arange(end-begin)/fs*1e6, x[begin:end], linewidth=.6)
    axes[0, 1].set(xlabel="Time (us)", ylabel="ADC code", title="Raw samples around a broad sync pulse")
    axes[1, 0].bar(np.arange(256), np.bincount(x, minlength=256), width=1)
    axes[1, 0].set(xlabel="ADC code", ylabel="Samples", title="Histogram (first nominal second)")
    axes[1, 1].plot(freq/1e6, 10*np.log10(np.maximum(power, 1e-20)/np.max(power)), linewidth=.6)
    axes[1, 1].axvline(4.43361875, color="red", linestyle="--", linewidth=.6)
    axes[1, 1].set(xlabel="Frequency (MHz, nominal clock)", ylabel="Relative power (dB)", title="Spectrum (analysis only)", ylim=(-100, 5))
    fig.suptitle("Real CX23880 raw CVBS diagnostics - no signal changes in capture")
    fig.savefig(args.plot, dpi=150)
