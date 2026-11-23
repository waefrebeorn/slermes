#!/usr/bin/env python3
"""Differential oracle for agent/interrupt_control.py (gate w9_interruptco_7c8a77).

AST-extracts the live pure reason helpers plus steer() (with lock/queue stubs)
so no heavy agent deps are imported. Compares harness "out" to live results.
"""
import ast
import contextlib
import json
import sys
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/interrupt_control.py"
KEEP_FUNCS = {"interrupt_issuer", "interrupt_skip_wording", "interrupted_during_api_call_reason"}
KEEP_CONSTS = {"_REASON_HARD_STOP", "_REASON_NEW_MESSAGE", "_REASON_USER_INTERRUPT",
               "USER_INTERRUPT_REASONS"}

ns = {}
tree = ast.parse(open(SRC, encoding="utf-8").read())
nodes = []
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in KEEP_FUNCS:
        nodes.append(n)
    elif isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) \
            and n.targets[0].id in KEEP_CONSTS:
        nodes.append(n)
    elif isinstance(n, ast.ClassDef) and n.name == "InterruptControlMixin":
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name == "steer":
                nodes.append(m)
exec(compile(ast.fix_missing_locations(ast.Module(body=nodes, type_ignores=[])), SRC, "exec"), ns)

ns["_ic_lock"] = lambda self, attr: contextlib.nullcontext()
ns["_steer_parts"] = lambda self: []
ns["_set_steer_parts"] = lambda self, parts: setattr(self, "_stored_parts", parts)


def compute(r):
    func = r.get("func")
    reason = r.get("reason")
    agent = SimpleNamespace(_tool_interrupt_reason=reason)
    if func == "interrupt_issuer":
        return ns["interrupt_issuer"](agent)
    if func == "interrupt_skip_wording":
        return ns["interrupt_skip_wording"](agent)
    if func == "interrupted_during_api_call_reason":
        return ns["interrupted_during_api_call_reason"](agent)
    if func == "steer":
        self = SimpleNamespace()
        accepted = ns["steer"](self, r.get("text"))
        parts = getattr(self, "_stored_parts", [])
        cleaned = parts[0][1] if parts else None
        return {"accepted": bool(accepted), "cleaned": cleaned}
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
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w9_interruptco_7c8a77: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w9_interruptco_7c8a77: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
