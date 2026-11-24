#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_router_call.py
(gate w20_hostedroom_8052fe).

The live module is imported against stubbed transports: `spawn_context_thread`
runs the worker inline, `read_events` returns the fixture page, and
`read_profile_meta` answers from the fixture's homes map. Everything compared —
the roster, the recent-message digest, the handle map and RouterCalls.start's
claim gate — is the live source executing.
"""
import importlib.util
import json
import logging
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

GATE = "w20_hostedroom_8052fe"
NAME = GATE + "_live"


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


def _load(dotted, path):
    spec = importlib.util.spec_from_file_location(dotted, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = mod
    spec.loader.exec_module(mod)
    return mod


CUR = {"meta": {}, "page": {"events": []}, "logs": []}

_mk("agent")
_mk("agent.memory_provider", spawn_context_thread=lambda fn, name=None: SimpleNamespace(start=fn))
_mk("persistence")
_mk("persistence.gateway")
_mk("persistence.gateway.hosted_rooms", HostedPlacement=object)
_mk("persistence_assembly")
_mk("hermes_cli")
_mk("hermes_cli.profiles",
    read_profile_meta=lambda home: {"description": CUR["meta"].get(str(home), "")})

_gw = _mk("gateway")
RR = _load("gateway.hosted_room_router", os.path.join(Q, "gateway", "hosted_room_router.py"))
setattr(_gw, "hosted_room_router", RR)

HR = _mk("gateway.hosted_rooms", read_events=lambda *a, **k: CUR["page"])
setattr(_gw, "hosted_rooms", HR)

spec = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "tui_gateway", "hosted_room_router_call.py"))
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

LOG = logging.getLogger(NAME)
LOG.setLevel(logging.DEBUG)
LOG.propagate = False


class Capture(logging.Handler):
    def __init__(self, sink):
        super().__init__(logging.DEBUG)
        self.sink = sink

    def emit(self, record):
        self.sink.append(record.getMessage())


def run_roster(r):
    room = r.get("room") or {}
    homes = r.get("homes") if isinstance(r.get("homes"), dict) else {}
    CUR["meta"] = {}
    paths = {}
    for k, v in homes.items():
        p = Path("/profiles/" + k)
        paths[k] = p
        CUR["meta"][str(p)] = v
    return live._member_roster(room, paths)


def run_recent(r):
    room_members = r.get("members") or []
    CUR["page"] = {"events": r.get("events") or []}
    return live._recent_messages(None, None, {"room_id": "r1", "members": room_members},
                                 int(r.get("before_seq") or 0))


def run_start(r):
    now = float(r.get("now") or 0)
    claim = r.get("claim")
    call_result = r.get("call_result")
    sink = {"claims": [], "releases": [], "recorded": None, "record_called": False,
            "record_ok": True, "release_called": False, "release_ok": True, "logs": []}

    class Store:
        hosted_rooms = None          # `store.hosted_rooms.claim_router_call(...)`

        def claim_router_call(self, **kw):
            sink["claims"].append(kw)
            if isinstance(claim, dict) and "__raise__" in claim:
                raise RuntimeError(claim["__raise__"].get("message", ""))
            return bool(claim)

        def release_router_call(self, **kw):
            sink["releases"].append(kw)
            sink["release_called"] = True
            if r.get("release_raises"):
                sink["release_ok"] = False
                raise RuntimeError("release failed")

    store = Store()
    store.hosted_rooms = store
    rc = live.RouterCalls(store, object(), clock=lambda: now)
    key = (r.get("room_id") or "", r.get("discussion_event_id") or "")
    if r.get("already_inflight"):
        rc._inflight.add(key)

    def call():
        return None if call_result is None else tuple(call_result)

    def record(value):
        sink["record_called"] = True
        if r.get("record_raises"):
            sink["record_ok"] = False
            raise RuntimeError("record failed")
        sink["recorded"] = None if value is None else list(value)

    handler = Capture(sink["logs"])
    LOG.addHandler(handler)
    raised = False
    started = False
    try:
        started = bool(rc.start(key, call, record))
    except BaseException:  # noqa: BLE001 - start() re-raises a failed claim
        raised = True
    finally:
        LOG.removeHandler(handler)

    claims = sink["claims"]
    return {
        "started": started and not raised,
        "raised": raised,
        "claim_called": bool(claims),
        "expires_at": float(claims[0]["expires_at"]) if claims else 0.0,
        "now": float(claims[0]["now"]) if claims else now,
        "record_called": sink["record_called"],
        "recorded": sink["recorded"],
        "record_ok": sink["record_ok"],
        "release_called": sink["release_called"],
        "release_ok": sink["release_ok"],
        "inflight_after": rc.inflight(key),
        "logs": sink["logs"],
    }


def run_case(r):
    f = r.get("func")
    if f == "member_roster":
        return run_roster(r)
    if f == "recent_messages":
        return run_recent(r)
    if f == "start":
        return run_start(r)
    if f == "handles":
        members = r.get("members") or []
        handles = {str(m["handle"]).casefold(): str(m["member_id"]) for m in members}
        return [[k, v] for k, v in handles.items()]
    if f == "claim_ttl":
        return live.CLAIM_TTL_SECONDS
    raise AssertionError("unknown func %r" % (f,))


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
