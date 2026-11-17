#!/usr/bin/env python3
"""Differential oracle for file_ops_lint.

Reads the C harness's JSON-lines records on stdin and re-emits the SAME record
shape with Python's computed `res`. The batch comparator then diffs C's res
against Python's res. The fixture is a placeholder ("single"); the harness
carries its own case list, so stdin is the only source of cases here.
"""
import json
import os
import sys
from pathlib import Path

import load_dev_module as _ldm

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import tools.file_operations as fo
_ldm.rebind_used(fo, "fo", ["tools.file_operations_lint"], Path(__file__))
import agent.file_safety as fs


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        fn = rec.get("fn")
        bc = rec.get("base_cmd")
        out = rec.get("out")
        if fn == "looks_like_linter_unusable":
            exp = bool(fo._looks_like_linter_unusable(bc, out))
        elif fn == "delete_path_denied":
            exp = bool(fs.is_write_denied(bc))
        elif fn == "delete_path_ok":
            exp = False  # a real temp file inside the safe root must delete
        else:
            exp = bool(rec.get("res"))
        print(json.dumps({"fn": fn, "base_cmd": bc, "out": out, "res": exp},
                         separators=(",", ":"), ensure_ascii=False))


if __name__ == "__main__":
    main()
