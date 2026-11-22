#!/usr/bin/env python3
# coverage-targets: _env_timezone, _timezone_cache_identity
"""LIVE oracle for hermes_time.py:_env_timezone."""
import ast
import json
import os
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_time.py"
t = ast.parse(open(SRC, encoding="utf-8").read())
fns = [n for n in t.body if isinstance(n, ast.FunctionDef)
       and n.name in ("_env_timezone", "_timezone_cache_identity")]
for fn in fns: fn.decorator_list = []

secret_scope = types.ModuleType("agent.secret_scope")
NS = {"os": os}
exec(compile(ast.Module(body=fns, type_ignores=[]), SRC, "exec"), NS)
NS["_env_timezone"] = NS["_env_timezone"]
NS["get_config_path"] = lambda: NS["_cfg"]

cases = mism = 0
for line in sys.stdin:
    if '"out"' not in line:
        continue
    r = json.loads(line)
    for k in ("HERMES_TIMEZONE",):
        os.environ.pop(k, None)
    if r.get("tz"):
        os.environ["HERMES_TIMEZONE"] = r["tz"]
    secret_scope.is_multiplex_active = lambda: bool(r.get("multiplex"))
    sys.modules["agent.secret_scope"] = secret_scope
    py = NS["_env_timezone"]()
    got = r["out"]
    NS["_cfg"] = r.get("config_path") or ""
    py_ident = list(NS["_timezone_cache_identity"]())
    got_ident = [r.get("identity_kind"), r.get("identity_value")]
    cases += 1
    if py_ident != got_ident:
        mism += 1
        print(f"MISMATCH identity in={r} py={py_ident!r} c={got_ident!r}", file=sys.stderr)
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE hermes_time_env: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
