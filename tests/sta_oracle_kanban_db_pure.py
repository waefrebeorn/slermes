#!/usr/bin/env python3
"""sta_oracle_kanban_db_pure.py — oracle twin for kanban_db_pure.c.

Calls the LIVE hermes_cli/kanban_db.py pure helpers and emits one JSON line
per op. The C harness (t_port_kanban_db_pure.c) emits the same shape.

Fixture shape:
{
  "ops": [
    {"op": "normalize_effort", "effort": "High"},
    {"op": "claim_ttl", "ttl": null},
    {"op": "crash_grace", "env": null},
    {"op": "rate_cooldown", "env": null},
    {"op": "relative_age", "ts": 1000, "now": 1000},
    {"op": "normalize_slug", "slug": "My Board"},
    {"op": "display_name", "slug": "atm10-server"}
  ]
}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "kanban_db.py")
    spec = importlib.util.spec_from_file_location("live_kanban_db", cand)
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
        if op == "normalize_effort":
            try:
                r = mod.normalize_reasoning_effort(case.get("effort"))
                out.append({"op": op, "out": r})
            except Exception as e:
                out.append({"op": op, "out": "__ERR__: " + str(e)})
        elif op == "claim_ttl":
            ttl = case.get("ttl")
            out.append({"op": op, "out": mod._resolve_claim_ttl_seconds(ttl)})
        elif op == "crash_grace":
            env = case.get("env")
            out.append({"op": op, "out": mod._resolve_crash_grace_seconds(env)})
        elif op == "rate_cooldown":
            env = case.get("env")
            out.append({"op": op, "out": mod._resolve_rate_limit_cooldown_seconds(env)})
        elif op == "relative_age":
            out.append({"op": op, "out": mod._relative_age(case.get("ts"), case.get("now"))})
        elif op == "normalize_slug":
            try:
                r = mod._normalize_board_slug(case.get("slug"))
                out.append({"op": op, "out": r})
            except Exception as e:
                out.append({"op": op, "out": "__ERR__: " + str(e)})
        elif op == "display_name":
            out.append({"op": op, "out": mod._default_board_display_name(case.get("slug", ""))})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
