#!/usr/bin/env python3
"""Differential oracle for w7_onboardingt_01579d -> agent/onboarding_text.py.

Live module loaded via spec_from_file_location; parent packages session_rules/
tools and their imported submodules are registered as stubs before exec_module.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("session_rules")
_pkg("tools")
_mod("session_rules.onboarding_first_tasks",
     FIXED_ID="build", MAX_LABEL=60, MAX_PICKS=3,
     OWN_TASK=types.SimpleNamespace(), TASK_BY_ID={},
     eligible_tasks=lambda *a, **k: [], task_skills=lambda *a, **k: [])
_mod("session_rules.onboarding_flow",
     Facts=types.SimpleNamespace, next_beat=lambda r: None)
_mod("tools.set_theme_tool", ACCENT_SWATCHES=("violet",))

SRC = Q + "/agent/onboarding_text.py"
spec = importlib.util.spec_from_file_location("onboardingt_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["onboardingt_live"] = live
spec.loader.exec_module(live)


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


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
    try:
        if func == "ui_language":
            py = live.ui_language(r.get("locale"))
        elif func == "greeting":
            py = live.greeting(r.get("locale"))
        elif func == "question":
            py = live.question(r.get("beat", ""), r.get("locale"))
        elif func == "names":
            py = live._names(r.get("values") or [])
        elif func == "facts_block":
            py = live.facts_block(types.SimpleNamespace(**(r.get("facts") or {})))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = canonical(r.get("out"))
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={canonical(py)!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_onboardingt_01579d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
