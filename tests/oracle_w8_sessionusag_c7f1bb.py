#!/usr/bin/env python3
"""Differential oracle for gateway/session_usage.py pure helpers (w8_sessionusag_c7f1bb)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# parent-package bootstrap: load the module under a real gateway package.
PKG = "w8_sessionusag_c7f1bb_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/gateway"]
sys.modules[PKG] = pkg
LIVE_NAME = PKG + ".session_usage"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/session_usage.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def ns(d):
    if not isinstance(d, dict):
        return d
    out = types.SimpleNamespace()
    for k, v in d.items():
        setattr(out, k, ns(v) if isinstance(v, dict) else v)
    return out


def compute(r):
    f = r.get("func")
    if f == "finite":
        return live._finite(r.get("value"))
    if f == "carried_compression_count":
        agent = r.get("agent")
        return live.carried_compression_count(ns(r.get("live") or {}), ns(agent))
    if f == "stored_stats":
        return live._stored_stats(r.get("row") or {})
    if f == "tui_usage":
        return live.tui_usage(r.get("stats") or {})
    if f == "usage_frame":
        return live.usage_frame(r.get("stats") or {})
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
print("ORACLE w8_sessionusag_c7f1bb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
