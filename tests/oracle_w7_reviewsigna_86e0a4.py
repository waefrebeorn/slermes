#!/usr/bin/env python3
"""Differential oracle for agent/review_signals.py (gate w7_reviewsigna_86e0a4)."""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/review_signals.py"
spec = importlib.util.spec_from_file_location("w7_reviewsigna_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _agent(r):
    return SimpleNamespace(
        _skill_nudge_interval=int(r.get("skill_interval", "0")),
        _memory_nudge_interval=int(r.get("memory_interval", "0")),
        _iters_since_skill=int(r.get("iters", "0")),
        _turns_since_memory=int(r.get("turns", "0")),
        valid_tool_names=set(r.get("valid_tool_names", [])),
        skip_background_review=bool(r.get("skip", False)),
        _delegate_depth=int(r.get("delegate_depth", "0")),
    )


def compute(r):
    func = r.get("func")
    if func == "skill_review_due":
        return live.skill_review_due(_agent(r))
    if func == "background_review_enabled":
        configured = bool(r.get("configured", False))
        fake = types.ModuleType("agent.background_review")
        fake.load_background_review_settings = lambda: (configured, {})
        sys.modules["agent.background_review"] = fake
        return live.background_review_enabled(_agent(r))
    if func == "build_review_signals":
        prev = live.background_review_enabled
        live.background_review_enabled = lambda a: bool(r.get("background_enabled", False))
        try:
            return live.build_review_signals(
                _agent(r),
                memory_review_due=bool(r.get("memory_review_due", False)),
                skill_review_due=bool(r.get("skill_review_due", False)),
            )
        finally:
            live.background_review_enabled = prev
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_reviewsigna_86e0a4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
