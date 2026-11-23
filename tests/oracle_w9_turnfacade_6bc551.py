#!/usr/bin/env python3
"""Differential oracle for gate w9_turnfacade_6bc551 (agent/turn_facade_lease.py).

AST-extracts _committed_lease_expiry and DurableTurnLease._current_session_id from
the quarry. Store/db readers are scripted (numeric / non-numeric / raise) and the C
seam receives the resolved reader result. Compares 'out' against the live result.
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
if not (QUARRY / "agent" / "turn_facade_lease.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "agent/turn_facade_lease.py")
NAME = "w9_turnfacade_6bc551"

_TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
_FLAGS = __future__.annotations.compiler_flag


def _compile(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


_CODE = {}
for _n in _TREE.body:
    if isinstance(_n, ast.FunctionDef) and _n.name == "_committed_lease_expiry":
        _CODE["_committed_lease_expiry"] = _compile(_n)
    if isinstance(_n, ast.ClassDef) and _n.name == "DurableTurnLease":
        for _m in _n.body:
            if isinstance(_m, ast.FunctionDef) and _m.name == "_current_session_id":
                _CODE["_current_session_id"] = _compile(_m)
if set(_CODE) != {"_committed_lease_expiry", "_current_session_id"}:
    print(f"ORACLE {NAME}: FATAL missing live defs", file=sys.stderr)
    sys.exit(1)

_MOD = types.ModuleType("w9tfl_live_turn_facade_lease")
_MOD.__dict__.update({"__name__": "w9tfl_live_turn_facade_lease", "__builtins__": builtins,
                      "LEASE_TTL_SECONDS": 300.0, "logger": types.SimpleNamespace(debug=lambda *a, **k: None)})
sys.modules["w9tfl_live_turn_facade_lease"] = _MOD
_NS = _MOD.__dict__
for _nm, _c in _CODE.items():
    exec(_c, _NS)


def _materialize(value):
    if isinstance(value, dict) and value.get("__raise__"):
        raise RuntimeError("scripted reader failure")
    return value


def drive(rec):
    inp, func = rec["in"], rec["func"]
    if func == "committed_expiry":
        value = inp.get("value")
        mode = inp.get("mode", "none")
        db, store = None, None
        if mode == "store":
            store = types.SimpleNamespace(coordination=types.SimpleNamespace(
                turn_lease_expires_at=lambda **kw: _materialize(value)))
        elif mode == "reader":
            db = types.SimpleNamespace(session_turn_lease_expires_at=lambda s, h: _materialize(value))
        else:
            db = types.SimpleNamespace()
        return _NS["_committed_lease_expiry"](db, store, "sess", "holder", inp.get("floor", 0.0))
    if func == "current_session_id":
        fake = types.SimpleNamespace(
            agent=types.SimpleNamespace(session_id=inp.get("agent_sid")),
            session_id=inp.get("fallback", ""))
        return _NS["_current_session_id"](fake)
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
