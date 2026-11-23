#!/usr/bin/env python3
"""Oracle for plugins/google_meet/_jsonfile.py:read_json.

Writes the same fixture-described file under .testbin/ that the C harness
writes, then compares read_json's parsed value to the harness output.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = "C:/Users/eman5/hermes-agent"
SRC = ROOT + "/plugins/google_meet/_jsonfile.py"
spec = importlib.util.spec_from_file_location("gmeetjson_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


def prepare(name, create, content, bom):
    path = os.path.join(".testbin", name)
    if not create:
        try:
            os.remove(path)
        except OSError:
            pass
        return
    data = content.encode("utf-8")
    if bom:
        data = b"\xef\xbb\xbf" + data
    with open(path, "wb") as f:
        f.write(data)


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
    if func != "read_json":
        continue
    name = r.get("path", "")
    prepare(name, bool(r.get("create", True)), r.get("content", ""),
            bool(r.get("bom", False)))
    path = os.path.join(".testbin", name)
    py = live.read_json(Path(path))

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE gmeetjson: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
