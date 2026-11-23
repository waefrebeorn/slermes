#!/usr/bin/env python3
"""Differential oracle for hermes_constants_scratch_detach.py (w5_hermesconst_e6f362)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/hermes_constants_scratch_detach.py"
spec = importlib.util.spec_from_file_location("hcsdetach_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hcsdetach_live"] = live
spec.loader.exec_module(live)


def decode_like_child(lineage):
    """The expression in run(): None for '-', else a frozenset of (pid, started)."""
    try:
        return None if lineage == "-" else frozenset(
            (int(pid), float(started))
            for pid, started in (item.split(":") for item in lineage.split(","))
        )
    except Exception:
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
    func = r.get("func")
    got = r.get("out")

    if func == "encode":
        raw = r.get("chain")
        chain = None if raw in (None, "null") or (isinstance(raw, str) and raw == "null") \
            else [(int(p), float(s)) for p, s in raw]
        py = live._encode_lineage(chain)
    elif func == "decode":
        expected = decode_like_child(r.get("lineage", ""))
        if expected is None:
            py = None
        else:
            py = [[p, s] for p, s in expected]
        # C emits a JSON array; compare as sets of (pid, float) tuples.
        if isinstance(got, list):
            got_set = frozenset((int(p), float(s)) for p, s in got)
        else:
            got_set = None
        py_set = None if py is None else frozenset((int(p), float(s)) for p, s in py)
        cases += 1
        if py_set != got_set:
            mism += 1
            print(f"MISMATCH decode args={r} py={py_set!r} c={got_set!r}", file=sys.stderr)
        continue
    elif func == "child_script":
        py = live._CHILD
    else:
        continue

    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_hermesconst_e6f362: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
