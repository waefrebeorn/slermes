#!/usr/bin/env python3
"""sta_oracle_web_server_ws_auth.py — LIVE-Python oracle for the WS gate
family. Extracts the real functions from web_server.py + the real
ws_tickets module, execs them against a faked app.state/WebSocket."""
import ast
import json
import sys
import types
from pathlib import Path

WEB_SERVER = Path("/home/wubu/hermes-agent-dev/hermes_cli/web_server.py")
WS_TICKETS = Path("/home/wubu/hermes-agent-dev/hermes_cli/dashboard_auth/ws_tickets.py")

WANT_FUNCS = {
    "_ws_client_reason", "_ws_client_is_allowed", "_ws_host_origin_reason",
    "_ws_host_origin_is_allowed", "_ws_request_reason",
    "_ws_request_is_allowed", "_ws_auth_mode", "_ws_auth_reason",
    "_ws_auth_ok", "_has_valid_query_token", "_is_accepted_host",
}
WANT_CONSTS = {"_LOOPBACK_HOSTS", "_LOOPBACK_HOST_VALUES",
               "_QUERY_TOKEN_API_PATHS"}


class NS:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class FakeURL:
    path = "/api/ws"


class FakeWS:
    def __init__(self, req):
        client_host = req.get("client_host")
        self.client = NS(host=client_host) if client_host is not None else None
        self.headers = {}
        if req.get("host_header") is not None:
            self.headers["host"] = req["host_header"]
        if req.get("origin_header") is not None:
            self.headers["origin"] = req["origin_header"]
        self.query_params = {}
        for k in ("token", "ticket", "internal"):
            if req.get(k) is not None:
                self.query_params[k] = req[k]
        self.url = FakeURL()


class FakeRequest:
    def __init__(self, token):
        self.query_params = {"token": token} if token is not None else {}


def load(state):
    src = WEB_SERVER.read_text(encoding="utf-8")
    tree = ast.parse(src)
    pieces = []
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANT_FUNCS:
            pieces.append(ast.get_source_segment(src, node))
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for t in targets:
                if isinstance(t, ast.Name) and t.id in WANT_CONSTS:
                    pieces.append(ast.get_source_segment(src, node))

    # Real ws_tickets module, loaded as hermes_cli.dashboard_auth.ws_tickets
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "hermes_cli.dashboard_auth.ws_tickets", WS_TICKETS)
    ws_tickets = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ws_tickets)

    audit_mod = types.ModuleType("hermes_cli.dashboard_auth.audit")

    class AuditEvent:
        WS_TICKET_REJECTED = "ws_ticket_rejected"

    audit_mod.AuditEvent = AuditEvent
    audit_mod.audit_log = lambda *a, **k: None

    pkg = types.ModuleType("hermes_cli")
    da = types.ModuleType("hermes_cli.dashboard_auth")
    sys.modules["hermes_cli"] = pkg
    sys.modules["hermes_cli.dashboard_auth"] = da
    sys.modules["hermes_cli.dashboard_auth.ws_tickets"] = ws_tickets
    sys.modules["hermes_cli.dashboard_auth.audit"] = audit_mod

    import hmac
    import urllib.parse
    from typing import Optional

    app = NS(state=NS(auth_required=state.get("auth_required", False),
                      bound_host=state.get("bound_host")))
    ns = {
        "hmac": hmac, "urllib": urllib, "Optional": Optional,
        "app": app, "_SESSION_TOKEN": state.get("session_token", ""),
        "frozenset": frozenset,
    }
    exec("from __future__ import annotations\n" + "\n\n".join(pieces), ns)
    missing = (WANT_FUNCS | WANT_CONSTS) - set(ns)
    if missing:
        raise SystemExit(f"missing: {missing}")
    return ns, ws_tickets


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op = fx["op"]
    state = fx.get("state", {})
    ns, ws_tickets = load(state)
    req = fx.get("req", {})
    ws = FakeWS(req)

    if op == "gates":
        out = {
            "client_reason": ns["_ws_client_reason"](ws),
            "client_allowed": ns["_ws_client_is_allowed"](ws),
            "host_origin_reason": ns["_ws_host_origin_reason"](ws),
            "host_origin_allowed": ns["_ws_host_origin_is_allowed"](ws),
            "request_reason": ns["_ws_request_reason"](ws),
            "request_allowed": ns["_ws_request_is_allowed"](ws),
            "mode": ns["_ws_auth_mode"](),
        }
    elif op == "auth":
        if fx.get("mint"):
            ticket = ws_tickets.mint_ticket(user_id="user-1", provider="github")
            ws.query_params["ticket"] = ticket
        if fx.get("internal"):
            ws.query_params["internal"] = ws_tickets.internal_ws_credential()
        if fx.get("double_consume") and "ticket" in ws.query_params:
            try:
                ws_tickets.consume_ticket(ws.query_params["ticket"])
            except Exception:
                pass
        reason, credential = ns["_ws_auth_reason"](ws)
        out = {"reason": reason, "credential": credential,
               "ok": reason is None}
    elif op == "query_token":
        out = {"valid": ns["_has_valid_query_token"](
            FakeRequest(fx.get("token")), fx.get("path", ""))}
    else:
        raise SystemExit(f"unknown op {op}")
    print(json.dumps(out, separators=(",", ":"), ensure_ascii=False))


if __name__ == "__main__":
    main()
