#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/_hosted_work.py (gate w34_hostedwork_56ec19).

The live module is imported as a package module (its relative import of
``._hosted_rooms_common`` needs the package context).  Its pure builders are
called against a recording/fake connection: no sqlite3 database, no schema, no
savepoint and no transaction is created.  That is the seam -- what is compared
is exactly what those values decide:

  * ``row_size_sql`` / ``usage_sql`` / ``retained_fields`` (the byte charge);
  * ``scope_disposition`` over a room authority projection;
  * ``_guards`` / ``_invalid_guards`` (the DDL text, statement for statement);
  * ``save_locked`` (the upsert statement and its parameters);
  * ``_budget`` (the capacity predicate);
  * ``read_history_locked`` / ``source_prefix_locked`` (the lineage chain and
    the history-prefix check, without their digests);
  * ``_capture_tasks`` (the bounded task/receipt slice);
  * the module's constants.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_hostedwork_56ec19"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
# `tests/` holds the shared seam stubs.
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from persistence.sqlite.gateway import _hosted_work as live  # noqa: E402
from session_rules import hosted_rooms as rules  # noqa: E402

WANTED = {
    "retained_fields", "row_size", "usage", "scope_disposition", "guards", "invalid_guards",
    "save", "budget", "read_history", "prefix", "capture_tasks", "constants",
}


class _Cur:
    """A sqlite3.Cursor stand-in over a canned value."""

    def __init__(self, value):
        self.value = value

    def fetchone(self):
        return self.value

    def fetchall(self):
        return self.value or []

    def __iter__(self):
        return iter(self.value or [])


class _Conn:
    """A sqlite3.Connection stand-in: it answers from ``handler`` and records."""

    def __init__(self, handler=None):
        self.handler = handler or (lambda sql, params: None)
        self.calls = []

    def execute(self, sql, params=None):
        self.calls.append((sql, tuple(params) if isinstance(params, (list, tuple)) else params))
        return _Cur(self.handler(sql, params))


# ── expected values, computed by the live module ────────────────────────────


def retained_fields(req):
    return list(rules.retained_fields(req.get("table")))


def row_size(req):
    return live.row_size_sql(req.get("table"), req.get("prefix") or "")


def usage(req):
    return list(live.usage_sql(tuple(req.get("tables") or [])))


def scope_disposition(req):
    current = req.get("current")
    conn = _Conn(lambda sql, params: tuple(current) if current is not None else None)
    return live.scope_disposition(conn, req.get("table"), req.get("row") or {})


def guards(req):
    conn = _Conn()
    live._guards(conn, tuple(req.get("tables") or []))
    return [sql for sql, _params in conn.calls]


def invalid_guards(_req):
    conn = _Conn()
    live._invalid_guards(conn)
    return [sql for sql, _params in conn.calls]


def save(req):
    """save_locked's statement and parameters.

    ``record_json`` is the seam (session_rules' canonical encode).  The fixture
    carries the encoding the caller would produce, so the comparison covers the
    statement shape and the whole parameter list, including that the fixture's
    canonical bytes are the ones the store writes.
    """

    def handler(sql, params):
        if sql.startswith("SELECT (SELECT COALESCE(SUM"):
            return (0, 0)
        if sql.startswith("SELECT 20+"):
            return (0,) if " FROM (" in sql else None
        return (0,)

    conn = _Conn(handler)
    live.save_locked(conn, req.get("table"), req.get("record"),
                     target_install_id=req.get("target_install_id"),
                     route_generation=req.get("route_generation"))
    sql, params = conn.calls[-1]
    return {"sql": sql, "values": list(params)}


def budget(req):
    def handler(sql, params):
        if sql.startswith("SELECT (SELECT COALESCE(SUM"):
            return (req.get("total"), req.get("count"))
        if " FROM (" in sql:
            return (req.get("size"),)
        old = req.get("old_size")
        return None if old is None else (old,)

    conn = _Conn(handler)
    proposed = {
        "room_id": "room-1",
        "producer_gateway_id": "gw-a",
        "producer_epoch": 1,
        "target_install_id": "install-1",
    }
    try:
        live._budget(conn, req.get("table"), proposed)
    except live.WorkRecordCapacityError:
        return True
    return False


