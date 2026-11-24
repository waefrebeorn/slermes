#!/usr/bin/env python3
"""Differential oracle for agent/conversation_compression_watermark.py
(gate w20_conversati_6c3404).

The live module is imported from the quarry and its two store reads are driven
by a recording fake: `session_db.get_message_role` /
`session_db.rewritten_held_row_ids` (or the `persistence.messages` twins when
the fixture selects that seam). The marker constant and StaleHeldHistory are
the quarry's own, injected the way the module imports them at call time.
"""
import importlib.util
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

GATE = "w20_conversati_6c3404"


class StaleHeldHistory(RuntimeError):
    pass


def _mk(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


_mk("agent")
_mk("agent.context_compressor", _DB_PERSISTED_MARKER="_db_persisted",
    StaleHeldHistory=StaleHeldHistory)
_mk("agent.conversation_compression_archive", held_message=lambda m: m)
_mk("persistence")
_mk("persistence.core")
_mk("persistence.core.identity", MessageId=lambda v: v)

NAME = GATE + "_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "conversation_compression_watermark.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _id(v):
    if isinstance(v, bool):
        return None
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        try:
            return int(v)
        except ValueError:
            return None
    return None


class FakeStore:
    """get_message_role / rewritten_held_row_ids over the fixture's tables."""

    def __init__(self, spec):
        spec = spec if isinstance(spec, dict) else {}
        self.roles = spec.get("roles") if isinstance(spec.get("roles"), dict) else {}
        rw = spec.get("rewritten")
        self.rewritten = rw if isinstance(rw, list) else None
        self.has_rewritten = spec.get("has_rewritten", True) is not False
        self.calls = []

    def get_message_role(self, session_id, message_id):
        self.calls.append(("get_message_role", session_id, message_id))
        return self.roles.get(str(message_id))

    def rewritten_held_row_ids(self, session_id, held):
        self.calls.append(("rewritten_held_row_ids", session_id, len(list(held))))
        if not self.has_rewritten:
            raise AttributeError("no method")
        if self.rewritten is None:
            return None
        return tuple(_id(i) for i in self.rewritten)


def _store(spec, persistence):
    if persistence and isinstance(spec, dict) and isinstance(spec.get("persistence"), dict):
        spec = spec["persistence"]
    store = FakeStore(spec)
    if persistence:
        msgs = SimpleNamespace(message_role=store.get_message_role)
        if store.has_rewritten:
            msgs.rewritten_held_row_ids = store.rewritten_held_row_ids
        return SimpleNamespace(messages=msgs)
    if store.has_rewritten:
        return store
    # No rewritten_held_row_ids attribute at all: getattr(..., None) -> None.
    return SimpleNamespace(get_message_role=store.get_message_role, roles=store.roles)


def run_case(r):
    wm = r.get("watermark")
    if isinstance(wm, str):
        wm = int(wm)
    elif wm is not None and not isinstance(wm, int):
        wm = None
    messages = r.get("messages") if isinstance(r.get("messages"), list) else []
    tail = r.get("verbatim_tail")
    tail = tail if isinstance(tail, list) else None
    persistence = bool(r.get("persistence"))
    store = _store(r.get("db"), persistence)
    try:
        return {"status": "ok",
                "value": live.held_archive_watermark(
                    store, r.get("session_id", ""), wm, messages, tail,
                    stale_raises=bool(r.get("stale_raises")), persistence=store
                    if persistence else None)}
    except StaleHeldHistory as exc:
        return {"status": "raised", "type": "StaleHeldHistory", "message": str(exc)}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = run_case(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
