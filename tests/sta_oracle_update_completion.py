#!/usr/bin/env python3
"""Oracle for _print_update_completion ported to C."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from hermes_cli.update_cmd import _print_update_completion


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        msg = c.get("message", "")
        # _print_update_completion prints to stdout; capture it
        import io
        from contextlib import redirect_stdout
        buf = io.StringIO()
        with redirect_stdout(buf):
            _print_update_completion(msg)
        out = buf.getvalue().rstrip("\n")
        print(json.dumps({"out": out}))


if __name__ == "__main__":
    main()
