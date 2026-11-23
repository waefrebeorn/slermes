#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn_runner.py pure helpers.

AST-extracts the LIVE defs (the module itself cannot import in this env) and
compares _observed_turn_persist_message / _merge_turn_request_overrides against
the C ports. Fail-closed.
"""
import ast, copy, json, sys, types, __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/run_turn_runner.py"
ANN = __future__.annotations.compiler_flag

tree = ast.parse(open(SRC, encoding="utf-8").read())
mod = types.ModuleType("rtr_w9_live"); sys.modules["rtr_w9_live"] = mod
ns = mod.__dict__
ns["Any"] = object
WANTED = {"_observed_turn_persist_message", "_merge_turn_request_overrides"}

def ex(node):
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                 flags=ANN, dont_inherit=True), ns)

for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        ex(node)
    elif isinstance(node, ast.ClassDef):
        for sub in node.body:
            if isinstance(sub, ast.FunctionDef) and sub.name in WANTED:
                sub.decorator_list = []
                ex(sub)
for fn in WANTED:
    if fn not in ns:
        raise SystemExit("ORACLE w9_runturnrun_42e645: live def %s not found" % fn)


def compute(r):
    func = r.get("func")
    if func == "observed_persist":
        return ns["_observed_turn_persist_message"](
            r.get("run_message"), r.get("override"), r.get("live_text"))
    if func == "merge_overrides":
        agent = types.SimpleNamespace(
            request_overrides=copy.deepcopy(r.get("agent_overrides")),
            _gateway_turn_request_overrides=copy.deepcopy(r.get("prev_overrides")))
        ns["_merge_turn_request_overrides"](agent,
            {"request_overrides": r.get("turn_overrides")})
        return {"overrides": agent.request_overrides,
                "prev": agent._gateway_turn_request_overrides}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except BaseException as ex:
        py = {"__raise__": type(ex).__name__, "message": str(ex)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_runturnrun_42e645: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