def read_history(req):
    rows = req.get("rows") or []
    conn = _Conn(lambda sql, params: rows)
    try:
        history = live.read_history_locked(conn, "room-1", gateway_id=req.get("gateway_id"),
                                           epoch=req.get("epoch"))
    except live.AuthorityHistoryError as exc:
        return {"error": str(exc)}
    if history is None:
        return {"none": True}
    return {"history": history}


def _rows_from_spans(spans):
    """The transfer rows read_history_locked would have read to build ``spans``.

    ``spans[0]`` is built from the first transfer's previous owner and each
    further span comes from one row, so an N-span history needs N-1 rows.
    """
    rows = []
    for index in range(1, len(spans)):
        row = {
            "seq": spans[index]["from_seq"],
            "authority_epoch": spans[index]["epoch"],
            "actor_json": json.dumps({"kind": "system", "id": "authority-control"},
                                     separators=(",", ":")),
            "payload_json": json.dumps(
                {
                    "previous_gateway_id": spans[index - 1]["gateway_id"],
                    "authority_gateway_id": spans[index]["gateway_id"],
                    "authority_epoch": spans[index]["epoch"],
                },
                separators=(",", ":")),
        }
        rows.append(row)
    return rows


def prefix(req):
    rows = _rows_from_spans(req.get("spans") or [])
    events = req.get("events") or []

    def handler(sql, params):
        if "SELECT seq,authority_epoch,actor_json,payload_json" in sql:
            return rows
        return events

    conn = _Conn(handler)
    try:
        spans, _digest = live.source_prefix_locked(conn, "room-1", req.get("authority"), req.get("seq"))
    except (live.AuthorityHistoryError, live.WorkRecordPrefixError) as exc:
        return {"error": str(exc)}
    return {"spans": [span.as_mapping() for span in spans]}


def capture_tasks(req):
    def handler(sql, params):
        if "sqlite_master" in sql:
            name = params[0] if params else None
            if name == "hosted_room_driver_tasks":
                return None if req.get("store_missing") else (1,)
            return None
        if "FROM hosted_room_driver_tasks" in sql:
            return req.get("tasks") or []
        if "FROM hosted_room_remote_runs" in sql:
            return req.get("receipts") or []
        return None

    conn = _Conn(handler)
    tasks, receipts, reason = live._capture_tasks(conn, "room-1")
    return {"tasks": tasks, "receipts": receipts, "reason": reason}


def constants(_req):
    return {
        "max_tasks": rules.MAX_TASKS,
        "max_receipts": rules.MAX_RECEIPTS,
        "max_bytes": rules.MAX_BYTES,
        "max_store_bytes": rules.MAX_STORE_BYTES,
        "max_store_rows": rules.MAX_STORE_ROWS,
        "max_authority_spans": rules.MAX_AUTHORITY_SPANS,
        "transfer_kinds": list(live.TRANSFER_KINDS),
        "limitations": list(rules.LIMITATIONS),
        "tables": {
            "source": rules.SOURCE_TABLE,
            "target": rules.TARGET_TABLE,
            "pending": rules.PENDING_TABLE,
            "invalid": rules.INVALID_TABLE,
        },
    }


HANDLERS = {
    "retained_fields": retained_fields,
    "row_size": row_size,
    "usage": usage,
    "scope_disposition": scope_disposition,
    "guards": guards,
    "invalid_guards": invalid_guards,
    "save": save,
    "budget": budget,
    "read_history": read_history,
    "prefix": prefix,
    "capture_tasks": capture_tasks,
    "constants": constants,
}


def expected(req):
    return HANDLERS[req.get("func")](req)


def _brief(value, limit=600):
    """A mismatch line short enough to read when a statement is kilobytes long."""
    text = repr(value)
    return text if len(text) <= limit else text[:limit] + "...<%d chars>" % len(text)


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
            print("MISMATCH %s case=%r py=%s c=%s"
                  % (r.get("func"), r.get("case"), _brief(py), _brief(got)), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
