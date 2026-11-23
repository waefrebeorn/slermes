#!/usr/bin/env python3
"""Oracle for hermes_cli/main_dep_hints.py pure helpers."""
import ast, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/main_dep_hints.py"
source = open(SRC, encoding="utf-8").read()
tree = ast.parse(source)
wanted = ("missing_optional_deps_message", "smart_app_control_block_message")
body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in wanted]
module = ast.fix_missing_locations(ast.Module(
    body=[ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)] + body,
    type_ignores=[]))
ns = {}
exec(compile(module, SRC, "exec"), ns)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "missing_optional_deps_message":
        py = ns["missing_optional_deps_message"](r.get("surface", ""), r.get("what", ""), r.get("extra", ""))
    elif func == "smart_app_control_block_message":
        py = ns["smart_app_control_block_message"](r.get("error"))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_maindephi_e98fd2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
