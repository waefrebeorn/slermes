#!/usr/bin/env python3
"""sta_oracle_loops_pure.py — oracle twin for loops_pure.c.

Calls the LIVE hermes_cli/loops.py pure helpers and emits one JSON line
per op. The C harness (t_port_loops_pure.c) emits the same shape; the
runner diffs them.

Fixture shape:
{
  "ops": [
    {"op": "parse_interval", "token": "5m"},
    {"op": "format_interval", "seconds": 90},
    {"op": "response_complete", "text": "..."},
    {"op": "parse_loop_args", "text": "/loop 5m check deploy"},
    {"op": "config_min", "config": null},
    {"op": "config_max", "config": null},
    {"op": "config_floor", "config": null},
    {"op": "config_ceiling", "config": null}
  ]
}
config may be a JSON object {"loops": {...}} or null (defaults).
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "loops.py")
    spec = importlib.util.spec_from_file_location("live_loops", cand)
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
        if op == "parse_interval":
            r = mod.parse_interval_token(case.get("token"))
            out.append({"op": op, "out": (r if r is not None else None)})
        elif op == "format_interval":
            out.append({"op": op, "out": mod.format_interval(case.get("seconds", 0))})
        elif op == "response_complete":
            out.append({"op": op, "out": bool(mod.response_signals_complete(case.get("text", "")))})
        elif op == "parse_loop_args":
            r = mod.parse_loop_args(case.get("text", ""))
            out.append({"op": op, "out": r})
        elif op == "config_min":
            cfg = case.get("config")
            out.append({"op": op, "out": mod.min_interval_seconds(cfg)})
        elif op == "config_max":
            cfg = case.get("config")
            out.append({"op": op, "out": mod.max_ticks_default(cfg)})
        elif op == "config_floor":
            cfg = case.get("config")
            out.append({"op": op, "out": mod.self_paced_floor_seconds(cfg)})
        elif op == "config_ceiling":
            cfg = case.get("config")
            out.append({"op": op, "out": mod.self_paced_ceiling_seconds(cfg)})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
