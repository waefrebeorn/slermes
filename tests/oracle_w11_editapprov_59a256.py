#!/usr/bin/env python3
"""Differential oracle for acp_adapter/edit_approval.py (gate w11_editapprov_59a256)."""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/acp_adapter/edit_approval.py"
spec = importlib.util.spec_from_file_location("w11_editapprov_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_editapprov_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")
    try:
        if func == "extract":
            want = live._extract_v4a_patch_paths(r.get("patch_body", ""))
        elif func == "path_is_sensitive":
            want = live._path_is_sensitive(Path(r.get("path", "")))
        elif func == "denied":
            want = live._denied(r.get("message", ""))
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_editapprov_59a256: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
