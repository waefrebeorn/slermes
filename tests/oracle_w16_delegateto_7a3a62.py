#!/usr/bin/env python3
"""Differential oracle for gate w16_delegateto_7a3a62 (tools/delegate_tool_toolsets.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


DATA = {"resolved": {}, "alias_target": None}


def _resolve(name, include_registry=False):
    return list(DATA["resolved"].get(name, []))


ts = _pkg("toolsets")
ts.TOOLSETS = {}
ts.resolve_toolset = _resolve
tools = _pkg("tools")
_mod("tools.delegate_tool_config", _get_inherit_mcp_toolsets=lambda: False)


class _Reg:
    def get_toolset_alias_target(self, name):
        return DATA.get("alias_target")


_mod("tools.registry", registry=_Reg())

NAME = "w16dtt_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/delegate_tool_toolsets.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w16_delegateto_7a3a62"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    catalog = inp.get("catalog") or {}
    DATA["resolved"] = catalog
    DATA["alias_target"] = inp.get("alias_target")
    live.TOOLSETS = {k: {"tools": v} for k, v in catalog.items()}
    live.resolve_toolset = _resolve
    if func == "is_mcp":
        return live._is_mcp_toolset_name(inp.get("name", ""))
    if func == "blocked_for_role":
        return sorted(live._blocked_toolsets_for_role(inp.get("role", "")))
    if func == "strip_blocked":
        return live._strip_blocked_tools(inp.get("toolsets") or [])
    if func == "expand_parent":
        return sorted(live._expand_parent_toolsets(inp.get("parent") or []))
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
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
