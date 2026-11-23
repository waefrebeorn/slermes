#!/usr/bin/env python3
"""Differential oracle for gateway/session_handoff.py refusal mappers.

AST-extracts the LIVE defs and the _UNSETTLED/_REFUSALS constant tables from
the quarry source (the module itself cannot import here).  The lazily imported
gateway.handoff_target seam is stubbed with a scripted HandoffTargetError.
"""
import ast, json, sys, types, __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/session_handoff.py"
ANN = __future__.annotations.compiler_flag

gw = types.ModuleType("gateway"); gw.__path__ = []
ht = types.ModuleType("gateway.handoff_target")

class HandoffTargetError(Exception):
    def __init__(self, reason):
        self.reason = reason
        super().__init__(reason)

_HOLDER = {"reason": None}
def resolve_handoff_home(platform):
    raise HandoffTargetError(_HOLDER["reason"])
ht.HandoffTargetError = HandoffTargetError
ht.resolve_handoff_home = resolve_handoff_home
sys.modules["gateway"] = gw
sys.modules["gateway.handoff_target"] = ht

class HandoffRefusal(Exception):
    def __init__(self, reason, detail, field=None):
        self.reason, self.detail, self.field = reason, detail, field
        super().__init__(reason)

tree = ast.parse(open(SRC, encoding="utf-8").read())
mod = types.ModuleType("sh_w9_live"); sys.modules["sh_w9_live"] = mod
ns = mod.__dict__
ns["HandoffRefusal"] = HandoffRefusal
FUNCS = {"_unsettled", "_destination"}
CONSTS = {"_UNSETTLED", "_REFUSALS"}
def ex(n):
    exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec",
                 flags=ANN, dont_inherit=True), ns)
for node in tree.body:
    if isinstance(node, ast.Assign):
        for t in node.targets:
            if isinstance(t, ast.Name) and t.id in CONSTS:
                ex(node)
    if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
        ex(node)
for name in FUNCS | CONSTS:
    if name not in ns:
        raise SystemExit("ORACLE w9_sessionhand_e737d5: missing live %s" % name)


def refusal_dict(e):
    return {"reason": e.reason, "detail": e.detail, "field": e.field}


def compute(r):
    f = r.get("func")
    reason = r.get("reason")
    try:
        if f == "unsettled":
            return refusal_dict(ns["_unsettled"](types.SimpleNamespace(reason=reason)))
        if f == "destination":
            _HOLDER["reason"] = reason
            return refusal_dict(ns["_destination"](r.get("platform", "")))
    except HandoffRefusal as e:
        return refusal_dict(e)
    except KeyError:
        return {"error": "KeyError"}
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_sessionhand_e737d5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
