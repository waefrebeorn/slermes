#!/usr/bin/env python3
"""Oracle for hermes_cli/model_cost_guard.py pure helpers.

Harness families (t_port_model_cost_guard.c), emitted per line:
  {fn:"pricing", case, in:<model_info>, present, in, out, source}
  {fn:"money",   case, in:[cost_in,cost_out|null], out:["$X.YY/M",...]}

Replays the LIVE funcs on the decoded inputs. Ast-extracts `_format_money`,
`_to_decimal`, and `_pricing_from_model_info` from the module (skip the heavy
ModelInfo import chain). The pricing fn duck-types model_info for
`.has_cost_data()` / `.cost_input` / `.cost_output`. C present == has_cost_data.
Numbers compared numerically (libjson renders integral doubles like 20.0 as 20 —
same value); money outputs are strings compared exactly.
"""
import ast
import json
import sys
from decimal import Decimal, InvalidOperation
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/model_cost_guard.py"
FUNS = ("_format_money", "_to_decimal", "_pricing_from_model_info")


def _canon(x):
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, list):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    if x is None:
        return ("null",)
    return ("other", repr(x))


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Decimal": Decimal, "InvalidOperation": InvalidOperation,
          "Optional": None}
    body = []
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in FUNS:
            body.append(n)
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    format_money = ns["_format_money"]
    pricing = ns["_pricing_from_model_info"]

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE model_cost_guard: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    total = 0
    for c in cases:
        total += 1
        fn = c.get("fn")
        name = c.get("case")
        want = None
        if fn == "pricing":
            mi_raw = c.get("input")
            mi = None
            if isinstance(mi_raw, dict):
                mi = SimpleNamespace(
                    cost_input=mi_raw.get("cost_input", 0.0) or 0.0,
                    cost_output=mi_raw.get("cost_output", 0.0) or 0.0,
                )
                mi.has_cost_data = lambda: (mi.cost_input > 0 or mi.cost_output > 0)
            in_d, out_d, source = pricing(mi)
            present = in_d is not None or out_d is not None
            want = {
                "present": 1 if present else 0,
                "in": float(in_d) if in_d is not None else 0.0,
                "out": float(out_d) if out_d is not None else 0.0,
                "source": source,
            }
            got = {
                "present": c.get("present"),
                "in": c.get("in"),
                "out": c.get("out"),
                "source": c.get("source"),
            }
        elif fn == "money":
            got = c.get("out")
            vals = c.get("in") or []
            want = []
            for v in vals:
                want.append(format_money(None if v is None else Decimal(str(v))))
        else:
            mism += 1
            print(f"MISMATCH {name}: unknown fn {fn}")
            continue
        if _canon(want) != _canon(got):
            mism += 1
            print(f"MISMATCH {name}\n  C=  {json.dumps(got, sort_keys=True)}\n  PY= {json.dumps(want, sort_keys=True)}")
    print(f"ORACLE model_cost_guard: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
