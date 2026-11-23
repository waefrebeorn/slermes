#!/usr/bin/env python3
"""Differential oracle for gateway/session_ingress.py pure helpers.

AST-extracts the LIVE defs (the module cannot import in this env) and stubs the
lazily imported gateway.session_a2a.forward_author seam.  Compares
row_turn_author / pause_notice against the C ports. Fail-closed.
"""
import ast, json, sys, types, __future__

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/session_ingress.py"
ANN = __future__.annotations.compiler_flag

gw = types.ModuleType("gateway"); gw.__path__ = []
a2a = types.ModuleType("gateway.session_a2a")
_HOLDER = {"fallback": None}
a2a.forward_author = lambda policy: _HOLDER["fallback"]
sys.modules["gateway"] = gw
sys.modules["gateway.session_a2a"] = a2a

tree = ast.parse(open(SRC, encoding="utf-8").read())
mod = types.ModuleType("si_w9_live"); sys.modules["si_w9_live"] = mod
ns = mod.__dict__
WANTED = {"row_turn_author", "pause_notice"}
def ex(node):
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                 flags=ANN, dont_inherit=True), ns)
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        ex(node)
for name in WANTED:
    if name not in ns:
        raise SystemExit("ORACLE w9_sessioningr_2d59a9: missing live %s" % name)


def compute(r):
    f = r.get("func")
    if f == "row_turn_author":
        _HOLDER["fallback"] = r.get("fallback_author")
        return ns["row_turn_author"](object(), r.get("row") or {})
    if f == "pause_notified" or f == "pause_notice":
        notified = bool(r.get("pause_notified"))
        slack = bool(r.get("slack"))
        live = types.SimpleNamespace(
            pause_notified=notified,
            source=types.SimpleNamespace(platform=types.SimpleNamespace(value="slack" if slack else "telegram")))
        auth = types.SimpleNamespace(sessions={"S": live})
        ref = types.SimpleNamespace(session_id="S")
        return ns["pause_notice"](auth, ref, r.get("reason"))
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
print(f"ORACLE w9_sessioningr_2d59a9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
