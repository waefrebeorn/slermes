#!/usr/bin/env python3
"""Differential oracle for gate w16_fileoperat_7d3fcc (tools/file_operations_native.py)."""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w16fno_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/file_operations_native.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w16_fileoperat_7d3fcc"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    path = inp.get("path", "")
    seed = inp.get("seed_hex")
    if seed is None:
        try:
            os.remove(path)
        except OSError:
            pass
    else:
        with open(path, "wb") as fh:
            fh.write(bytes.fromhex(seed))
    if func == "probe":
        t = live.probe(path, bool(inp.get("want_text")))
        if t is None:
            return None
        return [bool(t[0]), t[1].hex() if t[1] is not None else None]
    if func == "sha256_of":
        return live.sha256_of(path)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
