#!/usr/bin/env python3
"""Differential oracle for gate w14_schedulerg_cccc6c (cron/scheduler_gateway_gate.py)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_schedulerg_cccc6c"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace

cli = types.ModuleType("hermes_cli")
cli.__path__ = []
sys.modules["hermes_cli"] = cli
runtime = types.ModuleType("hermes_cli.gateway_runtime")
runtime.__path__ = []
sys.modules["hermes_cli.gateway_runtime"] = runtime
cli.gateway_runtime = runtime
constants = types.ModuleType("hermes_constants")
constants.__path__ = []
constants.get_hermes_home = lambda: NS(resolve=lambda: "/resolved/home")
sys.modules["hermes_constants"] = constants

SRC = QUARRY + "/cron/scheduler_gateway_gate.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def _set_env(r):
    key = "HERMES_TUI_GATEWAY_URL"
    if "env_value" in r:
        os.environ[key] = r["env_value"]          # Python keeps the empty string
    else:
        os.environ.pop(key, None)


def _stub_discovery(r):
    runtime.discover_gateway_endpoint = (
        lambda home, timeout=None: NS(state=r.get("state", "absent"),
                                      reason_code=r.get("reason_code")))


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    _set_env(r)
    _stub_discovery(r)
    f = r.get("func")
    if f == "gateway_unavailable":
        return live._gateway_unavailable("/resolved/home")
    if f == "refuse_jobs":
        captured = {}
        def note(ids, detail):
            captured["ids"] = list(ids)
            captured["detail"] = detail
        live._note_fire_refused = note
        jobs = live.refuse_agent_jobs_without_gateway(r.get("due_jobs") or [])
        return {"jobs": jobs,
                "refused_ids": captured.get("ids"),
                "detail": captured.get("detail")}
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
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
