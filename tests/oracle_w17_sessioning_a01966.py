#!/usr/bin/env python3
"""Differential oracle for w17_sessioning_a01966 (gateway/session_ingress_media.py).

AST-extracts the LIVE sniff_image_mime / admission_media_references (the module
cannot import standalone: it pulls the whole persistence stack).
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
SRC = QUARRY + "/gateway/session_ingress_media.py"
NAME = "w17_sessioning_a01966"
ANN = __future__.annotations.compiler_flag

TREE = ast.parse(open(SRC, encoding="utf-8").read())
MOD = types.ModuleType("w17_live_media"); sys.modules["w17_live_media"] = MOD
NS = MOD.__dict__
NS["__builtins__"] = builtins
WANTED = {"sniff_image_mime", "admission_media_references"}
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN, dont_inherit=True), NS)
for name in WANTED:
    if name not in NS:
        raise SystemExit("ORACLE %s: missing live %s" % (NAME, name))


def compute(r):
    f = r.get("func")
    if f == "sniff_image_mime":
        data = bytes.fromhex(r.get("data_hex", ""))
        return NS["sniff_image_mime"](data)
    if f == "admission_media_references":
        return NS["admission_media_references"](r.get("payload") or {})
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
