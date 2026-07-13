#!/usr/bin/env python3
"""Oracle fixtures + reference for hermes_cli/model_cost_guard.py helpers.
Exercises _format_money and _pricing_from_model_info across edge cases."""
import json, os, sys, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, "/home/wubu/hermes-agent-dev")

from hermes_cli.model_cost_guard import _format_money, _pricing_from_model_info
from decimal import Decimal

# _format_money cases
fmt_cases = [
    None,
    Decimal("0"),
    Decimal("20"),
    Decimal("20.5"),
    Decimal("100"),
    Decimal("1234.567"),
]
ref_fmt = {("None" if c is None else str(c)): _format_money(c) for c in fmt_cases}

# _pricing_from_model_info cases: model info dicts (flat cost_input/cost_output)
pricing_cases = {
    "none": None,
    "empty": {},
    "no_cost": {"name": "x"},
    "zero_cost": {"cost_input": 0.0, "cost_output": 0.0},
    "input_only": {"cost_input": 20.0, "cost_output": 0.0},
    "both": {"cost_input": 20.0, "cost_output": 100.0},
}
ref_pricing = {}
for name, m in pricing_cases.items():
    # ModelInfo replacement: the Python helper accepts a ModelInfo dataclass;
    # we emulate has_cost_data() via the same flat fields the C side uses.
    class FakeMI:
        def __init__(self, d):
            self._d = d or {}
            self.cost_input = self._d.get("cost_input", 0.0)
            self.cost_output = self._d.get("cost_output", 0.0)
        def has_cost_data(self):
            return (self.cost_input or 0) > 0 or (self.cost_output or 0) > 0
    mi = FakeMI(m)
    inp, out, src = _pricing_from_model_info(mi if m is not None else None)
    ref_pricing[name] = [None if inp is None else float(inp),
                         None if out is None else float(out),
                         src]

with open(os.path.join(HERE, "model_cost_guard.ref.json"), "w") as f:
    json.dump({"format_money": ref_fmt, "pricing": ref_pricing}, f, indent=2)
print("model_cost_guard oracle reference written")
print("format_money:", ref_fmt)
print("pricing:", ref_pricing)
