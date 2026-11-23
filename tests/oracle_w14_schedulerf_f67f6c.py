#!/usr/bin/env python3
"""Differential oracle for gate w14_schedulerf_f67f6c (cron/scheduler_failure_copy.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_schedulerf_f67f6c"
sys.path.insert(0, QUARRY)

_seed_mod = types.ModuleType("hermes_constants")
_seed_mod.display_hermes_home = lambda: "/default"
_seed_mod.__path__ = []
sys.modules["hermes_constants"] = _seed_mod
agent = types.ModuleType("agent")
agent.__path__ = [QUARRY + "/agent"]
sys.modules["agent"] = agent
tfc = types.ModuleType("agent.turn_failure_copy")
tfc.__path__ = []
sys.modules["agent.turn_failure_copy"] = tfc
agent.turn_failure_copy = tfc

SRC = QUARRY + "/cron/scheduler_failure_copy.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def compute(r):
    live.display_hermes_home = lambda: r.get("home", "/home/u/.hermes")
    tfc.failure_cause_gloss = lambda reason, subject, possessive: r.get("cause")
    tfc.relogin_command_hint = lambda provider: r.get("relogin", "")
    f = r.get("func")
    name, jid = r.get("job_name", ""), r.get("job_id", "")
    if f == "output_dir":
        return live.cron_output_dir_display(jid)
    if f == "generic":
        return live.generic_failure_notice(name, jid, r.get("cleaned_error", ""))
    if f == "script_timeout":
        return live.script_timeout_notice(name, jid)
    if f == "inactivity":
        return live.inactivity_notice(name, jid)
    if f == "blocked_config":
        return live.blocked_config_notice(name, r.get("reason", ""))
    if f == "provider_notice":
        return live.provider_failure_notice(
            name, jid, r.get("reason", ""),
            backup_provider_phrase=r.get("backup_provider_phrase", ""),
            provider=r.get("provider"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
