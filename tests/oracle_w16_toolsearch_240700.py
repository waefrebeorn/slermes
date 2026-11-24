#!/usr/bin/env python3
"""Differential oracle for gate w16_toolsearch_240700 (tools/tool_search_validation.py)."""
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


tools = _pkg("tools")
ts = _mod("tools.tool_search", _core_tool_names=lambda: set(), out_of_scope_reason=lambda n: None)
_mod("tools.tool_search_catalog", BRIDGE_TOOL_NAMES=set(), _registry_entry=lambda n: None)


class _Reg:
    all_names = []

    def get_all_tool_names(self):
        return list(self.all_names)


reg = _Reg()
_mod("tools.registry", tool_error=lambda msg, **kw: msg, registry=reg)

NAME = "w16tsv_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/tool_search_validation.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w16_toolsearch_240700"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    live.BRIDGE_TOOL_NAMES = set(inp.get("bridge_names") or [])
    nm0 = inp.get("name", "")
    core = set([nm0]) if (inp.get("in_core") or inp.get("core_or_registered")) else set()
    registered = bool(inp.get("registered")) or bool(inp.get("core_or_registered"))
    live._registry_entry = (lambda n: object()) if registered else (lambda n: None)
    ts._core_tool_names = lambda: core
    ts.out_of_scope_reason = lambda n: inp.get("out_of_scope")
    reg.all_names = inp.get("all_tool_names") or []
    if func == "schema_for_local":
        return live._schema_for_local_validation(inp.get("node"))
    if func == "schema_has_external":
        return live._schema_has_external_ref(inp.get("node"))
    if func == "normalize_entries":
        entries, err = live.normalize_tool_call_entries(inp.get("args") or {})
        return {"entries": entries, "error": err}
    if func == "local_batch_error":
        return live.local_batch_error(inp.get("entries") or [])
    if func == "unavailable_in_session":
        return live.unavailable_in_session_reason(inp.get("name", ""))
    if func == "session_unavailable":
        return live.session_unavailable_error(inp.get("name", ""), inp.get("session_names"))
    if func == "not_deferrable":
        return live.not_deferrable_error(inp.get("name", ""), inp.get("session_names"))
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
