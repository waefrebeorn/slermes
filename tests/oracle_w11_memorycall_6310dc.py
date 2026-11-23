#!/usr/bin/env python3
"""Differential oracle for agent/memory_caller_scope.py (gate w11_memorycall_6310dc).

Env/filesystem-driven collaborators are injected from each fixture record into
explicit seams on the live module (PORT-KIT: no process env mutation).
"""
import importlib.util, json, sys, types
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_memorycall_6310dc"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/memory_caller_scope.py"
_spec = importlib.util.spec_from_file_location("memory_caller_scope_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["memory_caller_scope_live"] = live
_spec.loader.exec_module(live)

class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"): raise AttributeError(name)
        return lambda *a, **k: None
def _seed(name, **attrs):
    m = _Stub(name); m.__path__ = []
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        p = ".".join(parts[:i])
        if p in sys.modules and not hasattr(sys.modules[p], parts[i]):
            setattr(sys.modules[p], parts[i], m)
    return m
_ss = _seed("agent.secret_scope", is_multiplex_active=lambda: False)
_hc = _seed("hermes_constants", get_hermes_home=lambda: "")

def live_out(rec):
    func = rec.get("func")
    if func == "refused_tool_message":
        return live.refused_tool_message(rec.get("tool_name") or "")
    if func == "bound_scope":
        live._home_key = lambda h: rec.get("home_key")
        return live.bound_scope(rec.get("home"), rec.get("identity"))
    if func == "caller_scope":
        keys = rec.get("keys") or {}
        caller_home = rec.get("caller_home")          # None when absent
        process_home = rec.get("process_home") or ""
        multiplexed = bool(rec.get("multiplexed"))
        live._explicit_caller_home = lambda: caller_home
        live._home_key = lambda h: (keys.get(str(h)) if h else None)
        _ss.is_multiplex_active = lambda: multiplexed
        _hc.get_hermes_home = lambda: process_home
        return live.caller_scope(rec.get("bound"), "label")
    return None

def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: rec = json.loads(line)
        except ValueError: continue
        if rec.get("func") not in ("refused_tool_message","bound_scope","caller_scope"): continue
        cases += 1
        try: py = live_out(rec)
        except Exception as exc: py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)"); return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
