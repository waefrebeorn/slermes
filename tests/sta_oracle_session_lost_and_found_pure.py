#!/usr/bin/env python3
"""sta_oracle_session_lost_and_found_pure.py — oracle twin for session_lost_and_found_pure.c.

Calls the LIVE hermes_cli/session_lost_and_found.py pure helpers and emits one
JSON line per op. t_port_session_lost_and_found_pure.c emits the same shape.

Fixture ops:
  {"op": "is_session_id", "value": "20260812_135332_ab12cd"}
  {"op": "looks_like_source", "value": "telegram"}
  {"op": "classify_row", "nfield": 55, "cells": [id, source, ...]}
  {"op": "heuristic_started_at", "cells": [1234567890, "x"]}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "session_lost_and_found.py")
    # module imports sqlite3/subprocess at load; that's fine in CPython
    spec = importlib.util.spec_from_file_location("live_laf", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op == "is_session_id":
            out.append({"op": op, "out": bool(mod._is_session_id(case.get("value")))})
        elif op == "looks_like_source":
            out.append({"op": op, "out": bool(mod._looks_like_source(case.get("value")))})
        elif op == "classify_row":
            res = mod.classify_lost_and_found_row(int(case.get("nfield", 0)), tuple(case.get("cells", [])))
            out.append({"op": op, "out": res if res is not None else "null"})
        elif op == "heuristic_started_at":
            out.append({"op": op, "out": mod._heuristic_started_at(tuple(case.get("cells", [])))})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
