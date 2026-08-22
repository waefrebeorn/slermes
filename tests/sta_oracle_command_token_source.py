#!/usr/bin/env python3
"""sta_oracle_command_token_source.py — Python twin for
agent/command_token_source.py, mirrored by src/agent/command_token_source.c.

Ops: {"kind":"mint","command":...,"label":...} and
     {"kind":"build_provider","key_cmd":...,"label":...}
Emits one JSON line per op ({"out", "error", "ttl"} / {"out"}).
"""
import json
import sys

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from agent.command_token_source import (
    CommandTokenError,
    _mint,
    build_command_token_provider,
)


def out_line(o):
    print(json.dumps(o, ensure_ascii=False, separators=(",", ":")))


def main():
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        fx = json.load(f)
    for c in fx.get("ops", []):
        kind = c.get("kind", "")
        o = {}
        if kind == "mint":
            try:
                token, ttl = _mint(c.get("command", ""), c.get("label", ""))
                o["out"] = token
                o["error"] = None
                # C harness clamps huge ttls to null; only pass small int ttls
                if ttl is not None and ttl <= 1e9:
                    o["ttl"] = ttl
                else:
                    o["ttl"] = None
            except CommandTokenError as exc:
                o["out"] = None
                o["error"] = str(exc)
                o["ttl"] = None
        elif kind == "build_provider":
            p = build_command_token_provider(
                c.get("key_cmd", ""), c.get("label", ""))
            if p is None:
                o["out"] = None
            else:
                o["out"] = p._command
        out_line(o)


if __name__ == "__main__":
    main()
