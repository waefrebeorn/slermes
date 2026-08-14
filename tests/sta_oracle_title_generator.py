#!/usr/bin/env python3
"""Oracle for the pure title helpers of agent/title_generator.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
agent/title_generator.py, emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from agent import title_generator as t  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg")
        if op == "strip_wrappers":
            out = t.strip_control_wrappers(arg if isinstance(arg, str) else "")
            print(json.dumps({"op": "strip_wrappers", "out": out}))
        elif op == "clean_title":
            out = t._clean_title(arg if isinstance(arg, str) else "")
            print(json.dumps({"op": "clean_title", "out": out}))
        elif op == "extract_title":
            out = t._extract_title_text(arg if isinstance(arg, str) else "")
            print(json.dumps({"op": "extract_title", "out": out}))
        elif op == "is_titleable":
            out = bool(t.is_titleable_user_message(arg if isinstance(arg, str) else ""))
            print(json.dumps({"op": "is_titleable", "out": out}))
        elif op == "derive_title":
            out = t.derive_title(arg if isinstance(arg, str) else "")
            print(json.dumps({"op": "derive_title", "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
