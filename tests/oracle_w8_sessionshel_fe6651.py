#!/usr/bin/env python3
"""Differential oracle for gateway/session_shell.py (gate w8_sessionshel_fe6651).

The validation and backend decisions are INLINE in shell_exec, so their exact
boolean expressions are AST-extracted from the live source and evaluated with
the live module globals (constants / stdlib) rather than re-implemented here.
"""
import ast, importlib.util, json, os, sys, types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


stub("persistence.core.errors", AuthorityRefusal=type("AuthorityRefusal", (Exception,), {}))
PKG = "w8_sessionshel_fe6651_live_pkg"
pkg = types.ModuleType(PKG)
pkg.__path__ = [Q + "/gateway"]
sys.modules[PKG] = pkg
LIVE_NAME = PKG + ".session_shell"
SRC = Q + "/gateway/session_shell.py"
spec = importlib.util.spec_from_file_location(LIVE_NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

# AST-extract the exact inline expressions from shell_exec.
_tree = ast.parse(Path(SRC).read_text(encoding="utf-8"))
_fn = next(n for n in _tree.body if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
           and n.name == "shell_exec")
_ifs = [n for n in ast.walk(_fn) if isinstance(n, ast.If)]
_val_if = next(n for n in _ifs if "params" in ast.dump(n.test))
_val_expr = ast.Expression(body=_val_if.test)
ast.fix_missing_locations(_val_expr)
VAL_TEST = compile(_val_expr, SRC, "eval")
_assigns = [n for n in ast.walk(_fn) if isinstance(n, ast.Assign)
            and any(isinstance(t, ast.Name) and t.id == "backend" for t in n.targets)]
_backend_assign = _assigns[0]
_backend_expr = ast.Expression(body=_backend_assign.value)
ast.fix_missing_locations(_backend_expr)
BACKEND_EXPR = compile(_backend_expr, SRC, "eval")


def compute(r):
    f = r.get("func")
    if f == "timeout_seconds":
        return live.TIMEOUT_SECONDS
    if f == "local_backends":
        return sorted(live._LOCAL_BACKENDS)
    if f == "handler_names":
        return sorted(live.handlers(None).keys())
    if f == "params_valid":
        params = r.get("params") or {}
        ns = {"set": set, "isinstance": isinstance, "str": str,
              "params": params,
              "ref": types.SimpleNamespace(session_id=r.get("session_id")),
              "command": r.get("command")}
        invalid = bool(eval(VAL_TEST, ns))
        return not invalid
    if f == "backend_is_local":
        policy = types.SimpleNamespace(terminal_json=json.dumps(
            {"TERMINAL_ENV": r.get("terminal_env")}))
        backend = eval(BACKEND_EXPR, {"str": str, "json": json, "policy": policy})
        return backend in live._LOCAL_BACKENDS
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_sessionshel_fe6651: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
