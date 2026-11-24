#!/usr/bin/env python3
"""Differential oracle for gateway/run_restored_delivery.py (w17_w17_runrestore_0acd81_0acd81)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

gateway = types.ModuleType("gateway")
gateway.__path__ = []
sys.modules["gateway"] = gateway
class Platform:
    LOCAL = "local"; TELEGRAM = "telegram"; RELAY = "relay"; DISCORD = "discord"
    def __init__(self, value): self.value = value
gconfig = types.ModuleType("gateway.config")
gconfig.Platform = Platform
sys.modules["gateway.config"] = gconfig
gateway.config = gconfig

_state = {}
def _check_and_record(max_restarts, window, max_gap_seconds=None):
    if _state.get("guard_raises"):
        raise RuntimeError("guard unavailable")
    return _state.get("guard_trip", False)
mod_guard = types.ModuleType("gateway.restart_loop_guard")
mod_guard.check_and_record = _check_and_record
sys.modules["gateway.restart_loop_guard"] = mod_guard
gateway.restart_loop_guard = mod_guard

SRC = Q + "/gateway/run_restored_delivery.py"
spec = importlib.util.spec_from_file_location("w17_runrestore_0acd81_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_runrestore_0acd81_live"] = live
spec.loader.exec_module(live)


class Lock:
    def __enter__(self):
        if _state.get("lock_raises"):
            raise RuntimeError("lock")
        return self
    def __exit__(self, *a):
        return False


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
    if func != "candidates":
        continue
    try:
        _state.clear()
        _state.update(r)
        platform = r.get("platform", None)
        fronts_platforms = r.get("fronts_platforms", [])
        relay = (types.SimpleNamespace(fronts_platform=lambda p: p in fronts_platforms)
                 if platform == Platform.RELAY and r.get("relay_fronts", False) else None)
        entries = {}
        for e in r.get("entries", []):
            o = e.get("origin")
            origin = types.SimpleNamespace(platform=o) if isinstance(o, str) else None
            entries[e.get("session_key", "")] = types.SimpleNamespace(
                session_key=e.get("session_key", ""),
                resume_pending=bool(e.get("resume_pending", False)),
                suspended=bool(e.get("suspended", False)),
                origin=origin,
                resume_reason=e.get("resume_reason"))
        def _ensure():
            if r.get("ensure_raises"):
                raise RuntimeError("ensure")
        store = types.SimpleNamespace(
            _lock=Lock(), _ensure_loaded_locked=_ensure, _entries=entries)
        runner = types.SimpleNamespace(
            _primary_adapters=lambda: {Platform.RELAY: relay},
            session_store=store,
            _AUTO_RESUME_REASONS=frozenset(r.get("auto_reasons", [])),
            _restart_loop_guard_config=lambda: (r.get("max_restarts", 1), r.get("window", 60.0),
                                                 r.get("max_gap", 200.0)))
        result = live.resume_pending_candidates(runner, platform=platform)
        py = None if result is None else [e.session_key for e in result]
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_runrestore_0acd81: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
