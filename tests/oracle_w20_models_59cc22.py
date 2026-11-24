#!/usr/bin/env python3
"""Differential oracle for plugins/teams_pipeline/models.py (gate w20_models_59cc22).

The live module is pure stdlib, so the oracle executes its real source: the
conversion helpers (`_camel`, `_pick`, `_str`, `_parse_datetime`,
`_serialize_datetime`) are lifted out of the AST (functions and lambda
assignments, sync and async), and the five dataclasses are imported as-is so
`from_dict` / `to_dict` are the live implementations. A model instance is
canonicalized to the JSON the C side carries: datetimes in their UTC `...Z`
wire form, nested models as full field dicts.
"""
import __future__
import ast
import importlib.util
import json
import sys
from dataclasses import fields
from datetime import UTC, datetime
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "teams_pipeline" / "models.py"
GATE = "w20_models_59cc22"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

G = {"__name__": "models_helpers_live", "datetime": datetime, "UTC": UTC, "Any": object,
     "str": str}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in names for t in targets):
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                             flags=_ANN, dont_inherit=True), into)


_exec(SRC, {"_parse_datetime", "_serialize_datetime", "_camel", "_pick", "_str"}, G)

_spec = importlib.util.spec_from_file_location("teams_pipeline_models_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["teams_pipeline_models_live"] = live
_spec.loader.exec_module(live)


def canon(v):
    """The JSON shape the C port carries for one model value."""
    if isinstance(v, datetime):
        return G["_serialize_datetime"](v)
    if isinstance(v, live._Model):
        return {f.name: canon(getattr(v, f.name)) for f in fields(v)}
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def err_of(exc):
    return {"status": "error", "type": type(exc).__name__, "message": str(exc)}


def expected(r):
    f = r.get("func")
    if f == "camel":
        return G["_camel"](r.get("name"))
    if f == "str":
        return G["_str"](r.get("value"))
    if f == "pick":
        return G["_pick"](r.get("payload"), *r.get("keys", []))
    if f == "parse":
        try:
            return {"status": "ok",
                    "value": G["_serialize_datetime"](G["_parse_datetime"](r.get("value")))}
        except Exception as exc:  # noqa: BLE001 - reported as the harness shapes it
            return err_of(exc)
    model = getattr(live, r.get("model") or "", None)
    if model is None:
        return {"status": "error", "type": "NameError", "message": str(r.get("model"))}
    if f == "from_dict":
        try:
            return {"status": "ok", "value": canon(model.from_dict(r.get("payload")))}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    if f == "roundtrip":
        try:
            return {"status": "ok", "value": model.from_dict(r.get("payload")).to_dict()}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"camel", "str", "pick", "parse", "from_dict", "roundtrip"}
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
