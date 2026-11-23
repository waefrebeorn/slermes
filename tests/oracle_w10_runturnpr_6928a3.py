#!/usr/bin/env python3
"""Differential oracle for live gateway/run_turn_prepare.py (w10_runturnpr_6928a3).

AST-extracts the two pure helpers and drives them with fixture-injected state.
"""
import ast
import __future__
import json
import logging
import sys
import types
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/gateway/run_turn_prepare.py"
ANN = __future__.annotations.compiler_flag

tree = ast.parse(open(SRC, encoding="utf-8").read())
mod = types.ModuleType("rtpr_6928a3_live")
sys.modules["rtpr_6928a3_live"] = mod
ns = mod.__dict__
ns["logger"] = logging.getLogger("rtpr_oracle")
ns["Any"] = object
WANTED = {"_session_override_snapshot", "_recover_or_remember_model"}


def ex(node):
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN, dont_inherit=True), ns)


for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        ex(node)
for fn in WANTED:
    if fn not in ns:
        raise SystemExit("ORACLE w10_runturnpr_6928a3: live def %s not found" % fn)


def compute(r):
    f = r.get("func")
    if f == "session_override_snapshot":
        return ns["_session_override_snapshot"](r.get("override") or {})
    if f == "recover_or_remember_model":
        skey = r.get("skey") or ""
        model = r.get("model")
        state_last = r.get("state_last")
        star_last = r.get("star_last")
        def peek(k):
            val = state_last if (k == skey and skey) else (star_last if k == "*" else None)
            if val is None:
                return None
            return NS(conversation=NS(last_resolved_model=val))
        runner = NS(_peek_session_state=peek, _session_state=peek)
        return ns["_recover_or_remember_model"](runner, skey, model)
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
print("ORACLE w10_runturnpr_6928a3: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
