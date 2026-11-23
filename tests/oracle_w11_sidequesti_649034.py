#!/usr/bin/env python3
"""Differential oracle for agent/side_question_lease.py (gate w11_sidequesti_649034).

Imports the LIVE module with a stub side_question_request; the two pure
decision helpers are exercised with an injected lease (expiry is forced to a
fixed past/future value) and an injected attribute-bearing agent.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name); m.__path__ = []; sys.modules[name] = m
    return m

agent_pkg = _pkg("agent")
sqr = types.ModuleType("agent.side_question_request")
sqr.SideQuestionCapture = type("SideQuestionCapture", (), {})
sqr.ask_text_only = lambda *a, **k: None
sqr.runtime_key = lambda a: None
sys.modules["agent.side_question_request"] = sqr
agent_pkg.side_question_request = sqr

SRC = QUARRY + "/agent/side_question_lease.py"
spec = importlib.util.spec_from_file_location("w11_sidequesti_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_sidequesti_live"] = live
spec.loader.exec_module(live)


def make_lease(lv):
    if lv is None:
        return None
    parent = None if lv.get("parent_gone") else SimpleNamespace(
        _current_turn_id=lv.get("parent_turn_id"))
    capture = SimpleNamespace(parent_ref=lambda: parent, turn_id=lv.get("turn_id"))
    return SimpleNamespace(expires_at=(-1.0 if lv.get("expired") else 1e18), capture=capture)


def compute(r):
    func = r.get("func")
    if func == "unattended":
        amap = {"persist_disabled": "_persist_disabled", "platform": "platform",
                "delegate_depth": "_delegate_depth"}
        attrs = {py: r[k] for k, py in amap.items() if k in r}
        return live.is_unattended_turn(SimpleNamespace(**attrs))
    if func == "redeemable":
        mpc = r.get("max_prompt_chars", 4096)
        return live._redeemable(make_lease(r.get("lease")), r.get("prompt"), mpc)
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
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_sidequesti_649034: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_sidequesti_649034: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
