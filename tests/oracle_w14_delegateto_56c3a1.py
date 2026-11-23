#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_routes.py (gate w14_delegateto_56c3a1)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_delegateto_56c3a1"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# parent-package bootstrap + stubs
_ag = _mod("agent")
_ag.__path__ = []
_mod("agent.delegation_provider",
     RESERVED_TASK_PARAMETERS=frozenset({"goal", "context", "index", "output_schema", "images", "group", "role",
        "toolsets", "acp_command", "acp_args", "provider", "model", "base_url", "api_key", "api_mode"}),
     ROUTE_KEYS=frozenset({"model", "provider", "base_url", "api_key", "api_mode", "request_overrides",
        "reasoning_effort", "service_tier", "fallback_providers", "profile", "label"}),
     DelegationRouteError=type("DelegationRouteError", (ValueError,), {}),
     DelegationProvider=type("DelegationProvider", (), {}))
_to = _mod("tools")
_to.__path__ = []
_mod("tools.delegate_tool_config", _resolve_delegation_credentials=lambda cfg, agent: {})

SRC = QUARRY + "/tools/delegate_tool_routes.py"
_spec = importlib.util.spec_from_file_location("delegate_tool_routes_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegate_tool_routes_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "param_error":
        return live._param_error(int(rec.get("index", 0)), rec.get("name", ""),
                                 rec.get("value"), rec.get("schema") or {})
    if func == "clean_route":
        return live._clean_route(rec.get("provider_name", ""), rec.get("route"))
    if func == "with_inherited_fallback":
        return live._with_inherited_fallback(rec.get("route") or {}, rec.get("config") or {})
    if func == "report":
        r = live.TaskRoute(creds={}, routing_cfg={}, backend=rec.get("backend", "default"),
                           label=rec.get("label"))
        return r.report()
    if func == "refused_entry":
        r = live.TaskRoute(creds={}, routing_cfg={}, backend=rec.get("backend", "default"),
                           label=rec.get("label"), error=rec.get("error"))
        return r.refused_entry(int(rec.get("index", 0)))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("param_error", "clean_route", "with_inherited_fallback", "report", "refused_entry"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
