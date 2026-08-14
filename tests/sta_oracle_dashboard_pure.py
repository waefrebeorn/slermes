#!/usr/bin/env python3
"""Oracle for the pure helpers of hermes_cli/dashboard_procs.py ported to C."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from hermes_cli.dashboard_procs import (
    _is_desktop_local_serve_cmdline,
    _hermes_home_dir,
    _valid_lockfile_payload,
)


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        if op == "is_desktop_serve":
            out = bool(_is_desktop_local_serve_cmdline(c.get("cmd", "")))
            print(json.dumps({"op": "is_desktop_serve", "out": out}))
        elif op == "hermes_home":
            out = str(_hermes_home_dir())
            print(json.dumps({"op": "hermes_home", "out": out}))
        elif op == "valid_lockfile":
            raw = c.get("json", "{}")
            parsed = json.loads(raw) if isinstance(raw, str) else raw
            ownership = c.get("ownership", "")
            out = bool(_valid_lockfile_payload(parsed, ownership))
            print(json.dumps({"op": "valid_lockfile", "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
