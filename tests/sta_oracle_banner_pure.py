import os
#!/usr/bin/env python3
"""Oracle for the pure helpers of hermes_cli/banner.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
hermes_cli/banner.py, emits one JSON object per case.
"""
import json
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, os.environ.get("SLERMES_DEV_ROOT", "/home/wubu/hermes-agent") + "")

from hermes_cli import banner as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg", "")
        if op == "ctx_len":
            out = m._format_context_length(int(arg))
            print(json.dumps({"op": "ctx_len", "out": out}))
        elif op == "toolset":
            out = m._display_toolset_name(arg)
            print(json.dumps({"op": "toolset", "arg": arg, "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
