#!/usr/bin/env python3
"""Differential oracle for gate w13_pluginmess_f27478 (hermes_cli/plugin_messages.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_pluginmess_f27478"
sys.path.insert(0, QUARRY)


def _seed(name):
    mod = types.ModuleType(name)
    mod.__path__ = []
    sys.modules[name] = mod
    return mod


# parent-package bootstrap (module's gateway import is lazy and not exercised)
_seed("hermes_cli")
_seed("gateway")

SRC = QUARRY + "/hermes_cli/plugin_messages.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def handle(st):
    st = st or {}
    result = st.get("result") or {}
    target = None if st.get("target_null") else object()
    h = live.PluginMessage("plugin-x", target, result)
    if "message_id" in st:
        h.message_id = st["message_id"]
    return h


def as_state(h):
    return {"target_null": h._target is None, "result": h.result, "message_id": h.message_id}


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "err":
        return live._err(r.get("code"), r.get("detail") or "")
    if func == "props":
        h = handle(r.get("state"))
        return {"ok": h.ok, "error": h.error}
    if func == "repr":
        return repr(handle(r.get("state")))
    if func == "refused":
        return handle(r.get("state"))._refused()
    if func == "settle":
        h = handle(r.get("state"))
        h._settle(r.get("result"))
        return as_state(h)
    if func == "adopt":
        h = handle(r.get("state"))
        h._adopt(handle(r.get("other")))
        return as_state(h)
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
