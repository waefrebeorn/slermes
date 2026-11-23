#!/usr/bin/env python3
"""Differential oracle for gateway/session_bot.py pure helpers (w7_sessionbot_29c875)."""
import importlib.util
import json
import os
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m


def _did(v):
    if not isinstance(v, str) or re.fullmatch(r"[0-9a-f]{32,64}", v) is None:
        raise ValueError("delivery id must be 32 to 64 lowercase hex characters")
    return v


stub("agent", __path__=[Q+"/agent"])
stub("agent.turn_author", parse_turn_author=lambda x: x)
stub("tools")
stub("tools.bot_failure_reasons", HELD_NOT_SENT="held_not_sent", UNKNOWN="unknown")
stub("tools.bot_live_delivery", _TERMINAL=set(), _delivery_id=_did, _read=lambda p: None,
     _ticket_shape_error=lambda path, record: "owner ticket error")
stub("gateway.session_contract", SessionActor=object, SessionRef=object)
stub("gateway.config", Platform=types.SimpleNamespace(LOCAL="local", API_SERVER="api_server"))
stub("gateway.platforms")
stub("gateway.platforms.event", MessageEvent=object)
stub("gateway.session_persistence", authority_persistence=lambda a: a, owner_rule=lambda a: None)
stub("gateway.session_bot_mailbox", mailbox_lock=None, read_receipt=None, write_receipt=None,
     scan_receipts=None, remember_receipt=None, track_receipt_task=None, withdraw_pin=None,
     refuse_pin=None, requeue_pin=None)
stub("persistence.core.errors", AuthorityRefusal=type("A", (Exception,), {}))

LIVE_NAME = "w7_sessionbot_29c875_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/session_bot.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

DEFAULT = ("admission_id", "profile_home", "session_id", "principal_id")


def compute(r):
    f = r.get("func")
    record = r.get("record") or {}
    path = types.SimpleNamespace(stem=r.get("stem", ""))
    if f == "canonical_record_error":
        req = r.get("required")
        return live._canonical_record_error(path, record, tuple(req) if req is not None else DEFAULT)
    if f == "record_shape_error":
        return live._record_shape_error(path, record)
    if f == "is_pre_admission_pin":
        return live._is_pre_admission_pin(path, record)
    if f == "retry_identity":
        return live._retry_identity(r.get("key", ""))
    if f == "refused_answer":
        return live._refused_answer(record)
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
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH sessionbot %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w7_sessionbot_29c875: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
