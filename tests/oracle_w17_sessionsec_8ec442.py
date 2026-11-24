#!/usr/bin/env python3
"""Differential oracle for w17_sessionsec_8ec442 (gateway/session_secret_prompt.py).

AST-extracts the LIVE _skipped and the real nested callback of secret_prompt_scope
into a registered namespace and drives the callback with scripted seams:
authority.sessions.get, ask_viewer and save_env_value_secure.
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
SRC = QUARRY + "/gateway/session_secret_prompt.py"
NAME = "w17_sessionsec_8ec442"
ANN = __future__.annotations.compiler_flag

TREE = ast.parse(open(SRC, encoding="utf-8").read())
NS = {"__builtins__": builtins}
SKIPPED_NODE = None
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name == "_skipped":
        SKIPPED_NODE = node
if SKIPPED_NODE is None:
    raise SystemExit("ORACLE %s: missing live _skipped" % NAME)
exec(compile(ast.Module(body=[SKIPPED_NODE], type_ignores=[]), SRC, "exec",
             flags=ANN, dont_inherit=True), NS)
# Module-level constant the nested callback closes over.
for node in TREE.body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "SECRET_TIMEOUT_S"
                                            for t in node.targets):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN, dont_inherit=True), NS)

CB = None
for node in ast.walk(TREE):
    if isinstance(node, ast.FunctionDef) and node.name == "callback":
        CB = node
        break
if CB is None:
    raise SystemExit("ORACLE %s: missing live nested callback" % NAME)

# The live callback does LOCAL imports of ask_viewer / save_env_value_secure, so
# scripted globals are shadowed. Register stub parent packages + modules in
# sys.modules so those imports resolve to our scripted seams.
from pathlib import Path
for pkg in ("gateway", "hermes_cli"):
    m = types.ModuleType(pkg)
    m.__path__ = [str(Path(QUARRY) / pkg)]
    sys.modules[pkg] = m
ssr = types.ModuleType("gateway.session_server_requests")
HOLDER = {"ask": None, "save": None}
ssr.ask_viewer = lambda *a, **kw: HOLDER["ask"](*a, **kw)
sys.modules["gateway.session_server_requests"] = ssr
cfg = types.ModuleType("hermes_cli.config")
cfg.save_env_value_secure = lambda *a, **kw: HOLDER["save"](*a, **kw)
sys.modules["hermes_cli.config"] = cfg


def compute(r):
    f = r.get("func")
    if f == "skipped":
        return NS["_skipped"](r.get("var_name", ""), r.get("reason", ""), r.get("message", ""))
    if f == "callback_result":
        g = dict(NS)
        g["session_id"] = "S"
        g["authority"] = types.SimpleNamespace(
            sessions={"S": types.SimpleNamespace(event_stream=object())}
            if r.get("live_session") else {})
        g["agent"] = types.SimpleNamespace(
            _interrupt_requested=bool(r.get("interrupt", False)))
        HOLDER["ask"] = lambda *a, **kw: r.get("ask_value")
        HOLDER["save"] = lambda *a, **kw: dict(r.get("saved") or {})
        exec(compile(ast.Module(body=[CB], type_ignores=[]), SRC, "exec",
                     flags=ANN, dont_inherit=True), g)
        meta = r.get("metadata") if r.get("has_metadata") else None
        return g["callback"](r.get("var_name", ""), r.get("prompt", ""), meta)
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
