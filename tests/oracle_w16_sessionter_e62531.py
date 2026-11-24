#!/usr/bin/env python3
"""Differential oracle for w16_sessionter_e62531 (gateway/session_terminal_cwd.py).

AST-extracts the LIVE _local_backend into a registered namespace (the module's
own top-level imports are stdlib-only; the None-policy branch's lazy imports are
registered scripted stubs with an injected _tenv seam). shall_persist is the
exact branch predicate of live persist_turn_cwd.
"""
import __future__
import ast
import builtins
import json
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "gateway" / "session_terminal_cwd.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "session_terminal_cwd.py")
NAME = "w16_sessionter_e62531"

TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
FLAGS = __future__.annotations.compiler_flag

def cnode(n):
    return compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec",
                   flags=FLAGS, dont_inherit=True)

NS = types.ModuleType("wlive_sessionter")
NS.__dict__.update({"__name__": "wlive_sessionter", "__builtins__": builtins, "json": json})
sys.modules["wlive_sessionter"] = NS
_found = False
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name == "_local_backend":
        exec(cnode(node), NS.__dict__)
        _found = True
if not _found:
    print(f"ORACLE {NAME}: FATAL missing live _local_backend", file=sys.stderr)
    sys.exit(1)

# Lazy imports used when policy is None.
tt = types.ModuleType("tools.terminal_tool")
tt._ensure_terminal_env_bridged = lambda: None
sys.modules["tools.terminal_tool"] = tt
ttc = types.ModuleType("tools.terminal_tool_config")
sys.modules["tools.terminal_tool_config"] = ttc
tools = types.ModuleType("tools")
tools.__path__ = []
tools.terminal_tool = tt
tools.terminal_tool_config = ttc
sys.modules["tools"] = tools
_ENV = {"v": "local"}

def _tenv(name, default):
    return _ENV["v"] if _ENV["v"] is not None else default

ttc._tenv = _tenv

def drive(rec):
    func = rec["func"]
    if func == "local_backend":
        pj = rec.get("policy_json")
        if pj is None:
            _ENV["v"] = rec.get("env_value", "local")
            pol = None
        else:
            pol = types.SimpleNamespace(terminal_json=pj)
        return bool(NS._local_backend(pol))
    if func == "should_persist":
        cwd = rec.get("cwd") or ""
        saved = rec.get("saved")
        return bool(cwd) and saved != cwd
    raise KeyError(func)

def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            out = drive(rec)
        except BaseException as e:
            out = {"__raise__": type(e).__name__}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:500]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:500]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
