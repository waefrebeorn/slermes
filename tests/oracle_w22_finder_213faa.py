#!/usr/bin/env python3
"""Differential oracle for hermes_agent/compat/finder.py (gate w22_finder_213faa).

The module's portable half is name algebra, so the two pure functions are lifted
straight out of the AST (`current_name`, and `_child_names` with `current_name`
already in its globals) and executed live. The strict finder's ImportError text
is produced by the live `Finder(moves, strict=True).find_spec(...)`, whose strict
branch raises before it touches the import system; the derived-name fixtures use
synthetic names no real module can shadow.

Every answer is compared as a decoded JSON value, so object key order is not
part of the contract.
"""
import ast
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_agent" / "compat" / "finder.py"
GATE = "w22_finder_213faa"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("finder_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["finder_live"] = live
_spec.loader.exec_module(live)

G = {}
_tree = ast.parse(SRC.read_text(encoding="utf-8"))
_wanted = {"current_name": False, "_child_names": False}
for _node in _tree.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in _wanted:
        _node.decorator_list = []
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
        _wanted[_node.name] = True
for _name, _found in _wanted.items():  # pragma: no cover - both must exist
    if not _found:
        raise SystemExit("%s not found in %s" % (_name, SRC))

current_name = G["current_name"]
child_names = G["_child_names"]
WANTED = {"current_name", "child_names", "strict_error"}


def as_dict(moves):
    return moves if isinstance(moves, dict) else moves


def py_current_name(name, moves):
    try:
        return current_name(name if name is not None else "", as_dict(moves))
    except (TypeError, AttributeError):
        return None


def py_child_names(moves):
    try:
        return child_names(as_dict(moves))
    except (AttributeError, TypeError):
        # a non-mapping has no .items(): the source's own TypeError path
        return {}


def py_strict_error(name, moves):
    try:
        live.Finder(as_dict(moves), strict=True).find_spec(name if name is not None else "")
    except ImportError as exc:
        return str(exc)
    return None


def expected(r):
    f = r.get("func")
    if f == "current_name":
        return py_current_name(r.get("name"), r.get("moves"))
    if f == "child_names":
        return py_child_names(r.get("moves"))
    if f == "strict_error":
        return py_strict_error(r.get("name"), r.get("moves"))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
