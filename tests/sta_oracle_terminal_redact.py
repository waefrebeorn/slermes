#!/usr/bin/env python3
"""Oracle for _redact_terminal_error_text ported to C."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from tools.terminal_tool import _redact_terminal_error_text


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        arg = c.get("arg")
        out = _redact_terminal_error_text(arg if arg is not None else "")
        print(json.dumps({"out": out}))


if __name__ == "__main__":
    main()
