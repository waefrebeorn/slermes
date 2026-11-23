#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_costs.py (gate w7_mcptoolcos_c8e8a5)."""
import importlib.util
import json
import sys

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import tools  # noqa: F401  (parent-package bootstrap)
spec = importlib.util.spec_from_file_location("tools.mcp_tool_costs", Q + "/tools/mcp_tool_costs.py")
live = importlib.util.module_from_spec(spec)
sys.modules["tools.mcp_tool_costs"] = live
spec.loader.exec_module(live)


def render_cost(c):
    return {"low": c.low, "high": c.high, "note": c.note}


def render_policy(p):
    if p is None:
        return None
    return {"costs": {k: render_cost(v) for k, v in p.costs.items()},
            "limit": p.limit, "unit": p.unit}


def make_charge(r):
    cost = live.ToolCost(r.get("low", 0), r.get("high", 0), r.get("note", ""))
    policy = live.CostPolicy({"t": cost}, r.get("limit", 0), r.get("unit", "credits"))
    return live.Charge(("k", "s"), r.get("server", ""), r.get("tool", "tool"), cost, policy)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "fmt":
            py = live._fmt(r.get("amount", 0))
        elif func == "qty":
            py = live._qty(r.get("amount", 0), r.get("unit", "credits"))
        elif func == "parse_cost":
            try:
                py = render_cost(live._parse_cost(r.get("value"), r.get("where", "srv")))
            except Exception:
                py = {"error": True}
        elif func == "parse_cost_policy":
            try:
                py = render_policy(live.parse_cost_policy(r.get("costs"), r.get("spend_limit"),
                                                          r.get("cost_unit"), where=r.get("where", "srv")))
            except Exception:
                py = {"error": True}
        elif func == "unknown_cost_keys":
            cm = r.get("costs")
            policy = None if cm is None else live.CostPolicy(
                {k: live.ToolCost(**v) for k, v in cm.items()}, 0.0, "credits")
            py = live.unknown_cost_keys(policy, r.get("tool_names", []))
        elif func == "charge_sentence":
            py = make_charge(r).cost_sentence()
        elif func == "spend_sentence":
            py = make_charge(r).spend_sentence(r.get("spent", 0))
        elif func == "charge_lines":
            py = make_charge(r).lines(r.get("spent", 0))
        elif func == "over_limit":
            py = make_charge(r).over_limit(r.get("spent", 0))
        elif func == "charge_refusal":
            py = make_charge(r).refusal(r.get("spent", 0))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_mcptoolcos_c8e8a5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
