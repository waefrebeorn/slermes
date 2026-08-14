#!/usr/bin/env python3
"""Oracle for the pure plugin helpers of hermes_cli/plugins.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
hermes_cli/plugins.py, emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from hermes_cli import plugins as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg")
        if op == "namespace":
            out = m._portable_skill_namespace(arg)
            print(json.dumps({"op": "namespace", "arg": arg, "out": out}))
        elif op == "display_author":
            # Python takes a Mapping or a scalar. Pass a dict when the arg
            # looks like a JSON object, else the scalar string.
            if isinstance(arg, str) and arg.lstrip().startswith("{"):
                import json as _j
                try:
                    arg = _j.loads(arg)
                except Exception:
                    pass
            out = m._display_author(arg)
            print(json.dumps({"op": "display_author", "arg": c.get("arg"),
                              "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
