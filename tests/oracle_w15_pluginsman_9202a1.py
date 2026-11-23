#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_manifest.py (w15_pluginsman_9202a1)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc
_hc = sys.modules["hermes_cli"]
_u = types.ModuleType("utils")
_u.fast_safe_load = lambda *a, **k: {}
sys.modules["utils"] = _u
_pc = types.ModuleType("hermes_cli.plugin_capabilities")
_pc.parse_declared_capabilities = lambda *a, **k: []
sys.modules["hermes_cli.plugin_capabilities"] = _pc
_hc.plugin_capabilities = _pc

SRC = Q + "/hermes_cli/plugins_manifest.py"
spec = importlib.util.spec_from_file_location("pluginsman_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pluginsman_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "portable_slug":
        return live._portable_slug(r.get("key", ""))
    if f == "portable_mcp_server_name":
        return live.portable_mcp_server_name(r.get("key", ""), r.get("server", ""))
    if f == "display_author":
        return live._display_author(r.get("value"))
    if f == "config_key_path":
        return live._config_key_path(r.get("item"))
    if f == "manifest_config_keys":
        return live.manifest_config_keys(r.get("data") or {}, "p")
    if f == "detect_kind":
        return live._detect_kind_from_source(r.get("source", ""))
    if f == "version_satisfies":
        return bool(live.version_satisfies(r.get("spec", ""), r.get("current", "")))
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
    expected = compute(r)
    got = r.get("out")
    if expected != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={expected!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w15_pluginsman_9202a1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
