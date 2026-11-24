#!/usr/bin/env python3
"""Differential oracle for w17_slashcomma_7fa9bc (gateway/slash_commands_status.py).

AST-extracts the LIVE pure helpers into a registered namespace and drives them
directly (they have no non-builtin dependencies).
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
SRC = QUARRY + "/gateway/slash_commands_status.py"
NAME = "w17_slashcomma_7fa9bc"
ANN = __future__.annotations.compiler_flag

TREE = ast.parse(open(SRC, encoding="utf-8").read())
MOD = types.ModuleType("w17_live_status"); sys.modules["w17_live_status"] = MOD
NS = MOD.__dict__
NS["__builtins__"] = builtins
WANTED = {"_clean_str", "_int_value", "_fmt", "_pct", "_clip", "_routes_for_default"}
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN, dont_inherit=True), NS)
for name in WANTED:
    if name not in NS:
        raise SystemExit("ORACLE %s: missing live %s" % (NAME, name))


def compute(r):
    f = r.get("func")
    if f == "clean_str":
        return NS["_clean_str"](r.get("value"))
    if f == "int_value":
        return NS["_int_value"](r.get("value"))
    if f == "fmt":
        return NS["_fmt"](r.get("n"))
    if f == "pct":
        return NS["_pct"](r.get("used", 0), r.get("total", 0))
    if f == "clip":
        return NS["_clip"](r.get("text", ""), r.get("limit", 0))
    if f == "routes_for_default":
        return list(NS["_routes_for_default"](
            r.get("default_route") or [], r.get("persisted_route") or {},
            r.get("session_row") or {}))
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
        py = {"__raise__": type(ex).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (NAME, cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
