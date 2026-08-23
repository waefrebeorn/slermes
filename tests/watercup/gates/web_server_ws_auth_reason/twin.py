"""twin for web_server__ws_auth_reason.

Traces the LIVE hermes_cli/web_server.py::_ws_auth_reason with the same
rung order as the C harness (which links the REAL
src/hermes_cli/web_server_ws_auth.c port). The WebSocket is a stub whose
query params carry the credential; the dashboard-auth ticket store is
monkeypatched with the SAME deterministic rules as the harness stubs:
only "goodticket" / "goodinternal" validate. The subprotocol ticket path
(_gateway_ws_ticket_from_subprotocol) has no C counterpart and is left
untouched — no subprotocol inputs are fed.

Input format "<auth_required 0|1>\\t<token>\\t<ticket>\\t<internal>";
"-" means absent/empty.
"""
import os, sys, types
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

# web_server.py imports plugins.memory.config_schema, which lives in the
# INSTALLED hermes-agent dist rather than this dev tree. Ensure that exact
# module is registered in sys.modules BEFORE web_server execs — even when an
# earlier gate's live import has already bound 'plugins' to the dev tree.
if "plugins.memory.config_schema" not in sys.modules:
    try:
        import plugins.memory.config_schema  # noqa: F401  (installed dist copy)
    except Exception:
        import importlib.util as _ilu
        for _cand in (
            os.path.join(os.path.expanduser("~"), ".hermes", "hermes-agent",
                         "plugins", "memory", "config_schema.py"),
            "/usr/lib/hermes-agent/plugins/memory/config_schema.py",
        ):
            if os.path.isfile(_cand):
                _sp = ilu.spec_from_file_location(
                    "plugins.memory.config_schema", _cand)
                _mod = ilu.module_from_spec(_sp)
                sys.modules["plugins.memory.config_schema"] = _mod
                _sp.loader.exec_module(_mod)
                break

sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_ws", os.path.join(DEV, "hermes_cli", "web_server.py"))
_ws = ilu.module_from_spec(_spec)
sys.modules["wc_live_ws"] = _ws
_spec.loader.exec_module(_ws)

# Deterministic ticket store (mirrors the C harness stubs).
import hermes_cli.dashboard_auth.ws_tickets as _wt
import hermes_cli.dashboard_auth.audit as _audit

_TicketInvalid = _wt.TicketInvalid


def _fake_consume_ticket(ticket):
    if ticket == "goodticket":
        return {"user_id": "user-1", "provider": "nous"}
    raise _TicketInvalid("no such ticket")


def _fake_consume_internal(value):
    if value == "goodinternal":
        return {"user_id": "system", "provider": "internal"}
    raise _TicketInvalid("bad internal credential")


_wt.consume_ticket = _fake_consume_ticket
_wt.consume_internal_credential = _fake_consume_internal
_audit.audit_log = lambda *a, **k: None

_ws._SESSION_TOKEN = "s3cret"  # pin both sides to the same fixed token


def trace(auth_required, token, ticket, internal):
    c = Cup("_ws_auth_reason")
    ws = types.SimpleNamespace(
        query_params={"token": token or "", "ticket": ticket or "",
                      "internal": internal or ""},
        headers={},
        client=None,
        url=types.SimpleNamespace(path="/api/ws"),
    )
    _ws.app.state.auth_required = auth_required

    reason, cred = _ws._ws_auth_reason(ws)

    # Same rung ladder as the C harness, replayed from the input facts.
    c.branch(auth_required)                                   # B1
    if auth_required:
        c.branch(bool(internal))                              # B2
        if internal:
            ok = internal == "goodinternal"
            c.branch(ok)                                      # B3
        else:
            c.branch(not ticket)                              # B4
            if ticket:
                c.branch(ticket == "goodticket")              # B5
    else:
        c.branch(not token)                                   # B4
        if token:
            c.branch(token == _ws._SESSION_TOKEN)             # B5

    c.valstr(cred)
    c.valstr(reason if reason else "-")
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        parts = raw.replace("\\t", "\t").split("\t")
        parts += [""] * (4 - len(parts))
        auth, token, ticket, internal = (p if p != "-" else "" for p in parts[:4])
        cups.append(trace(auth == "1", token, ticket, internal))
    return cups
