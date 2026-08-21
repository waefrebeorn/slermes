#!/usr/bin/env python3
"""sta_oracle_foreign_sessions_parse.py — oracle twin for the session parsers
in src/hermes_cli/foreign_sessions_pure.c
(hermes_cli/foreign_sessions.py: parse_claude_session, parse_codex_session).

Mirrors t_port_foreign_sessions_parse.c using the REAL Python functions.
"""
import json
import os
import sys
import importlib.util


def load_module():
    spec = importlib.util.spec_from_file_location(
        "foreign_sessions",
        os.path.join(os.path.dirname(__file__), "..", "..", "hermes_cli",
                     "foreign_sessions.py"),
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    mod = load_module()
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        op = c.get("op")
        file = c.get("file")
        if op == "claude":
            res = mod.parse_claude_session(file)
        elif op == "codex":
            res = mod.parse_codex_session(file)
        else:
            continue
        print(json.dumps({"op": op, "file": file, "out": res}))


if __name__ == "__main__":
    main()
