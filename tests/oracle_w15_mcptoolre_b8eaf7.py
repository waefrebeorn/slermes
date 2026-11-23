#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_registration.py (gate w15_mcptoolre_b8eaf7)."""
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
G = "w15_mcptoolre_b8eaf7"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _noop(*args, **kwargs):
    return None


# parent-package bootstrap + every heavy import of the module, stubbed
_to = _mod("tools")
_to.__path__ = []
_core = types.SimpleNamespace(_TRUST_FULL="full", _TRUST_UNTRUSTED="untrusted")
_mod("tools.mcp_tool_common",
     _parse_boolish=lambda v, default=True: default,
     _core=_core, _resolve_tool_timeout=_noop, mcp_field=_noop,
     mcp_server_enabled=lambda c: True)
_mod("tools.mcp_tool_config")
_mod("tools.mcp_tool_events",
     events_enabled=lambda c: False,
     _make_list_event_types_handler=_noop, _make_subscribe_event_handler=_noop,
     _make_unsubscribe_event_handler=_noop)
_handler_names = ("_make_check_fn", "_make_get_prompt_handler", "_make_list_prompts_handler",
                  "_make_list_resources_handler", "_make_read_resource_handler", "_make_tool_handler")
_mod("tools.mcp_tool_handlers", **{n: _noop for n in _handler_names})
_mod("tools.mcp_tool_schema",
     _scan_mcp_description=_noop, _convert_mcp_schema=lambda n, t: {"name": n},
     _UTILITY_CAPABILITY_ATTRS={}, _build_utility_schemas=lambda n: [],
     _normalize_name_filter=lambda *a, **k: set(), matches_name_filter=lambda *a: False)
_mod("tools.mcp_tool_scope",
     _key_name=_noop, _key_scope=_noop, _resolve_server_key=_noop, _server_key=_noop)

SRC = QUARRY + "/tools/mcp_tool_registration.py"
_spec = importlib.util.spec_from_file_location("mcp_tool_registration_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["mcp_tool_registration_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "normalize_trust":
        return live._normalize_server_trust(rec.get("value"))
    if func == "auth_type":
        return live._auth_type(rec.get("config") or {})
    return None


def main():
    cases = mism = 0
    allowed = ("normalize_trust", "auth_type")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
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
