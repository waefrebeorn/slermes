#!/usr/bin/env python3
"""sta_oracle_web_server_events.py — LIVE-Python oracle for the events/PTY
support layer. Extracts the real function sources from web_server.py and
execs them with faithful app-state doubles."""
import ast
import json
import os
import sys
import urllib.parse
from pathlib import Path

WEB_SERVER = Path("/home/wubu/hermes-agent-dev/hermes_cli/web_server.py")

WANT_FUNCS = {"_ws_close_reason", "_resolve_client_ws_host",
              "_build_gateway_ws_url", "_build_sidecar_url"}
WANT_CONSTS = {"_WILDCARD_HOSTS"}


class AppState:
    pass


class App:
    def __init__(self):
        self.state = AppState()


def load_live(bound_host, bound_port, auth_required, token, internal):
    src = WEB_SERVER.read_text(encoding="utf-8")
    tree = ast.parse(src)
    pieces = []
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANT_FUNCS:
            pieces.append(ast.get_source_segment(src, node))
        elif isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id in WANT_CONSTS:
                    pieces.append(ast.get_source_segment(src, node))

    app = App()
    app.state.bound_host = bound_host
    app.state.bound_port = bound_port
    app.state.auth_required = auth_required

    # stub module for the gated-mode import inside the functions
    import types
    dash = types.ModuleType("hermes_cli.dashboard_auth.ws_tickets")
    dash.internal_ws_credential = lambda: internal
    pkg = types.ModuleType("hermes_cli.dashboard_auth")
    pkg.ws_tickets = dash
    root = types.ModuleType("hermes_cli")
    root.dashboard_auth = pkg
    sys.modules.setdefault("hermes_cli", root)
    sys.modules["hermes_cli.dashboard_auth"] = pkg
    sys.modules["hermes_cli.dashboard_auth.ws_tickets"] = dash

    ns = {"os": os, "urllib": urllib, "app": app, "frozenset": frozenset,
          "_SESSION_TOKEN": token, "Optional": None}
    import typing
    ns["Optional"] = typing.Optional
    exec("\n\n".join(pieces), ns)
    missing = (WANT_FUNCS | WANT_CONSTS) - set(ns)
    if missing:
        raise SystemExit(f"oracle failed to extract: {missing}")
    return ns


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op = fx["op"]

    if op == "close_reason":
        ns = load_live(None, None, False, "", "")
        print(json.dumps({"reason": ns["_ws_close_reason"](fx["text"])},
                         separators=(",", ":"), ensure_ascii=False))
    elif op == "resolve_host":
        env = fx.get("env")
        if env is not None:
            os.environ["HERMES_DASHBOARD_WS_HOST"] = env
        else:
            os.environ.pop("HERMES_DASHBOARD_WS_HOST", None)
        ns = load_live(fx.get("bound") or None, None, False, "", "")
        print(json.dumps({"host": ns["_resolve_client_ws_host"]()},
                         separators=(",", ":")))
    elif op == "gateway_url":
        os.environ.pop("HERMES_DASHBOARD_WS_HOST", None)
        ns = load_live(fx.get("bound") or None, fx.get("port") or None,
                       fx.get("auth", False), fx.get("token", ""),
                       fx.get("internal", ""))
        print(json.dumps({"url": ns["_build_gateway_ws_url"]()},
                         separators=(",", ":")))
    elif op == "sidecar_url":
        os.environ.pop("HERMES_DASHBOARD_WS_HOST", None)
        ns = load_live(fx.get("bound") or None, fx.get("port") or None,
                       fx.get("auth", False), fx.get("token", ""),
                       fx.get("internal", ""))
        print(json.dumps({"url": ns["_build_sidecar_url"](fx["channel"])},
                         separators=(",", ":")))
    elif op == "theme_esc":
        # _esc is a closure inside _render_active_theme_bootstrap_css:
        # replicate its one-liner verbatim from source
        src = WEB_SERVER.read_text(encoding="utf-8")
        assert 'return str(s).replace("</", "<\\\\/")' in src
        print(json.dumps({"esc": str(fx["text"]).replace("</", "<\\/")},
                         separators=(",", ":"), ensure_ascii=False))
    elif op == "registry_scenario":
        # Behavioral contract is asserted inside the C harness itself.
        print(json.dumps({"registry_fails": 0}, separators=(",", ":")))
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
