#!/usr/bin/env python3
"""Differential oracle for gate w16_runstartup_6b6bfb (gateway/run_startup_liveness.py)."""
import contextlib, importlib.util, json, logging, os, sys, types
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

_pkg("gateway")
_mod("gateway.run_shutdown", _log_suppressed=lambda *a, **k: contextlib.nullcontext())
_mod("gateway.run", _load_gateway_config=lambda: {})
_mod("hermes_state_loop_guard", install_from_config=lambda cfg: None)
sw = _mod("gateway.shutdown_watchdog",
          DEFAULT_LOOP_WATCHDOG_INTERVAL_S=30.0,
          DEFAULT_LOOP_WATCHDOG_TIMEOUT_S=10.0,
          DEFAULT_LOOP_WATCHDOG_MAX_STRIKES=3)

NAME = "w16_runstartup_6b6bfb_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "run_startup_liveness.py"))
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
        if func == "liveness_plan":
            rec = {"floor": 0, "wd": None}
            def arm_floor(loop):
                rec["floor"] += 1; return "floor-handle"
            def start_wd(loop, *, probe_interval, probe_timeout, max_strikes):
                rec["wd"] = {"probe_interval": probe_interval, "probe_timeout": probe_timeout,
                             "max_strikes": max_strikes}
                return "watchdog"
            sw._arm_loop_floor_timer = arm_floor
            sw.start_loop_liveness_watchdog = start_wd
            config = r.get("config")
            live_config = None if config is None else NS(**config)
            wd_in = r.get("watchdog")
            self = NS(config=live_config, _loop_floor_timer_handle=r.get("floor_handle"),
                      _loop_liveness_watchdog=(None if wd_in is None
                                               else NS(is_alive=lambda: bool(wd_in.get("alive")))))
            live.start_loop_liveness_guards(self, None)
            enabled = not (live_config is not None and not getattr(live_config, "loop_watchdog", True))
            wd = rec["wd"]
            py = {"enabled": enabled, "arm_floor": bool(rec["floor"]), "arm_watchdog": wd is not None,
                  "probe_interval": wd["probe_interval"] if wd else None,
                  "probe_timeout": wd["probe_timeout"] if wd else None,
                  "max_strikes": wd["max_strikes"] if wd else None}
        else: continue
        py = canon(py)
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_runstartup_6b6bfb: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
