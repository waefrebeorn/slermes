#!/usr/bin/env python3
"""Differential oracle for gate w11_welcometri_be5319 (hermes_cli/welcome_trial.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hp = types.ModuleType("hermes_cli")
    _hp.__path__ = [os.path.join(Q, "hermes_cli")]
    sys.modules["hermes_cli"] = _hp
NAME = "w11_welcometri_be5319_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "welcome_trial.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _setup(r):
    local = bool(r.get("local_backend", False))
    guest = bool(r.get("guest_allowed", False))
    nous = bool(r.get("has_nous_identity", False))
    prov = bool(r.get("provider_configured", False))
    free = bool(r.get("free_inference_active", False))
    machine = bool(r.get("machine_existing", False)) and not bool(r.get("trial_reserved", False))
    aa = types.ModuleType("anon_auth")
    aa.on_local_desktop_backend = lambda: local
    aa.guest_allowed = lambda: guest
    aa.free_inference_active = lambda: free
    live.anon_auth = aa
    live._has_nous_identity = lambda: nous
    live.provider_configured = lambda: prov
    live._machine_is_used = lambda assume_fresh: (not assume_fresh) and machine
    return r.get("configured", None)


def compute(r):
    f = r.get("func")
    af = bool(r.get("assume_fresh", False))
    if f == "claim_blocker":
        configured = _setup(r)
        cfg = None if configured is None else bool(configured)
        return live.claim_blocker(assume_fresh=af, configured=cfg)
    if f == "inference_available":
        _setup(r)
        return live.inference_available(assume_fresh=af)
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w11_welcometri_be5319: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
