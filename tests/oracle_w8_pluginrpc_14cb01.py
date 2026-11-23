#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_rpc.py (w8_pluginrpc_14cb01 gate).

The real hermes_cli/plugin_events.py supplies PLUGIN_EVENT_PREFIX and the
validator used by register_rpc; the registry and callable() are injected.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/plugin_rpc.py"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hcpkg = _mod("hermes_cli")
hcpkg.__path__ = [QUARRY + "/hermes_cli"]
_mod("hermes_cli.plugins_ledger", PluginRegistration=object)
TABLE = {}
_mod("hermes_cli.plugins",
     _ensure_plugins_discovered=lambda: types.SimpleNamespace(_plugin_rpc_methods=TABLE))

spec = importlib.util.spec_from_file_location("pluginrpc_w8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pluginrpc_w8_live"] = live
spec.loader.exec_module(live)


class FakeManager:
    def __init__(self, table):
        self._plugin_rpc_methods = table


class FakeSelf:
    def __init__(self, pid, table):
        self.plugin_id = pid
        self._manager = FakeManager(table)

    def _register_entry(self, bucket, method, table, handler, log_fmt, *args):
        return {"bucket": bucket, "method": method}


def compute(r):
    f = r["func"]
    if f == "handler":
        global TABLE
        TABLE = r.get("table") or {}
        return live.plugin_rpc_handler(r.get("method"))
    if f == "register":
        handler = (lambda *a, **k: None) if r.get("callable") else None
        obj = FakeSelf(r.get("plugin_id", ""), {})
        try:
            return live.register_rpc(obj, r.get("name", ""), handler)
        except Exception as exc:
            return {"error": str(exc)}
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
        print(f"ORACLE pluginrpc: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH pluginrpc {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE pluginrpc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
