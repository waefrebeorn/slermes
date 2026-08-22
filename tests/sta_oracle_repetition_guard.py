#!/usr/bin/env python3
"""sta_oracle_repetition_guard.py — Python twin for src/agent/repetition_guard.c.

Imports the LIVE agent/repetition_guard.py and answers the same ops the C
harness answers, so outputs can be diffed byte-for-byte.
"""
import importlib.util
import json
import os
import sys

_LIVE = os.environ.get(
    "HERMES_LIVE_ROOT",
    os.path.join(os.path.expanduser("~"), ".hermes/hermes-agent"),
)
_spec = importlib.util.spec_from_file_location(
    "repetition_guard_live", os.path.join(_LIVE, "agent", "repetition_guard.py")
)
_rg = importlib.util.module_from_spec(_spec)
sys.modules["repetition_guard_live"] = _rg
_spec.loader.exec_module(_rg)


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "dom":
            out = _rg.is_repetition_dominated(c.get("text", ""))
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
