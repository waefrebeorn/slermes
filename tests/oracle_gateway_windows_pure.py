# coverage-targets: _normalize_windows_path, _quote_cmd_script_arg, _quote_vbs_string, _sanitize_filename, _gateway_run_argv, _is_access_denied, _should_fall_back
#!/usr/bin/env python3
"""Oracle for the pure Windows-launcher helpers in hermes_cli/gateway_windows.py.

LIVE function bodies and their regex constants are AST-extracted from the
quarry (no heavy import chain). The C harness emits the exact input fields it
used, so no input is re-typed on the Python side."""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/gateway_windows.py"
NS = {"re": re, "str": str, "int": int, "bool": bool, "list": list}

tree = ast.parse(open(SRC, encoding="utf-8").read())
KEEP_FUNCS = {
    "_normalize_windows_path", "_quote_cmd_script_arg", "_quote_vbs_string",
    "_sanitize_filename", "_gateway_run_argv", "_is_access_denied",
    "_should_fall_back",
}
KEEP_CONSTS = {"_ACCESS_DENIED_WORDS", "_FALLBACK_PATTERNS", "_ACCESS_DENIED_PATTERN"}
body = []
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in KEEP_FUNCS:
        n.decorator_list = []
        body.append(n)
    elif isinstance(n, ast.Assign):
        for t in n.targets:
            if isinstance(t, ast.Name) and t.id in KEEP_CONSTS:
                body.append(n)
exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), NS)
missing = sorted(KEEP_FUNCS - set(NS))
if missing:
    print(f"FATAL oracle_gateway_windows_pure: missing LIVE defs {missing}")
    sys.exit(2)


def project(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "message": str(v)}
    return v


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or "func" not in line:
        continue
    r = json.loads(line)
    fn = r["func"]
    a = r["args"] or {}
    try:
        want = project(NS[fn](**a))
    except BaseException as exc:
        want = project(exc)
    got = r["out"]
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {fn} args={a} want={want!r} got={got!r}", file=sys.stderr)
print(f"ORACLE gateway_windows_pure: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
