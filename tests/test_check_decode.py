#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Metadata fixtures only; these are not simulated ADC captures."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_decode import sequence_diagnostics


def normal():
    return [dict(seqNo=i + 1, isFirstField=i % 2 == 0, diskLoc=i + .5)
            for i in range(8)]


def with_filler():
    fields = normal()
    fields[4] = copy.deepcopy(fields[2])
    fields[5].update(seqNo=5, isDuplicateField=True, decodeFaults=4)
    return fields


class SequenceChecks(unittest.TestCase):
    def test_normal(self):
        self.assertEqual(sequence_diagnostics(normal())["sequence_number_mismatch_count"], 0)

    def test_pinned_decoder_filler(self):
        fields = with_filler()
        before = copy.deepcopy(fields)
        result = sequence_diagnostics(fields)
        self.assertEqual(result["sequence_number_mismatch_count"], 2)
        self.assertEqual(len(result["recognized_decoder_filler_insertions"]), 1)
        self.assertEqual(result["unexplained_sequence_mismatch_positions"], [])
        self.assertEqual(fields, before)

    def test_unflagged_pair_rejected(self):
        fields = with_filler()
        del fields[5]["isDuplicateField"]
        self.assertEqual(sequence_diagnostics(fields)["unexplained_sequence_mismatch_positions"], [5, 6])

    def test_changed_reused_metadata_rejected(self):
        fields = with_filler()
        fields[4]["diskLoc"] += .1
        self.assertEqual(sequence_diagnostics(fields)["unexplained_sequence_mismatch_positions"], [5, 6])

    def test_unrelated_mismatch_remains_visible(self):
        fields = with_filler()
        fields[7]["seqNo"] = 42
        self.assertEqual(sequence_diagnostics(fields)["unexplained_sequence_mismatch_positions"], [8])


if __name__ == "__main__":
    unittest.main()
