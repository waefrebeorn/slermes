#!/usr/bin/env python3
"""
sta_oracle_web_server_managed.py — LIVE-Python oracle for the managed-files
security cluster. Extracts the real function sources + constants from
hermes_cli/web_server.py and execs them (module import is FastAPI-heavy),
then answers the same fixture ops as the C harness.
"""
import ast
import base64
import json
import re as _re
import sys
from pathlib import Path

WEB_SERVER = Path("/home/wubu/hermes-agent-dev/hermes_cli/web_server.py")

WANT_FUNCS = {
    "_is_sensitive_filename",
    "_is_sensitive_path",
    "_chat_image_extension",
    "_sanitize_chat_image_filename",
}
WANT_CONSTS = {
    "_SENSITIVE_MANAGED_FILE_BASENAMES",
    "_SENSITIVE_MANAGED_DIR_NAMES",
    "_CHAT_IMAGE_MAGIC",
    "_CHAT_IMAGE_ALLOWED_EXTENSIONS",
}


def load_live():
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
        elif isinstance(node, ast.AnnAssign):
            if isinstance(node.target, ast.Name) and node.target.id in WANT_CONSTS:
                pieces.append(ast.get_source_segment(src, node))
    ns = {"Path": Path, "re": _re, "frozenset": frozenset}
    exec("\n\n".join(pieces), ns)  # live code, no mocks
    missing = (WANT_FUNCS | WANT_CONSTS) - set(ns)
    if missing:
        raise SystemExit(f"oracle failed to extract: {missing}")
    return ns


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    ns = load_live()
    op = fx["op"]
    if op == "sensitive_filename":
        print(json.dumps({"sensitive": ns["_is_sensitive_filename"](fx["name"])},
                         separators=(",", ":")))
    elif op == "sensitive_path":
        print(json.dumps({"sensitive": ns["_is_sensitive_path"](Path(fx["path"]))},
                         separators=(",", ":")))
    elif op == "chat_image_ext":
        data = base64.b64decode(fx["b64"]) if fx.get("b64") else b""
        ext = ns["_chat_image_extension"](data)
        allowed = ext in ns["_CHAT_IMAGE_ALLOWED_EXTENSIONS"] if ext else False
        print(json.dumps({"ext": ext, "allowed": bool(allowed)},
                         separators=(",", ":")))
    elif op == "sanitize_filename":
        print(json.dumps({"name": ns["_sanitize_chat_image_filename"](fx["filename"])},
                         separators=(",", ":")))
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
