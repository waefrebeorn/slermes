#!/usr/bin/env python3
"""Differential oracle for gate w9_executionid_8a3a66 (cron/execution_identity.py).

The LIVE enter_cron_execution / current_cron_execution defs are AST-extracted from
the quarry and executed in a registered module namespace with stubbed collaborators
(ContextVar, CronExecution dataclass, current_profile_name, delegation context). The
C port's *_ex seams expose exactly the deterministic identity-building logic those
functions perform. Both sides are fed the same fixture; 'out' is compared.
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
if not (QUARRY / "cron" / "execution_identity.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "cron/execution_identity.py")
NAME = "w9_executionid_8a3a66"
PORTED = ["enter_cron_execution", "exit_cron_execution", "current_cron_execution"]

_TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
_FLAGS = __future__.annotations.compiler_flag
_CODE = {}
for _n in _TREE.body:
    if isinstance(_n, ast.FunctionDef) and _n.name in PORTED:
        _CODE[_n.name] = compile(
            ast.Module(body=[_n], type_ignores=[]), SRC, "exec",
            flags=_FLAGS, dont_inherit=True)
_missing = set(PORTED) - set(_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

_MOD = types.ModuleType("w9ei_live_execution_identity")
_MOD.__dict__.update({"__name__": "w9ei_live_execution_identity", "__builtins__": builtins})
sys.modules["w9ei_live_execution_identity"] = _MOD
_NS = _MOD.__dict__
for _nm in PORTED:
    exec(_CODE[_nm], _NS)


# The live defs do lazy `from hermes_cli.profiles import current_profile_name`
# and `from agent.delegation_context import is_delegated_child_context`. Register
# stubs for both (with parent-package bootstrap) and refill their attrs per case.
def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


_AGENT = _pkg("agent")
_HC = _pkg("hermes_cli")
_PROFILES = types.ModuleType("hermes_cli.profiles")
_DELEG = types.ModuleType("agent.delegation_context")
_PROFILES.current_profile_name = lambda default=None: "default"
_DELEG.is_delegated_child_context = lambda: False
sys.modules["hermes_cli.profiles"] = _PROFILES
sys.modules["agent.delegation_context"] = _DELEG
_HC.profiles = _PROFILES
_AGENT.delegation_context = _DELEG


class _Token:
    pass


class _CtxVar:
    def __init__(self):
        self.value = None

    def set(self, value):
        old = self.value
        self.value = value
        _Token.old = old
        return _Token()

    def get(self):
        return self.value

    def reset(self, token):
        self.value = _Token.old


_NS["_CURRENT"] = _CtxVar()
_NS["CronExecution"] = lambda **kw: types.SimpleNamespace(**kw)


def norm(v):
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    return v


def drive(rec):
    inp = rec["in"]
    func = rec["func"]
    if func == "enter":
        _PROFILES.current_profile_name = lambda default=None: (inp.get("profile") or default)
        _NS["enter_cron_execution"](
            inp.get("job") or {}, inp.get("execution_id", ""),
            inp.get("record") or {})
        return norm(_NS["_CURRENT"].get())
    if func == "current":
        bound = inp.get("bound")
        _NS["_CURRENT"].value = types.SimpleNamespace(**bound) if isinstance(bound, dict) else None
        _DELEG.is_delegated_child_context = lambda: bool(inp.get("delegated", False))
        return norm(_NS["current_cron_execution"]())
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
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
