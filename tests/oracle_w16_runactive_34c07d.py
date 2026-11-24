#!/usr/bin/env python3
"""Differential oracle for gate w16_runactive_34c07d (gateway/run_active_work_verb.py)."""
import importlib.util, json, logging, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name, path=None):
    m = types.ModuleType(name)
    m.__path__ = [os.path.join(Q, path or name)]
    sys.modules[name] = m
    return m

def _session_key_namespace(profile):
    if not profile or profile == "default": return "agent:main"
    return "agent:main~" if profile == "main" else f"agent:{profile}"

_mod("gateway")
sess = types.ModuleType("gateway.session")
sess._session_key_namespace = _session_key_namespace
sys.modules["gateway.session"] = sess
sw = types.ModuleType("gateway.session_runtime_workers")
sw.managed_worker_units = lambda authority: list(getattr(authority, "_managed_units", []) or [])
sw.side_agents = lambda authority: [None] * int(getattr(authority, "_side_count", 0) or 0)
sys.modules["gateway.session_runtime_workers"] = sw

NAME = "w16_runactive_34c07d_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "run_active_work_verb.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)
NS = types.SimpleNamespace

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
        if func == "profile_units":
            profile, home = r.get("profile"), r.get("home")
            pid = r.get("pid", 0)
            live.os = NS(getpid=lambda: pid)
            auth_r = r.get("authority")
            if home is None or not isinstance(auth_r, dict):
                authority = None
            else:
                authority = NS(_cron_cancellations={i: None for i in range(int(auth_r.get("cron_count", 0)))},
                               _managed_units=auth_r.get("managed_units") or [],
                               _side_count=auth_r.get("side_count", 0))
            runner = NS(
                _served_profile_homes=({profile: home} if home is not None else {}),
                session_authorities=NS(for_home=lambda h: (authority if h is not None else None)),
            )
            py = live._profile_units(runner, profile, r.get("units") or [])
        else: continue
        py = canon(py)
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_runactive_34c07d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
