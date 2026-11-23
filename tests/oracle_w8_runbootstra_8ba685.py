#!/usr/bin/env python3
"""Differential oracle for live Hermes gateway/run_bootstrap.py (port w8_runbootstra_8ba685).

The identify handler is a nested function; it is AST-extracted from the LIVE
source and executed with injected base payload / descriptor / runner seams.
"""
import ast
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
import gateway  # noqa: F401  parent-package bootstrap

SRC = QUARRY + "/gateway/run_bootstrap.py"
src = open(SRC, encoding="utf-8").read()
tree = ast.parse(src)
node = None
for top in tree.body:
    if isinstance(top, (ast.FunctionDef, ast.AsyncFunctionDef)) and top.name == "_start_gateway_start_control_socket":
        for cand in ast.walk(top):
            if isinstance(cand, ast.FunctionDef) and cand.name == "_identify_runtime":
                node = cand
                break
if node is None:
    raise RuntimeError("live _identify_runtime not found")
node.decorator_list = []
node.body = [b for b in node.body if not isinstance(b, (ast.Import, ast.ImportFrom))]
NS = {"getattr": getattr}
exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), NS)

NAME = "run_bootstrap_live_w8"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)
import persistence.core.admin as adm


def compute(r):
    func = r.get("func")
    if func == "identify_payload":
        base = copy.deepcopy(r.get("base", {}))
        desc = copy.deepcopy(r.get("descriptor", {}))
        NS["build_identify_payload"] = lambda: copy.deepcopy(base)
        NS["RESTART_WAIT_REASON"] = r.get("restart_wait_reason", "restart_wait")
        NS["descriptor"] = desc
        NS["runner"] = types.SimpleNamespace(
            _draining=bool(r.get("draining", False)), session_runtime_descriptor=desc)
        return NS["_identify_runtime"]()
    if func == "browse_reconcile_reserve_wait_s":
        old = adm.BROWSE_RECONCILE_BUDGET_S
        adm.BROWSE_RECONCILE_BUDGET_S = r.get("budget", 0.0)
        try:
            return live._browse_reconcile_reserve_wait_s()
        finally:
            adm.BROWSE_RECONCILE_BUDGET_S = old
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_runbootstra_8ba685: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
