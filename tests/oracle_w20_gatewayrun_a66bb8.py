#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_runtime_attach.py (w20_gatewayrun_a66bb8).

The live module is imported against stubbed runtime controls (discover/ensure,
the profile-home resolver, the control socket and the ticket RPC) and against
the real ``hermes_cli.gateway_runtime_discovery.DiscoveryError`` (loaded from
the quarry, so ``bounded_detail`` is compared against its own implementation).
Each harness line is replayed through the live helper with the same inputs.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _load(name, path):
    sp = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(sp)
    sys.modules[name] = mod
    sp.loader.exec_module(mod)
    return mod


for pkg in ("hermes_cli", "hermes_platform"):
    if pkg not in sys.modules:
        _mk(pkg)

disc = _load("hermes_cli.gateway_runtime_discovery",
             os.path.join(Q, "hermes_cli", "gateway_runtime_discovery.py"))
DiscoveryError = disc.DiscoveryError

REC = {}


class Receipt:
    def __init__(self, spec):
        spec = spec if isinstance(spec, dict) else {}
        self.state = spec.get("state")
        self.reason_code = spec.get("reason_code")
        self.detail = spec.get("detail")
        self.attachable = bool(spec.get("attachable", False))
        self.endpoint = spec.get("endpoint")


RECEIPTS = {}


def _gateway_runtime():
    m = _mk("hermes_cli.gateway_runtime")

    def gateway_profile_home(home):
        REC["home"] = str(home)
        return home

    def attachable(receipt):
        return bool(getattr(receipt, "attachable", False))

    def discover_gateway_endpoint(home, timeout=None):
        REC["path"] = "discover"
        REC["discover_timeout"] = timeout
        return RECEIPTS.get("discover")

    def ensure_gateway_runtime(home, timeout=None, idle_exit=True):
        REC["path"] = "ensure"
        REC["ensures"] = REC.get("ensures", 0) + 1
        REC["ensure_timeout"] = timeout
        REC["idle_exit"] = idle_exit
        return RECEIPTS.get("ensure")

    m.gateway_profile_home = gateway_profile_home
    m.attachable = attachable
    m.discover_gateway_endpoint = discover_gateway_endpoint
    m.ensure_gateway_runtime = ensure_gateway_runtime
    return m


def _gateway_client():
    m = _mk("hermes_cli.gateway_client")

    def _session_ticket(home, endpoint, **kw):
        REC["mint_home"] = str(home)
        REC["mint_endpoint"] = endpoint
        REC["mint_kwargs"] = dict(kw)
        err = REC.get("mint_error")
        if err == "DiscoveryError":
            raise DiscoveryError(REC.get("mint_reason"), REC.get("mint_detail"), REC.get("mint_fix"))
        if err:
            raise {"OSError": OSError, "ValueError": ValueError, "RuntimeError": RuntimeError,
                   "KeyError": KeyError}.get(err, RuntimeError)(REC.get("mint_message", "scripted"))
        return REC.get("ticket", "ticket-1")

    m._session_ticket = _session_ticket
    return m


_gateway_runtime()
_gateway_client()
_mk("hermes_dir_acl").process_is_elevated = lambda: REC.get("elevated", False)
host = _mk("hermes_platform.host")
host.windows_session_id = lambda: REC.get("session_id")

NAME = "w20_gatewayrun_a66bb8_live"
live = _load(NAME, os.path.join(Q, "hermes_cli", "gateway_runtime_attach.py"))


def _attach_expected(r):
    REC.clear()
    REC["ensures"] = 0
    REC["ticket"] = r.get("mint", {}).get("ticket", "ticket-1") if isinstance(r.get("mint"), dict) else "ticket-1"
    mint = r.get("mint") if isinstance(r.get("mint"), dict) else {}
    if mint.get("error"):
        REC["mint_error"] = mint["error"]
        REC["mint_reason"] = mint.get("reason")
        REC["mint_detail"] = mint.get("detail")
        REC["mint_fix"] = mint.get("fix")
        REC["mint_message"] = mint.get("message", "scripted")
    RECEIPTS.clear()
    receipts = r.get("receipts") if isinstance(r.get("receipts"), dict) else {}
    RECEIPTS["discover"] = Receipt(receipts.get("discover"))
    RECEIPTS["ensure"] = Receipt(receipts.get("ensure"))
    try:
        result = live.attach_gateway(r.get("home"), start=bool(r.get("start")), recover=bool(r.get("recover")),
                                     timeout=r.get("timeout", 0.0))
    except Exception as exc:  # noqa: BLE001 - the harness reports raised exceptions verbatim
        return {"status": "raised", "type": type(exc).__name__, "message": str(exc),
                "reason": getattr(exc, "reason", None), "detail": getattr(exc, "detail", None)}
    kwargs = REC.get("mint_kwargs") or {}
    return {
        "path": REC.get("path"),
        "ensures": REC.get("ensures", 0),
        "discover_timeout": REC.get("discover_timeout"),
        "ensure_timeout": REC.get("ensure_timeout"),
        "attached": result.ticket is not None,
        "state": result.receipt.state if result.receipt else None,
        "ticket": result.ticket,
        "mint": {
            "home": REC.get("mint_home"),
            "endpoint": REC.get("mint_endpoint"),
            "purpose": kwargs.get("purpose"),
            "scope": kwargs.get("scope"),
        } if result.ticket is not None else None,
    }


def expected(r):
    f = r.get("func")
    if f == "client_launch_env":
        return list(live.CLIENT_LAUNCH_ENV)
    if f == "discover_timeout":
        return live.DISCOVER_TIMEOUT
    if f == "service_session_warning":
        return live.SERVICE_SESSION_START_WARNING
    if f == "windows_service_session":
        REC.clear()
        REC["session_id"] = r.get("session_id")
        REC["elevated"] = bool(r.get("elevated", False))
        if r.get("error"):
            def boom():
                raise {"AttributeError": AttributeError, "OSError": OSError,
                       "TypeError": TypeError, "ValueError": ValueError}.get(r["error"], RuntimeError)("probe")
            host.windows_session_id = boom
        else:
            host.windows_session_id = lambda: REC.get("session_id")
        try:
            return live.windows_service_session()
        finally:
            host.windows_session_id = lambda: REC.get("session_id")
    if f == "failure_message":
        return live.failure_message(Receipt(r.get("receipt")))
    if f == "bounded_detail":
        err = DiscoveryError("scripted", r.get("detail"), r.get("fix"))
        return err.bounded_detail(int(r.get("limit", 500)))
    if f == "ticket_unavailable":
        exc = live.TicketUnavailable(r.get("reason", ""), r.get("detail"))
        return {"message": str(exc), "reason": exc.reason, "detail": exc.detail}
    if f == "attach":
        return _attach_expected(r)
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
        exp = expected(r)
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
print("ORACLE w20_gatewayrun_a66bb8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
