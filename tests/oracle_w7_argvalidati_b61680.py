#!/usr/bin/env python3
"""Differential oracle for tools/arg_validation.py (gate w7_argvalidati_b61680)."""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import tools  # noqa: F401  (parent-package bootstrap)
spec = importlib.util.spec_from_file_location("tools.arg_validation", Q + "/tools/arg_validation.py")
live = importlib.util.module_from_spec(spec)
sys.modules["tools.arg_validation"] = live
spec.loader.exec_module(live)


def norm(s):
    try:
        return json.loads(s)
    except (ValueError, TypeError):
        return s


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "with_choices":
            py = live._with_choices(r.get("key", ""), r.get("properties", {}))
        elif func == "missing_unknown":
            entry = types.SimpleNamespace(compat_args=r.get("compat_args", []),
                                          self_checked_args=r.get("self_checked_args", []))
            py = list(live._missing_and_unknown(entry, r.get("params", {}), r.get("args", {})))
            py = [list(py[0]), list(py[1])]
        elif func == "flag_unknown_args":
            py = norm(live.flag_unknown_args(r.get("result", ""), r.get("name", ""),
                                             r.get("unknown", []), r.get("valid", "")))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if func == "flag_unknown_args":
        got = norm(got)
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_argvalidati_b61680: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
