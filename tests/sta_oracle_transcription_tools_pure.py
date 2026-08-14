#!/usr/bin/env python3
"""Oracle for the config-parse helpers of tools/transcription_tools.py ported
to C (transcription_tools_pure.c).

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
tools/transcription_tools.py, and emits one JSON object per case. The C harness
emits the same shape; the runner exact-compares.
"""
import json
import os
import sys

# Quarry root (parent of slermes) so `import tools.transcription_tools` works.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# The fixture's @SBX@/@NOW@ placeholders and isolated HOME shouldn't matter for
# these pure config-parsers, but keep the module importable from cwd too.
sys.path.insert(0, os.getcwd())

from tools import transcription_tools as t  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg", {})
        cfg = json.loads(arg) if isinstance(arg, str) and arg else {}
        if op == "idle_unload":
            out = t._get_idle_unload_seconds(cfg)
            print(json.dumps({"op": "idle_unload", "out": out}))
        elif op == "cloud_trim":
            enabled, tdb, keep = t._cloud_trim_settings(cfg)
            print(json.dumps({"op": "cloud_trim",
                              "enabled": bool(enabled),
                              "threshold_db": tdb,
                              "keep_ms": keep}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
