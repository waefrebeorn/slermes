#!/usr/bin/env python3
"""Oracle for the pure trust/identity-header helpers of tools/mcp_tool.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
tools/mcp_tool.py, emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from tools.mcp_tool import (
    _normalize_server_trust,
    _annotation_read_only_hint,
    _resolve_identity_header,
    _apply_identity_header,
)


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        if op == "normalize_trust":
            arg = c.get("arg")
            # mimic the Python dispatch: None, str, dict
            raw = arg
            if raw is not None and not isinstance(raw, str):
                raw = str(raw)
            out = _normalize_server_trust(raw)
            print(json.dumps({"op": "normalize_trust", "out": out}))
        elif op == "read_only_hint":
            tool = c.get("arg")
            # Python uses getattr(mcp_tool, "annotations", None) — for dicts
            # that returns None, so wrap dicts in a namespace that exposes
            # annotations as an attribute.
            class _Obj:
                def __init__(self, d):
                    self.annotations = d.get("annotations")
            out = bool(_annotation_read_only_hint(_Obj(tool) if isinstance(tool, dict) else tool))
            print(json.dumps({"op": "read_only_hint", "out": out}))
        elif op == "resolve_identity":
            out = _resolve_identity_header(c.get("server", ""), c.get("cfg", {}))
            print(json.dumps({"op": "resolve_identity", "out": out}))
        elif op == "apply_identity":
            hdrs = dict(c.get("headers", {}))
            out = _apply_identity_header(c.get("server", ""), c.get("cfg", {}), hdrs)
            print(json.dumps({"op": "apply_identity", "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
