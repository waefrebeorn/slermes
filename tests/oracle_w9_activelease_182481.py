#!/usr/bin/env python3
"""Differential oracle for gateway/active_lease_io.py (gate w9_activelease_182481)."""
import concurrent.futures
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# hermes_cli.active_sessions is imported lazily inside release_soon.
_hc = types.ModuleType("hermes_cli")
_hc.__path__ = [QUARRY + "/hermes_cli"]
sys.modules.setdefault("hermes_cli", _hc)
_as = types.ModuleType("hermes_cli.active_sessions")
events = {}


def mark_release_queued(lease):
    events["marked"] = True


_as.mark_release_queued = mark_release_queued
sys.modules["hermes_cli.active_sessions"] = _as

SRC = QUARRY + "/gateway/active_lease_io.py"
NAME = "active_lease_io_live_w9_activelease"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


class FakePool:
    def __init__(self, max_workers, thread_name_prefix):
        events["pool_created"] = True
        events["max_workers"] = max_workers

    def submit(self, fn, lease):
        events["submitted"] = True
        try:
            fn(lease)
        except BaseException:
            events["propagated"] = True


class FakeLease:
    def __init__(self, raises):
        self.raises = raises

    def release(self):
        events["released"] = True
        if self.raises:
            raise RuntimeError("release failed")


_orig_pool_cls = concurrent.futures.ThreadPoolExecutor
concurrent.futures.ThreadPoolExecutor = FakePool

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "release":
        continue
    live._pool = FakePool(1, "preexisting") if r.get("pool_exists") else None
    events.clear()
    try:
        live.release_soon(FakeLease(bool(r.get("release_raises"))))
    except BaseException:
        events["propagated"] = True
    py = {
        "marked": bool(events.get("marked")),
        "pool_created": bool(events.get("pool_created")),
        "submitted": bool(events.get("submitted")),
        "released": bool(events.get("released")),
        "propagated": bool(events.get("propagated")),
    }
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

concurrent.futures.ThreadPoolExecutor = _orig_pool_cls
print("ORACLE w9_activelease_182481: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
