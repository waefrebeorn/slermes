#!/usr/bin/env python3
"""Differential oracle for tools/approval_shell_tokens.py (gate w15_approvalsh_f4acbf)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

NAME = "tools.approval_shell_tokens_live"
spec = importlib.util.spec_from_file_location(NAME, QUARRY + "/tools/approval_shell_tokens.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "backtick_end":
        return live._backtick_end_from(r.get("segment", ""), int(r.get("i", 0)))
    if f == "shell_tokens":
        res = live._shell_tokens_with_spans(r.get("segment", ""), int(r.get("start", 0)))
        if res is None:
            return None
        return [[v, s, e, bool(inert)] for (v, s, e, inert) in res]
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
        print(f"ORACLE w15_approvalsh_f4acbf: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w15_approvalsh_f4acbf {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w15_approvalsh_f4acbf: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
