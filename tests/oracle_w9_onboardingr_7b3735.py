#!/usr/bin/env python3
"""Differential oracle for agent/onboarding_record.py (gate w9_onboardingr_7b3735).

Drives the LIVE module's state-free surface with injected seams: load()'s
store collaborator and _announce()'s ended-listener are seeded by the fixture,
so wall-clock/DB/config side effects never enter the comparison.  Both sides
consume the same inputs; C must reproduce the module's own guard/type-filter
and ended-payload composition.
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


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


# --- parent-package bootstrap so `agent.*` / `session_rules.*` imports resolve ---
agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)

# Heavy import-time collaborators of the live module -> recording stubs.
aai = types.ModuleType("agent.agent_init_session")
aai.writer_interface = lambda *a, **k: None
aai.agent_interface = lambda *a, **k: None
sys.modules["agent.agent_init_session"] = aai

pa = types.ModuleType("persistence_assembly")
class Persistence:  # FakeDB is never an instance -> load() uses db.get_...
    pass
pa.Persistence = Persistence
sys.modules["persistence_assembly"] = pa

ot = types.ModuleType("agent.onboarding_text")
ot.reminder = lambda *a, **k: ""
sys.modules["agent.onboarding_text"] = ot

sr = types.ModuleType("session_rules")
sr.__path__ = [QUARRY + "/session_rules"]
sys.modules.setdefault("session_rules", sr)
of = types.ModuleType("session_rules.onboarding_flow")
of.Applied = of.End = of.Event = of.Facts = of.Step = object
of.applies_to = lambda *a, **k: False
of.apply = lambda *a, **k: types.SimpleNamespace(record={}, reminder="")
of.is_active = lambda d: bool(d and d.get("run_id"))
of.new_record = lambda *a, **k: {}
sys.modules["session_rules.onboarding_flow"] = of

live = _load("agent.onboarding_record", QUARRY + "/agent/onboarding_record.py")


def compute(r):
    func = r.get("func")
    if func == "load":
        sid = r.get("session_id", "")
        if r.get("db_none"):
            return live.load(None, sid)
        rec = r.get("record")

        class FakeDB:
            def get_session_model_config_value(self, s, key):
                return rec
        return live.load(FakeDB(), sid)
    if func == "announce":
        captured = {}
        live._ended_listeners.clear()
        live.on_ended(lambda d: captured.update(d))
        live._announce(r.get("run_id"), r.get("session_id", ""), r.get("outcome", ""))
        return captured
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_onboardingr_7b3735: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
