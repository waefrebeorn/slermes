#!/usr/bin/env python3
"""Oracle for gateway/hosted_room_replicas.py:_validate_page.
AST-extracts the LIVE function; identifier/bounded_int helpers are modeled
exactly as the imported partials define them."""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/hosted_room_replicas.py"


class ReplicaError(Exception):
    pass


class ReplicaGapError(ReplicaError):
    pass


IDENT = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._:-]*$")


def _validate_identifier(value, *, label, max_chars):
    if not isinstance(value, str):
        raise ReplicaError(f"{label} must be a string")
    value = value.strip()
    if not value or len(value) > max_chars or not IDENT.fullmatch(value):
        raise ReplicaError(f"invalid {label}")
    return value


def _positive_int(value, *, message):
    if isinstance(value, bool) or not isinstance(value, int) or value < 1:
        raise ReplicaError(message)
    return value


ns = {"ReplicaError": ReplicaError, "ReplicaGapError": ReplicaGapError,
      "_validate_identifier": _validate_identifier, "_positive_int": _positive_int,
      "MAX_ACTOR_ID_CHARS": 128, "Any": object, "dict": dict}
src = open(SRC, encoding="utf-8").read()
for n in ast.parse(src).body:
    if isinstance(n, ast.FunctionDef) and n.name == "_validate_page":
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
live = ns["_validate_page"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    try:
        live(r.get("page"))
        py_ok, py_err = 1, ""
    except Exception as exc:
        py_ok, py_err = 0, f"{type(exc).__name__}: {exc}"
    c_ok, c_err = int(r["ok"]), r.get("err", "")
    cases += 1
    if py_ok != c_ok or py_err != c_err:
        mism += 1
        print(f"MISMATCH {r} py=({py_ok},{py_err!r}) c=({c_ok},{c_err!r})", file=sys.stderr)
print(f"ORACLE validate_page: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
