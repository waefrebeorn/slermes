#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/_routes.py (gate
w23_routes_70b0a8).

The module is private SQL: every exported function is a text builder or the
named parameters of a statement.  There is nothing to stub -- no connection is
opened here, no clock is read -- so the live functions are called directly and
their exact text is compared to the C port's ``out``.
"""
import importlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_routes_70b0a8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("persistence.sqlite.gateway._routes")

WANTED = {"in_window", "freshest_of", "last_active", "fence_key", "namespace",
          "retired_params", "constants"}


def constants():
    return {
        "fence_prefix": live.ROUTE_FENCE_PREFIX,
        "retired_prefix": live.RETIRED_PREFIX,
        "in_window": live._SQL_IN_WINDOW,
        "expired": live._EXPIRED,
        "horizon": live.RETRY_HORIZON_S,
        "recoverable_sql": live._RECOVERABLE_END_REASONS_SQL,
        "donor_columns": live._ORPHAN_DONOR_COLUMNS,
        "retired_sql": live.ROUTE_RETIRED_SQL,
        "recoverable": list(live._RECOVERABLE_END_REASONS),
        "topic_tables": [t[0] for t in live._TOPIC_TABLES],
        "topic_columns": [t[1] for t in live._TOPIC_TABLES],
        "topic_ddl": [t[2] for t in live._TOPIC_TABLES],
    }


def expected(r):
    f = r.get("func")
    if f == "in_window":
        return live._sql_in_window(r.get("expr"))
    if f == "freshest_of":
        return live._sql_freshest_of(r.get("activity"), r.get("session_id_expr"),
                                     r.get("started"))
    if f == "last_active":
        if "alias" in r:
            return live._sql_session_last_active(r["alias"])
        return live._sql_session_last_active()
    if f == "fence_key":
        return live.route_fence_key(r.get("namespace"), r.get("session_id"))
    if f == "namespace":
        return live.route_namespace(r.get("session_key"))
    if f == "retired_params":
        return live.route_retired_params(r.get("session_key"), r.get("entry_json"),
                                         r.get("now"))
    if f == "constants":
        return constants()
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
