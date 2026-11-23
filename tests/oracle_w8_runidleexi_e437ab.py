#!/usr/bin/env python3
"""Differential oracle for live Hermes gateway/run_idle_exit.py (port w8_runidleexi_e437ab).

Pure helpers are AST-extracted from the LIVE source and driven through explicit
config / clock / runner seams (the Python module touches the process environment
and monotonic clock, which the C side receives as fixture inputs).
"""
import ast
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
import gateway  # noqa: F401  parent-package bootstrap

SRC = QUARRY + "/gateway/run_idle_exit.py"
tree = ast.parse(open(SRC, encoding="utf-8").read())


def extracted(name):
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            c = copy.deepcopy(n)
            c.decorator_list = []
            c.body = [b for b in c.body if not isinstance(b, (ast.Import, ast.ImportFrom))]
            return c
    raise RuntimeError("live fn not found: " + name)


def build(name, extra):
    ns = dict(extra)
    exec(compile(ast.Module(body=[extracted(name)], type_ignores=[]), SRC, "exec"), ns)
    return ns

QUIET = types.SimpleNamespace(warning=lambda *a, **k: None, info=lambda *a, **k: None,
                              debug=lambda *a, **k: None)
FAKE_ASYNCIO = types.SimpleNamespace(ensure_future=lambda x: None)


def compute(r):
    func = r.get("func")
    if func == "idle_seconds":
        value = r.get("value")
        ns = build("idle_exit_seconds", {
            "logger": QUIET,
            "DEFAULT_IDLE_EXIT_SECONDS": 600.0,
            "_load_gateway_config": lambda: {"gateway": {"unmanaged_idle_exit_seconds": value}},
        })
        return ns["idle_exit_seconds"](None)
    if func == "client_busy":
        now = r.get("now", 0.0)
        attaching = bool(r.get("attaching", False))
        ns = build("_client_busy", {
            "time": types.SimpleNamespace(monotonic=lambda: now),
            "_attaching": lambda runner: attaching,
        })
        runner = types.SimpleNamespace(
            _idle_exit_clients=r.get("clients", 0),
            _idle_exit_last_activity=r.get("last_activity", 0.0))
        return ns["_client_busy"](runner, window=r.get("window", 0.0))
    if func == "exit_reason":
        ns = build("_begin_idle_exit", {
            "IDLE_EXIT_REASON": "idle_exit",
            "IDLE_EXIT_STOP_PREFIX": "auto-started gateway idle",
            "logger": QUIET,
            "asyncio": FAKE_ASYNCIO,
        })
        runner = types.SimpleNamespace(
            session_runtime_descriptor=types.SimpleNamespace(update=lambda **kw: None),
            stop=lambda: None)
        ns["_begin_idle_exit"](runner, r.get("window", 0.0), r.get("restart"))
        return runner._exit_reason
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_runidleexi_e437ab: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
