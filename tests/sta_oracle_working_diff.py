#!/usr/bin/env python3
"""
sta_oracle_working_diff.py — Python oracle for tools/working_diff.py,
ported in src/tools/port_working_diff.c.

Imports the REAL tools.working_diff module. Reads cases.in (REPO token =
fixture dir), runs collect_working_diff for each case, emits one JSON object
per line matching the C harness's output shape:

  {"success", "error", "empty", "stat", "diff", "untracked"}
"""
import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.working_diff import collect_working_diff  # noqa: E402


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_working_diff.py <cases.in>\n")
        return 2
    fixture_dir = os.environ.get("ORACLE_FIXDIR")
    if not fixture_dir:
        fixture_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
    repo_dir = os.path.join(fixture_dir, "repo")
    with open(sys.argv[1], "r", encoding="utf-8") as fp:
        for raw in fp:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            if line.startswith("REPO"):
                line = repo_dir + line[4:]
            parts = line.split()
            cwd = parts[0]
            mode = parts[1] if len(parts) > 1 else "working"
            paths = [p for p in parts[2:] if p != "--stat"]
            r = collect_working_diff(cwd, mode=mode, paths=paths or None)
            if not r.get("success"):
                sys.stdout.write(json.dumps({
                    "success": False,
                    "error": r.get("error", ""),
                    "empty": False,
                    "stat": "",
                    "diff": "",
                    "untracked": [],
                }, separators=(",", ":")) + "\n")
                continue
            sys.stdout.write(json.dumps({
                "success": True,
                "error": None,
                "empty": bool(r.get("empty")),
                "stat": r.get("stat", ""),
                "diff": r.get("diff", ""),
                "untracked": r.get("untracked", []),
            }, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
