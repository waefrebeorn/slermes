#!/usr/bin/env python3
"""Differential oracle for live Hermes gateway/session_prune.py (port w8_sessionprun_f0679c).

Live prune() is driven with a fake connection/actor/authority; track_mutation is
seeded with a sentinel so validation is exercised without touching any store.
"""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
import gateway  # noqa: F401  parent-package bootstrap
import gateway.session_runtime_workers as srw

SRC = QUARRY + "/gateway/session_prune.py"
NAME = "session_prune_live_w8"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


class _Validated(Exception):
    pass


def _fake_track(authority, coro):
    close = getattr(coro, "close", None)
    if close:
        close()
    raise _Validated()


srw.track_mutation = _fake_track


def compute(r):
    func = r.get("func")
    if func == "filters":
        try:
            return {"ok": True, "value": live._filters(r.get("raw"))}
        except live.AuthorityRefusal:
            return {"ok": False, "value": None}
    if func == "prune_guard":
        authority = types.SimpleNamespace(
            profile_id=r.get("authority_profile"),
            _require_admission_open=lambda: None)
        actor = types.SimpleNamespace(
            profile_id=r.get("actor_profile"),
            capabilities=r.get("capabilities", []))
        conn = types.SimpleNamespace(
            authority=authority, actor=actor,
            native_owner=bool(r.get("native_owner", False)))
        try:
            asyncio.run(live.prune(conn, None, r.get("params", {})))
            return "ok"
        except _Validated:
            return "ok"
        except live.AuthorityRefusal as exc:
            return exc.reason
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_sessionprun_f0679c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
