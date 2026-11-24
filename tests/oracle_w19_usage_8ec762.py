#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/usage.py (gate w19_usage_8ec762).

The repository class is a DB wrapper; the pure helper ``_delta_kwargs`` is extracted
from the live AST and exercised against duck-typed stand-ins built from the fixture.
"""
import ast, json, sys, types, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "sqlite" / "conversation" / "usage.py"
GATE = "w19_usage_8ec762"
_ANN = __future__.annotations.compiler_flag

TOK = ("input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens", "reasoning_tokens")
DELTA = ("source", "estimated_cost_usd", "actual_cost_usd", "cost_status", "cost_source",
         "pricing_version", "api_call_count", "task", "prompt_cache_clock")
ROUTE = ("model", "billing_provider", "billing_base_url", "billing_mode")


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "usage_live", "Any": object}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_delta_kwargs":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def build(r):
    """A UsageDelta / TokenCounts / BillingRoute stand-in with real attribute names."""
    raw = r.get("delta") or {}
    delta = types.SimpleNamespace()
    for k in DELTA:
        setattr(delta, k, raw.get(k))
    tok = types.SimpleNamespace()
    toks = raw.get("tokens") or {}
    for k in TOK:
        setattr(tok, k, toks.get(k, 0))
    delta.tokens = tok
    route = raw.get("route")
    if route is None or "route" not in raw:
        delta.route = None
    else:
        ro = types.SimpleNamespace()
        for k in ROUTE:
            setattr(ro, k, route.get(k))
        delta.route = ro
    return delta


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "delta_kwargs":
        continue
    cases += 1
    try:
        py = G["_delta_kwargs"](build(r))
    except BaseException as exc:
        mism += 1
        print("MISMATCH _delta_kwargs oracle raised %r args=%s" % (exc, r), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH _delta_kwargs py=%r c=%r" % (py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
