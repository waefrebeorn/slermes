#!/usr/bin/env python3
"""Differential oracle for w7_moaalternat_e7a67f -> agent/moa_alternation.py.

The live module is loaded with spec_from_file_location. agent.agent_runtime_helpers
is stubbed with the REAL _merge_user_content / _UNMERGEABLE AST-extracted from the
quarry, so merge semantics are the live ones. agent.error_classifier is stubbed
with a FailoverReason identity sentinel for the rejection check.
"""
import ast
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")

# AST-extract the real merge helper from the quarry (no import of the heavy module).
src = open(Q + "/agent/agent_runtime_helpers.py", encoding="utf-8").read()
tree = ast.parse(src)
ns = {}
for node in tree.body:
    if isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "_UNMERGEABLE" for t in node.targets):
        exec(compile(ast.Module(body=[node], type_ignores=[]), "<helpers>", "exec"), ns)
    if isinstance(node, ast.FunctionDef) and node.name == "_merge_user_content":
        exec(compile(ast.Module(body=[node], type_ignores=[]), "<helpers>", "exec"), ns)
_mod("agent.agent_runtime_helpers",
     _UNMERGEABLE=ns["_UNMERGEABLE"], _merge_user_content=ns["_merge_user_content"])

SENTINEL = object()
_mod("agent.error_classifier",
     FailoverReason=types.SimpleNamespace(role_alternation=SENTINEL),
     classify_api_error=lambda exc, **kw: types.SimpleNamespace(
         reason=SENTINEL if getattr(exc, "_role_alt", False) else "other"))

SRC = Q + "/agent/moa_alternation.py"
spec = importlib.util.spec_from_file_location("moaalt_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["moaalt_live"] = live
spec.loader.exec_module(live)


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


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
    try:
        if func == "destination_key":
            py = live.destination_key(r.get("runtime") or {})
        elif func == "merge":
            py = live.merge_same_role_messages(r.get("messages") or [])
        elif func == "role_alt":
            flag = bool(r.get("classified", False))
            exc = types.SimpleNamespace(_role_alt=flag)
            py = live.is_role_alternation_rejection(exc, r.get("runtime") or {})
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = canonical(r.get("out"))
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={canonical(py)!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_moaalternat_e7a67f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
