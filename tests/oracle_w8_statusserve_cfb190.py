#!/usr/bin/env python3
"""Differential oracle for gateway/status_served.py (gate w8_statusserve_cfb190).

gateway.status and gateway.config are registered as stub modules (the live
module resolves both lazily at call time, so its monkeypatch seams stay live).
"""
import importlib.util, json, sys, types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

gw = types.ModuleType("gateway")
gw.__path__ = [Q + "/gateway"]
sys.modules["gateway"] = gw

status = types.ModuleType("gateway.status")
_SEAMS = {}
status.get_runtime_status_running_pid = lambda runtime, *, expected_home: _SEAMS.get("own_running_pid")
status.multiplexer_liveness_for_profile = lambda home: (
    tuple(_SEAMS["served"]) if _SEAMS.get("served") else None)
status.profile_name_for_home = lambda home: _SEAMS.get("profile_name")
status.profile_platforms_from_multiplexer = lambda record, name: _SEAMS.get("platforms")
sys.modules["gateway.status"] = status
gw.status = status

config = types.ModuleType("gateway.config")
config.SHARED_LISTENER_MIRROR_PLATFORMS = frozenset({"api_server", "webhook"})
config.SERVED_PROFILE_MIRROR_PLATFORMS = config.SHARED_LISTENER_MIRROR_PLATFORMS | frozenset({"relay"})
config.SHARED_LISTENER_MIRROR_PATHS = {"api_server": "/v1", "webhook": "/webhooks/<route>"}
sys.modules["gateway.config"] = config
gw.config = config

LIVE_NAME = "w8_statusserve_cfb190_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/status_served.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def ns(d):
    if not isinstance(d, dict):
        return d
    return types.SimpleNamespace(**{k: (ns(v) if isinstance(v, dict) else v) for k, v in d.items()})


def compute(r):
    global _SEAMS
    f = r.get("func")
    if f == "local_liveness_pid":
        return live.local_liveness_pid(ns(r.get("liveness") or {}))
    if f == "served_profile_runtime":
        _SEAMS = dict(r.get("seams") or {})
        return live.served_profile_runtime(
            Path(r.get("home") or "/h"), r.get("own_runtime"),
            gateway_pid=r.get("gateway_pid"))
    if f == "served_profile_mirror_platforms":
        return live.served_profile_mirror_platforms(r.get("runtime"), r.get("profile", ""))
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_statusserve_cfb190: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
