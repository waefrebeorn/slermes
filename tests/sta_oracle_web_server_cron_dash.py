#!/usr/bin/env python3
"""sta_oracle_web_server_cron_dash.py — LIVE-Python oracle for the dashboard
cron adapter layer. Extracts the real functions from web_server.py and runs
them with the real hermes_cli.profiles module."""
import ast
import json
import re
import sys
import types
from pathlib import Path

HERMES = Path("/home/wubu/hermes-agent-dev")
WEB_SERVER = HERMES / "hermes_cli/web_server.py"

WANT = {
    "_cron_optional_text", "_cron_string_list",
    "_normalize_dashboard_cron_script", "_validate_dashboard_cron_effective_job",
    "_normalize_dashboard_cron_updates", "_cron_default_profile",
    "_cron_profile_home", "_annotate_cron_job",
}


class HTTPException(Exception):
    def __init__(self, status_code=500, detail=""):
        self.status_code = status_code
        self.detail = detail


def load():
    src = WEB_SERVER.read_text(encoding="utf-8")
    tree = ast.parse(src)
    pieces = []
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANT:
            seg = ast.get_source_segment(src, node)
            assert seg is not None
            pieces.append(seg)

    sys.path.insert(0, str(HERMES))
    from typing import Any, Dict, List, Optional, Tuple
    ns = {
        "HTTPException": HTTPException, "Path": Path, "re": re,
        "Any": Any, "Dict": Dict, "List": List, "Optional": Optional,
        "Tuple": Tuple,
    }
    exec("from __future__ import annotations\n" + "\n\n".join(pieces), ns)
    return ns


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op = fx["op"]
    ns = load()

    def dump(o):
        print(json.dumps(o, separators=(",", ":"), ensure_ascii=False))

    if op == "script":
        try:
            rel = ns["_normalize_dashboard_cron_script"](
                fx.get("value"), Path(fx["home"]))
            dump({"rel": rel, "status": 0, "detail": None})
        except HTTPException as e:
            dump({"rel": None, "status": e.status_code, "detail": e.detail})
    elif op == "validate_job":
        try:
            ns["_validate_dashboard_cron_effective_job"](fx.get("job") or {})
            dump({"ok": True, "status": 0, "detail": None})
        except HTTPException as e:
            dump({"ok": False, "status": e.status_code, "detail": e.detail})
    elif op == "normalize":
        try:
            norm = ns["_normalize_dashboard_cron_updates"](
                fx.get("updates") or {}, Path(fx["home"]))
            dump({"norm": norm, "status": 0, "detail": None})
        except HTTPException as e:
            dump({"norm": None, "status": e.status_code, "detail": e.detail})
    elif op == "profile_home":
        try:
            name, home = ns["_cron_profile_home"](fx.get("profile"))
            dump({"ok": True, "name": name, "home": str(home),
                  "status": 0, "detail": None})
        except HTTPException as e:
            dump({"ok": False, "name": None, "home": None,
                  "status": e.status_code, "detail": e.detail})
    elif op == "default_profile":
        dump({"profile": ns["_cron_default_profile"]()})
    elif op == "annotate":
        dump(ns["_annotate_cron_job"](fx.get("job") or {}, fx["profile"],
                                      Path(fx["home"])))
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
