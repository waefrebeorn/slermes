#!/usr/bin/env python3
"""Differential oracle for gate w16_runplugin_bf0c8c (gateway/run_plugin_injection.py)."""
import importlib.util, json, logging, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _pkg(name):
    m = types.ModuleType(name); m.__path__ = [os.path.join(Q, name)]; sys.modules[name] = m; return m
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m

g = _pkg("gateway")
_mod("gateway.config", Platform=types.SimpleNamespace(LOCAL="local"))
_mod("gateway.delivery", prime_revived_egress=lambda *a, **k: None)
_pkg("gateway.platforms")
_mod("gateway.platforms.event", MessageEvent=type("MessageEvent", (), {}), MessageType=type("MessageType", (), {}))
_mod("gateway.session", SessionSource=type("SessionSource", (), {}))

NAME = "w16_runplugin_bf0c8c_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "run_plugin_injection.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)

def canon(v):
    if isinstance(v, (list, tuple)): return [canon(x) for x in v]
    if isinstance(v, dict): return {k: canon(x) for k, x in v.items()}
    return v

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "plugin_admission":
            py = live._plugin_admission(r.get("row") or {})
        else: continue
        py = canon(py)
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_runplugin_bf0c8c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
