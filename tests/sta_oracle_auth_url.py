#!/usr/bin/env python3
"""Oracle for the Actual base-URL helpers of hermes_cli/auth.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
hermes_cli/auth.py, emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from hermes_cli import auth as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg")
        if op == "is_local":
            out = bool(m.is_actual_local_base_url(arg if isinstance(arg, str) else ""))
            print(json.dumps({"op": "is_local", "arg": arg, "out": out}))
        elif op == "normalize":
            out = m.normalize_actual_base_url(arg if isinstance(arg, str) else "")
            print(json.dumps({"op": "normalize", "arg": arg, "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
