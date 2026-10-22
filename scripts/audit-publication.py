#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Heuristic source/Git audit. Report locations and commit IDs, never matched values.

This is a review aid, not a guarantee that a repository contains no private data.
Ignored local media, dependencies and signing assets are deliberately not exported.
"""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
PATTERNS = {
    "personal_home_path": re.compile(rb"/" + rb"Users/[^/\s`\"']+"),
    "local_volume_path": re.compile(rb"/" + rb"Volumes/[^/\s`\"']+"),
    "private_key": re.compile(rb"-----BEGIN (?:[A-Z ]+ )?PRIVATE KEY-----"),
    "access_token": re.compile(rb"(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|AKIA[A-Z0-9]{16}|sk-[A-Za-z0-9_-]{30,})"),
    "developer_team_setting": re.compile(rb"(?:DEVELOPMENT_TEAM|TeamIdentifier)\s*(?:=|:)\s*[A-Z0-9]{10}"),
    "device_uuid": re.compile(rb"\b[0-9A-Fa-f]{8}-(?:[0-9A-Fa-f]{4}-){3}[0-9A-Fa-f]{12}\b"),
    "email": re.compile(rb"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}"),
    "credential_assignment": re.compile(rb"(?i)(?:password|passwd|api[_-]?key|client[_-]?secret|access[_-]?token)\s*[:=]\s*[\"'][^\"'\r\n]{8,}[\"']"),
    "phone_candidate": re.compile(rb"(?<!\w)\+\d{1,3}[ .-](?:\d[ .-]?){7,14}\b"),
}
MEDIA = {".u8", ".rf", ".raw", ".tbc", ".flac", ".wav", ".mkv", ".mp4", ".mov", ".avi"}
SIGNING = {".p12", ".pfx", ".pem", ".key", ".cer", ".crt", ".der", ".mobileprovision", ".provisionprofile"}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def findings(path, data):
    result = []
    for kind, pattern in PATTERNS.items():
        if pattern.search(data):
            if kind == "email" and path == "THIRD_PARTY_NOTICES.md":
                continue  # Reviewed, retained upstream copyright/contact notices.
            result.append(kind)
    if b"\0" in data:
        result.append("binary_blob")
    if len(data) > 1_000_000:
        result.append("oversized_blob")
    if Path(path).suffix.lower() in MEDIA | SIGNING:
        result.append("media_or_signing_asset")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", action="store_true", help="also inspect all reachable refs/commit snapshots and identities")
    args = parser.parse_args()
    paths = git("ls-files", "--cached", "--others", "--exclude-standard", "-z").split(b"\0")
    current = []
    for item in sorted(set(paths)):
        if not item:
            continue
        path = item.decode()
        file = ROOT / path
        if file.is_symlink():
            current.append({"path": path, "kind": "symlink_requires_review"})
        elif file.is_file():
            current.extend({"path": path, "kind": kind} for kind in findings(path, file.read_bytes()))
    report = {"scope": "tracked and nonignored untracked source; ignored private data excluded",
              "current_findings": current, "history_findings": [], "identity_review_commits": []}
    broken = []
    for item in paths:
        if not item or not item.endswith(b".md"):
            continue
        path = ROOT / item.decode()
        if not path.is_file():
            continue
        for target in re.findall(r"\[[^\]]*\]\(([^)]+)\)", path.read_text()):
            target = target.strip().strip("<>")
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            destination = path.parent / unquote(parsed.path)
            if not destination.exists():
                broken.append({"path": item.decode(), "target": target})
    report["broken_local_markdown_links"] = broken
    for line in git("ls-files", "--stage").decode().splitlines():
        if line.startswith("160000 "):
            report["current_findings"].append({"kind": "embedded_gitlink", "path": line.split("\t", 1)[1]})
    if args.history:
        blobs = defaultdict(lambda: {"paths": set(), "commits": set()})
        commits = git("rev-list", "--all").decode().splitlines()
        report["reachable_commit_count"] = len(commits)
        for commit in commits:
            for line in git("ls-tree", "-r", "-z", commit).split(b"\0"):
                if not line:
                    continue
                header, path = line.split(b"\t", 1)
                mode, kind, oid = header.decode().split()
                if kind != "blob":
                    report["history_findings"].append({"kind": "embedded_gitlink", "path": path.decode(), "commits": [commit]})
                    continue
                blobs[oid]["paths"].add(path.decode())
                blobs[oid]["commits"].add(commit)
            metadata = git("show", "-s", "--format=%an%x00%ae%x00%cn%x00%ce%x00%B", commit).split(b"\0", 4)
            # Names/emails are public in Git even if they are absent from source.
            if metadata[1] or metadata[3]:
                report["identity_review_commits"].append(commit)
            for kind, pattern in PATTERNS.items():
                if pattern.search(metadata[4]):
                    report["history_findings"].append({"kind": "commit_message_" + kind, "commits": [commit]})
        report["unique_historical_blobs"] = len(blobs)
        for oid, record in blobs.items():
            data = git("cat-file", "blob", oid)
            for path in sorted(record["paths"]):
                for kind in findings(path, data):
                    report["history_findings"].append({"kind": kind, "path": path, "blob": oid, "commits": sorted(record["commits"])})
    json.dump(report, sys.stdout, indent=2)
    print()
    return int(bool(report["current_findings"] or report["history_findings"] or broken))


if __name__ == "__main__":
    sys.exit(main())
