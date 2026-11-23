#!/usr/bin/env python3
"""Differential oracle for live gateway/session_api_turn.py (w10_sessionapi_8e7979).

AST-extracts the two pure helpers (the module itself cannot import here) and
drives owns_api_run against an injected run_admission stub.
"""
import ast
import __future__
import hmac
import json
import os
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/gateway/session_api_turn.py"
ANN = __future__.annotations.compiler_flag

tree = ast.parse(open(SRC, encoding="utf-8").read())
mod = types.ModuleType("sapi_8e7979_live")
sys.modules["sapi_8e7979_live"] = mod
ns = mod.__dict__
ns["re"] = re
ns["hmac"] = hmac
ns["_OWNER_SCOPE_RE"] = re.compile(r"[0-9a-f]{64}")
ns["AuthorityRefusal"] = type("AuthorityRefusal", (Exception,), {})
STATE = {"raise": False, "stored": None}


def run_admission(adapter, run_id):
    if STATE["raise"]:
        raise ns["AuthorityRefusal"]("x")
    if STATE["stored"] is None:
        return None
    return ("a", {"payload": {"api_turn_v1": {"run_owner_scope": STATE["stored"]}}})


ns["run_admission"] = run_admission
for parent in ("gateway", "gateway.platforms"):
    if parent not in sys.modules:
        pm = types.ModuleType(parent); pm.__path__ = [os.path.join(Q, *parent.split("."))]; sys.modules[parent] = pm
stub = types.ModuleType("gateway.platforms.api_server_authority_runs")
stub.run_admission = run_admission
sys.modules["gateway.platforms.api_server_authority_runs"] = stub

WANTED = {"_valid_owner_scope", "owns_api_run"}


def ex(node):
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN, dont_inherit=True), ns)


for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        ex(node)
for fn in WANTED:
    if fn not in ns:
        raise SystemExit("ORACLE w10_sessionapi_8e7979: live def %s not found" % fn)


def compute(r):
    f = r.get("func")
    if f == "valid_owner_scope":
        return ns["_valid_owner_scope"](r.get("value"))
    if f == "owns_api_run":
        STATE["raise"] = bool(r.get("run_raise"))
        STATE["stored"] = r.get("stored_scope")
        return ns["owns_api_run"](object(), "run1", r.get("owner_scope", ""))
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s: %r" % (r.get("func"), exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_sessionapi_8e7979: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
