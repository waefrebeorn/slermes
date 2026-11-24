#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/_hosted_rooms_schema.py
(gate w26_hostedroom_598018).

The live module is imported and its value layer is compared directly.  The
sqlite seams are fixed inputs: ``_remote_run_schema_current`` is handed a stub
connection whose ``execute`` yields the fixture's PRAGMA rows, and the DDL text
the parse cases are given is either the module's own ``_SCHEMA_DDL`` or a
hand-written statement fed to the same two patterns.  Those patterns are read
out of the live module's source at run time (not retyped here), so the oracle
cannot drift from the shipped expressions.  ``_REQUIRED_COLUMNS``,
``_LEGACY_COLUMNS`` and the two ``{where}`` templates are compared as values.
"""
import inspect
import json
import os
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_hostedroom_598018"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)

from persistence.sqlite.gateway import _hosted_rooms_schema as live  # noqa: E402

WANTED = {"parse_ddl", "required_columns", "retire_sql",
          "event_bytes_backfill", "legacy_columns", "remote_run_current"}

_SRC = inspect.getsource(live)
PAT_TABLE = re.search(r're\.search\(r"([^"]+)"', _SRC).group(1)
PAT_COLUMNS = re.search(r're\.findall\(r"([^"]+)"', _SRC).group(1)
assert PAT_TABLE == r"EXISTS (\w+)", PAT_TABLE
assert PAT_COLUMNS == r"^\s*(\w+) (?:TEXT|INTEGER|REAL)\b", PAT_COLUMNS


class _Conn:
    """The connection seam: the PRAGMA rows are the fixture's."""

    def __init__(self, rows):
        self.rows = rows

    def execute(self, sql, *params):
        return iter(self.rows)


def parse_ddl(req):
    ddl = req["ddl"]
    table = re.search(PAT_TABLE, ddl).group(1)
    columns = re.findall(PAT_COLUMNS, ddl.split("(", 1)[1], re.MULTILINE)
    return {"table": table, "columns": columns}


def required_columns(req):
    return [{"table": table, "columns": sorted(cols)} for table, cols in live._REQUIRED_COLUMNS]


def retire_sql(req):
    return live._RETIRE_FROM_ROOMS.format(where=req.get("where", ""))


def event_bytes_backfill(req):
    return live._EVENT_BYTES_BACKFILL.format(where=req.get("where", ""))


def legacy_columns(req):
    return [{"table": table, "column": column, "declaration": declaration, "default": default}
            for table, column, declaration, default in live._LEGACY_COLUMNS]


def remote_run_current(req):
    return live._remote_run_schema_current(_Conn(req.get("pk_rows") or []),
                                           frozenset(req.get("columns") or []))


FUNCS = {"parse_ddl": parse_ddl, "required_columns": required_columns,
         "retire_sql": retire_sql, "event_bytes_backfill": event_bytes_backfill,
         "legacy_columns": legacy_columns, "remote_run_current": remote_run_current}


def expected(req):
    try:
        return FUNCS[req["func"]](req)
    except Exception as exc:  # noqa: BLE001
        return {"error": type(exc).__name__}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        py = expected(req)
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
