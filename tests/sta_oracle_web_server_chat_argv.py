#!/usr/bin/env python3
"""sta_oracle_web_server_chat_argv.py — LIVE-Python oracle for the chat
argv/env cluster. Uses the REAL hermes_cli.profiles module, the REAL
_session_latest_descendant/_resolve_profile_dir sources, and the real
resolve_session_id contract against a sqlite session DB."""
import ast
import json
import sqlite3
import sys
import types
from pathlib import Path

HERMES = Path("/home/wubu/hermes-agent-dev")
WEB_SERVER = HERMES / "hermes_cli/web_server.py"


class HTTPException(Exception):
    def __init__(self, status_code=500, detail=""):
        self.status_code = status_code
        self.detail = detail


class DBShim:
    """Duck-typed shim exposing exactly what _session_latest_descendant
    uses: resolve_session_id, get_session, .conn (real sqlite3)."""

    def __init__(self, path):
        self.conn = sqlite3.connect(path)
        self.conn.row_factory = sqlite3.Row

    def get_session(self, sid):
        row = self.conn.execute(
            "SELECT id FROM sessions WHERE id = ?", (sid,)).fetchone()
        return dict(row) if row else None

    def resolve_session_id(self, sid_or_prefix):
        # Real hermes_state.py contract.
        exact = self.get_session(sid_or_prefix)
        if exact:
            return exact["id"]
        escaped = (sid_or_prefix.replace("\\", "\\\\")
                   .replace("%", "\\%").replace("_", "\\_"))
        rows = self.conn.execute(
            "SELECT id FROM sessions WHERE id LIKE ? ESCAPE '\\' "
            "ORDER BY started_at DESC LIMIT 2", (escaped + "%",)).fetchall()
        matches = [r["id"] for r in rows]
        return matches[0] if len(matches) == 1 else None

    def close(self):
        self.conn.close()


def extract(names):
    src = WEB_SERVER.read_text(encoding="utf-8")
    tree = ast.parse(src)
    return [ast.get_source_segment(src, node) for node in tree.body
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
            and node.name in names]


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op = fx["op"]

    if op == "profile_dir":
        sys.path.insert(0, str(HERMES))
        import os
        os.environ["HERMES_HOME"] = fx.get("home", "/nonexistent")
        from hermes_cli import profiles as profiles_mod
        from typing import Optional
        ns = {"HTTPException": HTTPException, "Optional": Optional,
              "Path": Path}
        pkg = types.ModuleType("hermes_cli")
        pkg.profiles = profiles_mod
        sys.modules.setdefault("hermes_cli", pkg)
        exec("from __future__ import annotations\n" +
             "\n\n".join(extract({"_resolve_profile_dir"})), ns)
        try:
            d = ns["_resolve_profile_dir"](fx["name"])
            print(json.dumps({"dir": str(d), "status": 0, "detail": None},
                             separators=(",", ":")))
        except HTTPException as e:
            print(json.dumps({"dir": None, "status": e.status_code,
                              "detail": e.detail}, separators=(",", ":")))
    elif op == "resolve_sid":
        db = DBShim(fx["db"])
        print(json.dumps({"id": db.resolve_session_id(fx["sid"])},
                         separators=(",", ":")))
        db.close()
    elif op == "descendant":
        ns = {}
        exec("from __future__ import annotations\n" +
             "\n\n".join(extract({"_session_latest_descendant"})), ns)
        db = DBShim(fx["db"])
        latest, path = ns["_session_latest_descendant"](fx["sid"], db)
        db.close()
        print(json.dumps({"latest": latest, "path": path},
                         separators=(",", ":")))
    elif op == "build_env":
        # Faithful replay of the env-assembly body of _resolve_chat_argv
        # (line-by-line vs web_server.py 17529-17590; the pure part the C
        # port implements — no npm build, no config bridge, no os.environ).
        env = dict(fx.get("base") or {})
        env.setdefault("NODE_ENV", "production")
        env.setdefault("HERMES_TUI_DISABLE_MOUSE", "1")
        env.setdefault("HERMES_TUI_INLINE", "1")
        env.setdefault("COLORTERM", "truecolor")
        env["HERMES_TUI_DASHBOARD"] = "1"
        profile_dir = fx.get("profile_dir")
        if profile_dir:
            env["HERMES_HOME"] = profile_dir
        if fx.get("resume"):
            env["HERMES_TUI_RESUME"] = fx["resume"]
        if fx.get("sidecar"):
            env["HERMES_TUI_SIDECAR_URL"] = fx["sidecar"]
        if fx.get("asf"):
            env["HERMES_TUI_ACTIVE_SESSION_FILE"] = fx["asf"]
        if not profile_dir and fx.get("gateway"):
            env["HERMES_TUI_GATEWAY_URL"] = fx["gateway"]
        print(json.dumps(env, separators=(",", ":"), ensure_ascii=False))
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
