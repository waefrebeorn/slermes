#!/usr/bin/env python3
"""Oracle for _format_update_notice ported to C."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from hermes_cli.banner import _format_update_notice


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        out = _format_update_notice(int(c.get("behind", 0)))
        print(json.dumps({"out": out}))


if __name__ == "__main__":
    main()
