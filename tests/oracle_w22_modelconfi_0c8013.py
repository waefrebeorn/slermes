#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/model_config.py
(gate w22_modelconfi_0c8013).

The read/apply/write body is bound to a StoreHolder and a live sqlite cursor, so
those are the DB seam. The live pieces run here:

  update_config   -- `_parse_model_config` lifted out of sessions.py and the
                     source's `config.get(key)` / `config.pop(key, None)` /
                     `config[key] = value` application; the `update` callable is
                     represented by the value it returned (None removes).
  json_extract    -- live `_sql_json_extract` (with `_sql_literal`).
  select_ids_sql  -- the f-string session_ids_with_model_config_key builds around
                     that live helper.
"""
import ast
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SESSIONS = QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "sessions.py"
DDL = QUARRY / "persistence" / "sqlite" / "_schema_ddl.py"
GATE = "w22_modelconfi_0c8013"


def lift_functions(path, names):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    g = {"json": json}
    wanted = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            wanted.append(node)
    for node in sorted(wanted, key=lambda n: (n.lineno, n.col_offset)):
        node.decorator_list = []
        exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"), g)
    missing = names - set(g)
    if missing:
        raise SystemExit("missing %s in %s" % (sorted(missing), path))
    return g


_PMC = lift_functions(SESSIONS, {"_parse_model_config"})["_parse_model_config"]
_DDL = lift_functions(DDL, {"_sql_literal", "_sql_json_extract"})
_sql_json_extract = _DDL["_sql_json_extract"]

WANTED = {"update_config", "json_extract", "select_ids_sql"}


def expected(r):
    f = r.get("func")
    if f == "update_config":
        config = _PMC(r.get("row_config"))
        key = r.get("key")
        value = r.get("value")
        if value is None:
            config.pop(key, None)
        else:
            config[key] = value
        return config
    if f == "json_extract":
        return _sql_json_extract(r.get("expression"), r.get("path"))
    if f == "select_ids_sql":
        key = r.get("key")
        return ("SELECT id FROM sessions WHERE "
                f"{_sql_json_extract('model_config', '$.' + key)} IS NOT NULL "
                "ORDER BY started_at DESC")
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
