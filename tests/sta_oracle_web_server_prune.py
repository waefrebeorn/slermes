#!/usr/bin/env python3
"""sta_oracle_web_server_prune.py — LIVE-Python oracle for the prune engine.
Uses the REAL SessionDB class for candidates/prune, and the REAL extracted
_prune_sessions endpoint body with the real pydantic SessionPrune model."""
import ast
import json
import sys
from pathlib import Path

HERMES = Path("/home/wubu/hermes-agent-dev")  # live upstream tree
sys.path.insert(0, str(HERMES))
from hermes_state import SessionDB  # noqa: E402

WEB_SERVER = HERMES / "hermes_cli/web_server.py"


class HTTPException(Exception):
    def __init__(self, status_code=500, detail=""):
        self.status_code = status_code
        self.detail = detail


def build_endpoint(dbp):
    src = WEB_SERVER.read_text(encoding="utf-8")
    tree = ast.parse(src)
    pieces = []
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SessionPrune":
            pieces.append(ast.get_source_segment(src, node))
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and \
                node.name == "_prune_sessions":
            pieces.append(ast.get_source_segment(src, node))
    from typing import Any, Dict, List, Optional
    from pydantic import BaseModel
    ns = {
        "BaseModel": BaseModel, "HTTPException": HTTPException,
        "Optional": Optional, "Any": Any, "Dict": Dict, "List": List,
        "Path": Path,
        "_open_session_db_for_profile": lambda profile: SessionDB(dbp),
        "_cron_profile_home": lambda p: (_ for _ in ()).throw(
            AssertionError("profile not used in fixtures")),
        "get_hermes_home": lambda: Path("/nonexistent-hermes-home"),
    }
    exec("from __future__ import annotations\n" + "\n\n".join(pieces), ns)
    ns["SessionPrune"].model_rebuild(_types_namespace=ns)
    return ns


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op = fx["op"]
    dbp = Path(fx["db"])

    def dump(o):
        print(json.dumps(o, separators=(",", ":"), ensure_ascii=False))

    filters = dict(fx.get("filters") or {})
    older = fx.get("older")

    if op == "candidates":
        db = SessionDB(dbp)
        rows = db.list_prune_candidates(older_than_days=older, **filters)
        db.close()
        dump(rows)
    elif op == "prune":
        db = SessionDB(dbp)
        removed = db.prune_sessions(older_than_days=older, **filters)
        remaining = db.session_count(include_archived=True)
        db.close()
        dump({"removed": removed, "remaining": remaining})
    elif op == "endpoint":
        ns = build_endpoint(dbp)
        body = ns["SessionPrune"](**(fx.get("body") or {}))
        try:
            dump(ns["_prune_sessions"](body))
        except HTTPException as e:
            dump({"status": e.status_code, "detail": e.detail})
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
