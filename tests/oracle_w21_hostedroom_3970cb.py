#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_room_authority_history.py
(gate w21_hostedroom_3970cb).

The live module is imported from its real source. Only two imports are replaced,
both pure code lifted from the real files:
  * gateway.hosted_rooms_common.identifier -- AST-extracted with IDENTIFIER_RE,
  * persistence.gateway.hosted_rooms.AuthorityHistoryError -- the persistence
    package opens databases at import time, so the error class (whose only
    contract here is its name and str()) is declared locally.
`read_history_locked` (SQL on a live connection) is out of scope for both sides.
"""
import __future__
import ast
import importlib.util
import json
import os
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
GATE = "w21_hostedroom_3970cb"
SRC = os.path.join(QUARRY, "gateway", "hosted_room_authority_history.py")
COMMON = os.path.join(QUARRY, "gateway", "hosted_rooms_common.py")
_ANN = __future__.annotations.compiler_flag


def _exec(path, names, into):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                         flags=_ANN, dont_inherit=True), into)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in names for t in targets):
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                             flags=_ANN, dont_inherit=True), into)


class AuthorityHistoryError(ValueError):
    """persistence.gateway.hosted_rooms.AuthorityHistoryError (name + str only)."""


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


COMMON_G = {"re": re}
_exec(COMMON, {"identifier", "IDENTIFIER_RE"}, COMMON_G)

_mk("gateway")
_mk("gateway.hosted_rooms_common", identifier=COMMON_G["identifier"],
    IDENTIFIER_RE=COMMON_G["IDENTIFIER_RE"])
_mk("persistence")
_mk("persistence.gateway")
_mk("persistence.gateway.hosted_rooms", AuthorityHistoryError=AuthorityHistoryError)

_sp = importlib.util.spec_from_file_location(GATE + "_live", SRC)
live = importlib.util.module_from_spec(_sp)
sys.modules[GATE + "_live"] = live
_sp.loader.exec_module(live)


def err_of(exc):
    return {"status": "error", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    f = r.get("func")
    if f == "validate":
        try:
            spans = live.validate_history(r.get("value"), gateway_id=r.get("gateway_id"),
                                          epoch=r.get("epoch"))
            return {"status": "ok", "value": [s.as_mapping() for s in spans]}
        except Exception as exc:  # noqa: BLE001 - reported as the harness shapes it
            return err_of(exc)
    if f == "at":
        try:
            spans = tuple(live.AuthoritySpan(**m) for m in (r.get("history") or []))
            return {"status": "ok", "value": live.at_sequence(spans, r.get("seq")).as_mapping()}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    if f == "origin":
        try:
            return {"status": "ok", "value": live.origin_gateway_id(r.get("room"))}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    if f == "identifier":
        try:
            pattern = COMMON_G["IDENTIFIER_RE"] if r.get("check_shape", True) else None
            return {"status": "ok",
                    "value": COMMON_G["identifier"](r.get("value"), label=r.get("label", ""),
                                                   error=AuthorityHistoryError,
                                                   max_chars=r.get("max_chars", 128),
                                                   pattern=pattern)}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"validate", "at", "origin", "identifier"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
