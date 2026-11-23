#!/usr/bin/env python3
"""Differential oracle for hermes_state_registry.py (gate w5_hermesstate_732fec).

The hermes_state stand-in is registered in sys.modules so acquire() can be
observed without touching a real store; warnings are captured in-process.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

hs = types.ModuleType("hermes_state")
sys.modules["hermes_state"] = hs
_made = []


class SessionDB:
    def __init__(self, db_path=None):
        _made.append("SessionDB")
        self.closed = False

    def close(self):
        self.closed = True


class _SessionDB(SessionDB):
    def __init__(self, db_path=None):
        _made.append("_SessionDB")
        self.closed = False


hs.SessionDB = SessionDB
hs._SessionDB = _SessionDB
hs._first_warning = None
hs._REMOVAL_RELEASE = ""
hs._MIGRATION_GUIDE = ""

spec = importlib.util.spec_from_file_location("w5_hermesstate_live", Q + "/hermes_state_registry.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_hermesstate_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "release":
        is_none = bool(r.get("is_none", False))
        box = {"closed": False}

        class Db:
            def close(self):
                box["closed"] = True

        db = None if is_none else Db()
        res = live.release(db)
        return {"result": res, "closed": box["closed"]}
    if f == "acquire_variant":
        present = bool(r.get("first_warning_present", False))
        hs._first_warning = (lambda: bool(r.get("first_warning_result", False))) if present else None
        hs._REMOVAL_RELEASE = r.get("removal_release", "")
        hs._MIGRATION_GUIDE = r.get("migration_guide", "")
        del _made[:]
        warned = []
        live._warnings = types.SimpleNamespace(warn=lambda msg, *a, **k: warned.append(msg))
        live.acquire(123)
        return {"db_class": _made[-1] if _made else None,
                "warn": bool(warned), "msg": warned[0] if warned else None}
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH hermesstate %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_hermesstate_732fec: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
