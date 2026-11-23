#!/usr/bin/env python3
"""Differential oracle for tools/approval_gateway_wait.py (gate w15_approvalga_206913)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

NAME = "tools.approval_gateway_wait_live"
spec = importlib.util.spec_from_file_location(NAME, QUARRY + "/tools/approval_gateway_wait.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "identity_digest":
        return live._identity_digest(r.get("text"))
    if f == "same_answerers":
        return live._same_answerers(r.get("leader") or {}, r.get("follower") or {})
    if f == "hook_choice":
        return live._hook_choice(bool(r.get("resolved")), r.get("choice"), r.get("extra") or {})
    if f == "cancel_cause":
        old = live.get_interrupt_reason
        live.get_interrupt_reason = lambda: r.get("interrupt_reason")
        e = r.get("entry") or {}
        entry = types.SimpleNamespace(result=e.get("result"), cancelled=e.get("cancelled"))
        try:
            return live._cancel_cause(r.get("state", ""), entry)
        finally:
            live.get_interrupt_reason = old
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
        print(f"ORACLE w15_approvalga_206913: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w15_approvalga_206913 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w15_approvalga_206913: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
