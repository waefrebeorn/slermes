#!/usr/bin/env python3
"""Oracle for agent/context_compressor_prompt_rules.py (gate ctxprompt).

The three literal rule strings are compared to the live module constants.
_today_for_prompt is driven with a fake hermes_time.now returning the fixture
date (or raising for the clock-failure path); the C seam receives the same
facts and the resulting date string is compared.
"""
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

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)

spec = importlib.util.spec_from_file_location(
    "agent.context_compressor_prompt_rules",
    QUARRY + "/agent/context_compressor_prompt_rules.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.context_compressor_prompt_rules"] = live
spec.loader.exec_module(live)

_now_date = {"v": "1970-01-01"}


class _DT:
    def strftime(self, fmt):
        return _now_date["v"]


ht = types.ModuleType("hermes_time")


def _now():
    if not _clock_ok["v"]:
        raise RuntimeError("clock failure")
    return _DT()


_clock_ok = {"v": True}
ht.now = _now
sys.modules["hermes_time"] = ht

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
    if func == "heading":
        py = live.UNVERIFIED_SECTION_HEADING
    elif func == "rule":
        py = live.EPISTEMIC_STANCE_RULE
    elif func == "section":
        py = live.UNVERIFIED_HYPOTHESES_SECTION
    elif func == "today":
        _clock_ok["v"] = bool(r.get("clock_ok", False))
        _now_date["v"] = r.get("date", "")
        py = live._today_for_prompt()
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE ctxprompt: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
