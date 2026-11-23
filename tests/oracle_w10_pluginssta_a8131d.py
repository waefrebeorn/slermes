#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_state.py (gate w10_pluginssta_a8131d).

Pure helpers only; the injected `portable_ns` seam replaces the external
_portable_data_namespace call (as the live module's own tests do).
"""
import importlib.util, json, sys, types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
PKG = "w10_pluginssta_live_pkg"
_pkg = types.ModuleType(PKG); _pkg.__path__ = [Q + "/hermes_cli"]; sys.modules[PKG] = _pkg
spec = importlib.util.spec_from_file_location(PKG + ".plugins_state", Q + "/hermes_cli/plugins_state.py")
live = importlib.util.module_from_spec(spec); sys.modules[spec.name] = live; spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "segments":
        try:
            return {"ok": True, "segments": list(live._plugin_relative_segments(r.get("key")))}
        except ValueError:
            return {"ok": False, "error": "ValueError"}
    if func == "nested_value":
        return live._nested_plugin_value(r.get("root"), tuple(r.get("segments", [])), r.get("default"))
    if func == "nested_mapping":
        return live._nested_plugin_mapping(tuple(r.get("segments", [])), r.get("value"))
    if func == "state_key_valid":
        try:
            live.PluginState._validate_key(r.get("key"))
            return True
        except ValueError:
            return False
    if func == "data_ns":
        original = live._portable_data_namespace
        live._portable_data_namespace = lambda c: r.get("portable_ns", "")
        try:
            return live._plugin_data_namespace(r.get("plugin_id", ""), r.get("data_namespace", ""))
        finally:
            live._portable_data_namespace = original
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)
print("ORACLE w10_pluginssta_a8131d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
