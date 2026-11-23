#!/usr/bin/env python3
"""Differential oracle for tui_gateway/mcp_rpc_helpers.py (gate w15_mcprpchel_bbb1c7).

Loads the LIVE module with stubbed hermes_cli / tools submodules (plugin
discovery, OAuth probe, catalog telemetry are the only environment reads).
The resolved facts come from the fixture; the three projections are compared
with the C harness `out`.
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


def _seed(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], m)
    return m


_seed("hermes_cli")
_seed("tools")
_seen = {"oauth": False, "enabled": False, "portable": {}, "owners": {}, "install": None}

_seed("hermes_cli.mcp_config", _oauth_tokens_present=lambda n: _seen["oauth"])
_seed("tools.mcp_tool_common", mcp_server_enabled=lambda cfg: _seen["enabled"])
_seed("tools.mcp_tool_config", _filter_suspicious_mcp_servers=lambda x: x)
_seed("hermes_cli.plugins", discover_plugins=lambda: None,
      get_plugin_manager=lambda: types.SimpleNamespace(
          get_portable_mcp_servers=lambda: _seen["portable"],
          get_portable_mcp_server_plugins=lambda: _seen["owners"]))


def _install(*args, **kwargs):
    failure_class = kwargs.get("failure_class")
    _seen["install"] = (args[0], args[1], args[2], failure_class)


_seed("hermes_cli.mcp_catalog", record_mcp_install=_install)

_spec = importlib.util.spec_from_file_location(
    "mcp_rpc_helpers_live", QUARRY + "/tui_gateway/mcp_rpc_helpers.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["mcp_rpc_helpers_live"] = live
_spec.loader.exec_module(live)

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
        if func == "summary":
            _seen["oauth"] = bool(r.get("oauth"))
            _seen["enabled"] = bool(r.get("enabled"))
            plugin = r.get("plugin")
            py = live.summarize_server(r.get("name", ""), r.get("cfg", {}), plugin)
        elif func == "sources":
            _seen["portable"] = r.get("portable") or {}
            _seen["owners"] = r.get("owners") or {}
            servers, plugins = live.server_configs_with_sources(r.get("config_servers") or {})
            py = {"servers": servers, "plugins": plugins}
        elif func == "record":
            entry = (types.SimpleNamespace(name=r.get("entry_name", ""))
                     if r.get("entry_present") else None)
            _seen["install"] = None
            live.record_mcp_add(entry, r.get("server_config") or {}, bool(r.get("saved")))
            source, name, status, failure_class = _seen["install"]
            py = {"source": source, "name": name, "status": status,
                  "failure_class": failure_class}
        else:
            continue
    except Exception as e:
        print("PY-ERROR %r: %s" % (r, e), file=sys.stderr)
        mism += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w15_mcprpchel_bbb1c7: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
