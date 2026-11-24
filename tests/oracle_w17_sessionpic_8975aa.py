#!/usr/bin/env python3
"""Differential oracle for w17_sessionpic_8975aa (gateway/session_picker_fields.py).

AST-extracts the LIVE live_picker_page into a registered module namespace and
drives it with scripted store seams: workspace_members returns the fixture's
member set; current_segments/fill_picker_fields are deterministic no-ops, so the
returned page reflects the real filter + slice logic of the live function.
"""
import ast, builtins, json, sys, types, __future__
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/session_picker_fields.py"
NAME = "w17_sessionpic_8975aa"
ANN = __future__.annotations.compiler_flag

TREE = ast.parse(open(SRC, encoding="utf-8").read())
MOD = types.ModuleType("w17_live_picker"); sys.modules["w17_live_picker"] = MOD
NS = MOD.__dict__
NS["__builtins__"] = builtins
WANTED = {"live_picker_page"}
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN, dont_inherit=True), NS)
if "live_picker_page" not in NS:
    raise SystemExit("ORACLE %s: missing live live_picker_page" % NAME)

_CTX = {"members": set()}

def _members(store, ids, workspace):
    return set(_CTX["members"])

NS["authority_persistence"] = lambda authority: object()
NS["workspace_members"] = _members
NS["current_segments"] = lambda authority, ids: {}
NS["fill_picker_fields"] = lambda store, items: None


def compute(r):
    _CTX["members"] = set(r.get("members") or [])
    return NS["live_picker_page"](
        object(),
        r.get("sessions") or [],
        r.get("limit", 0),
        workspace=(r.get("workspace") if r.get("has_workspace") else None),
        picker=bool(r.get("picker", False)))


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
        py = {"__raise__": type(ex).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (NAME, cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
