#!/usr/bin/env python3
"""sta_oracle_goals_pure.py — oracle twin for goals_pure.c.

Calls the LIVE hermes_cli/goals.py pure helpers and emits one JSON line
per op. The C harness (t_port_goals_pure.c) emits the same shape; the
runner diffs them.

Fixture shape:
{
  "ops": [
    {"op": "parse_contract", "text": "..."},
    {"op": "contract_is_empty", "contract": {...} | null},
    {"op": "contract_to_dict", "contract": {...}},
    {"op": "contract_from_dict", "contract": {...} | null},
    {"op": "contract_render", "contract": {...}},
    {"op": "gate_to_dict", "gate": {...}},
    {"op": "gate_from_dict", "gate": {...} | null}
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
    cand = os.path.join(repo, "hermes_cli", "goals.py")
    spec = importlib.util.spec_from_file_location("live_goals", cand)
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
        if op == "parse_contract":
            headline, contract = mod.parse_contract(case.get("text", ""))
            out.append({"op": op, "out": {"headline": headline, "contract": contract.to_dict()}})
        elif op == "contract_is_empty":
            d = case.get("contract")
            c = mod.GoalContract.from_dict(d) if d is not None else mod.GoalContract()
            out.append({"op": op, "out": bool(c.is_empty())})
        elif op == "contract_to_dict":
            c = mod.GoalContract.from_dict(case.get("contract"))
            out.append({"op": op, "out": c.to_dict()})
        elif op == "contract_from_dict":
            c = mod.GoalContract.from_dict(case.get("contract"))
            out.append({"op": op, "out": c.to_dict()})
        elif op == "contract_render":
            c = mod.GoalContract.from_dict(case.get("contract"))
            out.append({"op": op, "out": c.render_block()})
        elif op == "gate_to_dict":
            g = mod.GoalGate(**(case.get("gate") or {"command": ""}))
            out.append({"op": op, "out": g.to_dict()})
        elif op == "gate_from_dict":
            g = mod.GoalGate.from_dict(case.get("gate"))
            out.append({"op": op, "out": g.to_dict()})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
