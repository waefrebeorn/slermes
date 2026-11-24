#!/usr/bin/env python3
"""Differential oracle for gateway/session_hosted_service.py (w18_sessionhos_6fbeb0).

Each line on stdin is one harness response (all fixture keys echoed plus
``out``). The same inputs are replayed against the LIVE module: ``_check_claim``
runs for real (with the quarry's own IDENTIFIER_RE), ``attempt_retired`` and
``retired_attempt_conflict`` are called directly, ``start_ready_hosted_services``
runs against scripted runner/authority fakes that record start() and
import_after_ready calls, and ``room_profiles`` is invoked as an unbound method
on a self whose ``local_profiles()`` returns the fixture tuple.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = [os.path.join(Q, *name.split("."))]
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for _name in ("gateway", "persistence", "persistence.core", "persistence.gateway",
              "tui_gateway"):
    _pkg(_name)


def _one_line(value, limit):
    """persistence.core.errors._one_line, verbatim."""
    if value is None:
        return None
    text = "".join(ch for ch in " ".join(str(value).split()) if ch.isprintable())
    return text[:limit] or None


class AuthorityRefusal(Exception):
    """persistence.core.errors.AuthorityRefusal with the same public surface."""

    def __init__(self, reason, *, field=None, detail=None):
        self.reason = reason
        self.field = _one_line(field, 80)
        self.detail = _one_line(detail, 160)
        super().__init__(reason)


class _HostedControls:
    pass


class _HostedRoomService:
    pass


class _HostedTaskIdentity:
    def __init__(self, **kw):
        self.kw = kw


class _HostedOwnerClaim:
    def __init__(self, **kw):
        self.kw = kw


_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
_mod("persistence.gateway.hosted_rooms",
     HostedOwnerClaim=_HostedOwnerClaim, HostedTaskIdentity=_HostedTaskIdentity)
_mod("gateway.session_contract", SessionActor=lambda *a, **k: object())
_mod("gateway.session_authorities",
     active_authority=lambda runner: None,
     all_authorities=lambda runner: [],
     serving_authorities=lambda runner: [],
     owner_scope=lambda authority, **kw: None)
_mod("gateway.session_hosted_controls", HostedControls=_HostedControls)
_mod("gateway.owner_identity", same_owner=lambda a, b: a == b)
_mod("gateway.session_persistence",
     authority_persistence=lambda a: None, owner_rule=lambda: None)
_mod("tui_gateway.hosted_room_service", HostedRoomService=_HostedRoomService,
     task_member_id=lambda task: (task.get("payload") or {}).get("target_member_id"))

IMPORT_CALLS = {"n": 0}
_mod("gateway.hosted_rooms_gateway_import",
     import_after_ready=lambda authorities: IMPORT_CALLS.__setitem__("n", IMPORT_CALLS["n"] + 1),
     import_standalone_rooms=lambda *a, **k: None)

NAME = "w18_sessionhos_6fbeb0_live"
spec = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "session_hosted_service.py"))
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

# The lazily-imported leaf used by _check_claim: the quarry's real module, so
# IDENTIFIER_RE is the live regex and not a copy.
import gateway.hosted_rooms_common  # noqa: E402  (after the gateway package stub)


def _attempt_retired(r):
    return bool(live.attempt_retired(r.get("task"), r.get("generation")))


def _check_claim(r):
    try:
        live._check_claim(r.get("actor_subject"), r.get("room_id"))
        return None
    except AuthorityRefusal as exc:
        return exc.reason


def _retired_conflict(r):
    exc = live.retired_attempt_conflict()
    return {"reason": exc.reason, "field": exc.field, "detail": exc.detail}


def _start_ready(r):
    started = []

    class Svc:
        def __init__(self, installed):
            self._transport_installed = installed

        def start(self):
            started.append(1)

    services = []
    for entry in r.get("services") or []:
        if entry is None:
            services.append(None)
        elif isinstance(entry, dict):
            services.append(Svc(entry.get("transport_installed", False)))
        else:
            services.append(Svc(False))

    runner = types.SimpleNamespace(
        session_runtime_descriptor={"state": r.get("state")},
        _draining=r.get("draining", False))
    live.serving_authorities = lambda _runner: [
        types.SimpleNamespace(hosted_room_service=svc) for svc in services]
    IMPORT_CALLS["n"] = 0
    live.start_ready_hosted_services(runner)
    return {"action": "start" if IMPORT_CALLS["n"] else "skip",
            "started": len(started)}


def _room_profiles(r):
    served = tuple(r.get("local_profiles") or [])
    fake_self = types.SimpleNamespace(local_profiles=lambda: served)
    return list(live.CanonicalHostedRoomService.room_profiles(fake_self, r.get("room")))


def _stop_read(r):
    return r.get("operation") in live._STOP_READS


HANDLERS = {
    "check_claim": _check_claim,
    "attempt_retired": _attempt_retired,
    "retired_attempt_conflict": _retired_conflict,
    "start_ready": _start_ready,
    "room_profiles": _room_profiles,
    "stop_read": _stop_read,
}

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    handler = HANDLERS.get(func)
    if handler is None:
        continue
    cases += 1
    try:
        py = handler(r)
    except Exception as exc:  # a live error is a divergence worth naming
        mism += 1
        print("MISMATCH %s case=%r live-error %r" % (func, r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%r py=%r c=%r" % (func, r.get("case"), py, got),
              file=sys.stderr)

print(f"ORACLE w18_sessionhos_6fbeb0: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
