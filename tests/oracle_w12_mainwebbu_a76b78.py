#!/usr/bin/env python3
"""Oracle for hermes_cli/main_web_build.py pure path helpers."""
import ast, json, sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/main_web_build.py"
source = open(SRC, encoding="utf-8").read()
tree = ast.parse(source)
wanted = ("_web_project_root", "_web_dist_dir")
body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in wanted]
module = ast.fix_missing_locations(ast.Module(
    body=[ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)] + body,
    type_ignores=[]))
ns = {"Path": Path}  # real WindowsPath on this host
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
    if func == "project_root":
        py = str(ns["_web_project_root"](Path(r.get("web_dir", ""))))
    elif func == "dist_dir":
        py = str(ns["_web_dist_dir"](Path(r.get("web_dir", ""))))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} web_dir={r.get('web_dir')!r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_mainwebbu_a76b78: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
