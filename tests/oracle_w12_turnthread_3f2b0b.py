#!/usr/bin/env python3
"""Differential oracle for gateway/turn_thread_reset.py (w12_turnthread_3f2b0b).

Installs fake sys.modules for tools.interrupt / tools.terminal_tool_sudo /
each configured threading.local, patches threading.get_ident to the fixture
tid, then drives the LIVE reset_reused_thread_state() and projects the resulting
state back into JSON for comparison with the C port.
"""
import importlib.util
import json
import sys
import threading
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/gateway/turn_thread_reset.py"
spec = importlib.util.spec_from_file_location("turn_thread_reset_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["turn_thread_reset_live"] = live
spec.loader.exec_module(live)

_ADDED = []


def _module(name):
    if name in sys.modules:
        return sys.modules[name]
    if "." in name:
        parent, _ = name.rsplit(".", 1)
        _module(parent)
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    _ADDED.append(name)
    return m


def _cleanup():
    for name in reversed(_ADDED):
        sys.modules.pop(name, None)
    _ADDED.clear()


def reveal(v):
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, dict):
        return {str(k): reveal(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [reveal(x) for x in v]
    return str(v)


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
        if func == "locals":
            py = [f"{m}.{a}" for m, a in live._PER_TURN_THREAD_LOCALS]
        elif func == "reset":
            _cleanup()
            tid = int(r.get("tid", "0"))
            intr_state = None
            if "interrupt" in r and isinstance(r["interrupt"], dict):
                intr_state = dict(r["interrupt"])
                intr_state["last_tid"] = None
                intr_mod = _module("tools.interrupt")

                def set_interrupt(flag, who, _s=intr_state):
                    _s["set"] = flag
                    _s["reason"] = None
                    _s["pending_yield"] = False
                    _s["last_tid"] = who

                intr_mod.set_interrupt = set_interrupt

            sudo_cache = None
            sudo_mod = None
            if "sudo_cache" in r and isinstance(r["sudo_cache"], dict):
                sudo_cache = dict(r["sudo_cache"])
                sudo_mod = _module("tools.terminal_tool_sudo")
                sudo_mod._sudo_password_cache_lock = threading.Lock()
                sudo_mod._sudo_password_cache = dict(sudo_cache)

            holders = {}
            for key, state in (r.get("locals") or {}).items():
                if not isinstance(key, str):
                    continue
                mod_name, attr = key.rsplit(".", 1)
                mod = _module(mod_name)
                holder = types.SimpleNamespace(**(state if isinstance(state, dict) else {}))
                setattr(mod, attr, holder)
                holders[key] = holder

            orig_ident = threading.get_ident
            threading.get_ident = lambda: tid
            try:
                live.reset_reused_thread_state()
            finally:
                threading.get_ident = orig_ident

            expected = {
                "interrupt": reveal(intr_state) if intr_state is not None else None,
                "sudo_cache": reveal(dict(sudo_mod._sudo_password_cache))
                if sudo_mod is not None else None,
                "locals": {k: reveal(dict(h.__dict__)) for k, h in holders.items()},
            }
            py = expected
            _cleanup()
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w12_turnthread_3f2b0b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
