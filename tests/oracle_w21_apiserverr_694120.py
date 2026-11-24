#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_room_dispatch.py
(gate w21_apiserverr_694120).

The live module source is imported as-is, so `_room_dispatch_error` and
`_normalize_room_dispatch` run their real bodies: the room-only-field refusal,
the accepted-key set, the grant/identity/digest/input/idempotency validations
and the error reclassification. The pieces that need a live service -- the
verified grant, the room catalog, the authority session mint and the aiohttp
request -- are scripted here (transport/config seams), never re-implemented.
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "api_server_room_dispatch.py"
GATE = "w21_apiserverr_694120"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

CASE = {}


def stub(name, **attrs):
    mod = types.ModuleType(name)
    mod.__dict__.update(attrs)
    sys.modules[name] = mod
    return mod


def _json_error(openai_error, message, code="", status=400):
    return {"kind": "error", "message": message, "code": code, "status": status}


class HostedMemberDispatch:
    def __init__(self, mapping):
        self._mapping = dict(mapping)
        for key, value in mapping.items():
            setattr(self, key, value)

    @classmethod
    def from_mapping(cls, mapping):
        if not isinstance(mapping, dict):
            raise ValueError("hosted_room_dispatch is invalid")
        return cls(mapping)

    def as_mapping(self):
        return dict(self._mapping)


def verify_room_grant(secret, token, dispatch, *, permission):
    if CASE.get("grant_error"):
        raise ValueError(CASE["grant_error"])


class RoomExecutionPolicy:
    def __init__(self, mapping):
        self._mapping = dict(mapping)

    @classmethod
    def from_mapping(cls, mapping):
        return cls(mapping)

    @property
    def policy_digest(self):
        return self._mapping.get("policy_digest")

    def as_mapping(self):
        return dict(self._mapping)


class GatewayRoomCatalog:
    def __init__(self, mapping):
        self._mapping = dict(mapping)
        self.execution_policy = RoomExecutionPolicy.from_mapping(
            mapping.get("execution_policy") or {})

    @classmethod
    def from_mapping(cls, mapping):
        return cls(mapping)

    @property
    def catalog_digest(self):
        return self._mapping.get("catalog_digest")


def _local_room_catalog(server, active_profile, local_install):
    return None, CASE.get("catalog") or {}


def request_target_profile(api_request_profile):
    return CASE.get("api_profile")


for _pkg in ("gateway", "gateway.platforms"):
    if _pkg not in sys.modules:
        stub(_pkg)
stub("gateway.platforms.api_server_room_grants", _json_error=_json_error,
     _local_room_catalog=_local_room_catalog, request_target_profile=request_target_profile)
stub("gateway.hosted_rooms", local_authority_gateway_id=lambda: CASE.get("local_install"))
stub("gateway.hosted_room_peer", GatewayRoomCatalog=GatewayRoomCatalog,
     HostedMemberDispatch=HostedMemberDispatch, verify_room_grant=verify_room_grant)
stub("gateway.hosted_room_execution_policy", RoomExecutionPolicy=RoomExecutionPolicy)

_spec = importlib.util.spec_from_file_location("api_server_room_dispatch_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["api_server_room_dispatch_live"] = live
_spec.loader.exec_module(live)


class Req:
    def __init__(self, headers):
        self.headers = headers or {}


class ApiServer:
    _openai_error = object()

    @property
    def _api_request_profile(self):
        return CASE.get("api_profile")


class Probe:
    _api_server = ApiServer()

    def _room_grant_token(self, request):
        return CASE.get("grant_token") or ""

    def _room_grant_secret(self):
        return "scripted-secret"

    async def _ensure_hosted_member_session(self, dispatch):
        return CASE.get("session_id") or ""


def canonical(result):
    body, resp = result
    if resp is not None:
        return resp
    if isinstance(body, dict) and "session_id" in body:
        return {"kind": "ok", "body": body}
    return {"kind": "passthrough", "body": body}


def expected(r):
    global CASE
    if r.get("func") == "error":
        return live._room_dispatch_error(Exception(r.get("message") or ""),
                                        _openai_error=object())
    CASE = r
    probe = Probe()
    return canonical(asyncio.run(live._normalize_room_dispatch(
        probe, Req(r.get("headers")), r.get("body"), _api_server=probe._api_server)))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in {"error", "normalize"}:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - a live raise is a harness bug
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
