#!/usr/bin/env python3
"""Oracle for the pure render/path helpers of hermes_cli/linux_desktop_entry.py
ported to C (port_linux_desktop_entry.c).

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
hermes_cli/linux_desktop_entry.py, and emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from hermes_cli import linux_desktop_entry as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg", "")
        if op == "is_supported":
            print(json.dumps({"op": "is_supported", "out": bool(m.is_supported())}))
        elif op == "xdg_data_home":
            out = str(m._xdg_data_home())
            print(json.dumps({"op": "xdg_data_home", "out": out}))
        elif op == "desktop_entry_path":
            out = str(m.desktop_entry_path())
            print(json.dumps({"op": "desktop_entry_path", "out": out}))
        elif op == "icon_path":
            from pathlib import Path
            out = str(m.icon_path(Path(arg) if arg else Path("/")))
            print(json.dumps({"op": "icon_path", "out": out}))
        elif op == "quote":
            out = m._quote_exec_arg(arg)
            print(json.dumps({"op": "quote", "arg": arg, "out": out}))
        elif op == "render":
            cfg = json.loads(arg) if isinstance(arg, str) and arg else {}
            out = m.render_desktop_entry(cfg.get("exec", ""), cfg.get("icon", ""))
            print(json.dumps({"op": "render", "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
