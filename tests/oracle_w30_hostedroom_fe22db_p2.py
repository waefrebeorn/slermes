#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_attachments.py
(gate w30_hostedroom_fe22db_p2): the commit, retention and read fences.

The live module is imported by package path from the quarry tree and its own
functions are called.  The seams are fed as fixture inputs:

  * ``db`` is a snapshot of the two files' tables (rooms, events, attachments,
    blobs, quarantine, fences); the oracle materialises it into a real
    in-memory ``sqlite3`` database, so every statement the module writes is
    executed by SQLite and the answer compared is the module's own;
  * blob bytes are real files in a temporary directory (``range_slice``), fed
    by the fixture's declared file size.

Ported here:
  * ``validate_manifest``        -> the canonical manifest (dependency of retain)
  * ``retain_message_attachments``-> the canonicality + commitment fences
  * ``_require_viewer_room``     -> the viewer authority fence
  * ``_read_committed_row``      -> the committed/expired/owned/published/recipient fences
  * ``_read_blob_range``         -> the slice-length fence
  * the quota/TTL constants and the current-column set

Every response is ``{"ok": true, "value": ...}`` or
``{"ok": false, "type": <exception class>, "message": <str or null>}``; the
message is compared only for the contract's own ``AttachmentError`` family.
"""
import json
import sqlite3
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_hostedroom_fe22db_p2"

# tests/ must be appended (not prepended): tests/hermes_cli shadows the quarry package.
if str(HERE) not in sys.path:
    sys.path.append(str(HERE))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.gateway import hosted_room_attachments as live  # noqa: E402
from session_rules import hosted_attachments as rules  # noqa: E402

WANTED = {
    "manifest_validate", "retain", "require_viewer_room", "read_gate",
    "range_slice", "constants",
}

ROOM_DDL = (
    "CREATE TABLE hosted_rooms (room_id TEXT PRIMARY KEY, authority_gateway_id TEXT,"
    " authority_epoch INTEGER, disbanded_at REAL)",
    "CREATE TABLE hosted_room_events (room_id TEXT, event_id TEXT, kind TEXT,"
    " payload_json TEXT)",
    "CREATE TABLE hosted_room_quarantine (room_id TEXT)",
    "CREATE TABLE hosted_room_disband_fences (room_id TEXT)",
)

TABLE_OF = {
    "rooms": "hosted_rooms",
    "events": "hosted_room_events",
    "quarantine": "hosted_room_quarantine",
    "fences": "hosted_room_disband_fences",
}


def _insert(conn, table, rows):
    for row in rows or ():
        keys = list(row)
        if not keys:
            continue
        conn.execute(
            "INSERT INTO %s (%s) VALUES (%s)"
            % (table, ", ".join(keys), ", ".join("?" for _ in keys)),
            [row[key] for key in keys],
        )


def context(r):
    """Materialise the fixture's db snapshot into a real sqlite3 database."""
    snapshot = r.get("db") or {}
    conn = sqlite3.connect(":memory:", isolation_level=None)
    conn.row_factory = sqlite3.Row
    live._initialize(conn)
    for name in snapshot.get("drop") or ():
        conn.execute("DROP TABLE IF EXISTS " + name)
    for key, ddl in zip(("rooms", "events", "quarantine", "fences"), ROOM_DDL):
        if key in snapshot and TABLE_OF[key] not in (snapshot.get("drop") or ()):
            conn.execute(ddl)
            _insert(conn, TABLE_OF[key], snapshot.get(key))
    _insert(conn, "hosted_room_attachments", snapshot.get("attachments"))
    _insert(conn, "hosted_room_attachment_blobs", snapshot.get("blobs"))
    return SimpleNamespace(conn=conn, root=Path(tempfile.mkdtemp(prefix="w30p2-")), db=snapshot)


def ok(value):
    return {"ok": True, "value": value}


def err(exc):
    return {
        "ok": False,
        "type": type(exc).__name__,
        "message": str(exc) if isinstance(exc, live.AttachmentError) else None,
    }


def guard(fn):
    try:
        return ok(fn())
    except Exception as exc:  # noqa: BLE001 - the exception is the answer
        return err(exc)


# --------------------------------------------------------------------------
# functions
# --------------------------------------------------------------------------

CONSTANTS = (
    ("max_attachments_per_message", rules.MAX_ATTACHMENTS_PER_MESSAGE),
    ("max_attachment_bytes", rules.MAX_ATTACHMENT_BYTES),
    ("max_message_attachment_bytes", rules.MAX_MESSAGE_ATTACHMENT_BYTES),
    ("max_room_attachment_bytes", rules.MAX_ROOM_ATTACHMENT_BYTES),
    ("max_gateway_blob_bytes", rules.MAX_GATEWAY_BLOB_BYTES),
    ("max_room_attachment_count", rules.MAX_ROOM_ATTACHMENT_COUNT),
    ("max_gateway_attachment_count", rules.MAX_GATEWAY_ATTACHMENT_COUNT),
    ("max_room_uncommitted_bytes", rules.MAX_ROOM_UNCOMMITTED_BYTES),
    ("max_room_uncommitted_count", rules.MAX_ROOM_UNCOMMITTED_COUNT),
    ("uncommitted_ttl_seconds", rules.UNCOMMITTED_TTL_SECONDS),
    ("disbanded_grace_seconds", rules.DISBANDED_GRACE_SECONDS),
)


def f_constants(r):
    def run():
        out = {key: float(value) for key, value in CONSTANTS}
        out["current_columns"] = sorted(live._CURRENT_COLUMNS)
        return out

    return guard(run)


def f_manifest_validate(r):
    return guard(lambda: rules.validate_manifest(r.get("manifest")))


def f_retain(r):
    c = context(r)

    def run():
        live.retain_message_attachments(
            c.conn, room_id=r.get("room_id"), event_id=r.get("event_id"),
            manifest=r.get("manifest"), now=float(r.get("now")),
        )
        return {
            "rows": [
                {
                    "attachment_id": str(row["attachment_id"]),
                    "state": str(row["state"]),
                    "expires_at": None if row["expires_at"] is None else float(row["expires_at"]),
                    "updated_at": None if row["updated_at"] is None else float(row["updated_at"]),
                }
                for row in c.conn.execute(
                    "SELECT attachment_id, state, expires_at, updated_at"
                    " FROM hosted_room_attachments ORDER BY attachment_id"
                )
            ]
        }

    return guard(run)


def f_require_viewer_room(r):
    c = context(r)
    return guard(lambda: live._require_viewer_room(
        c.conn, room_id=r.get("room_id"),
        authority_gateway_id=r.get("authority_gateway_id"),
        authority_epoch=r.get("authority_epoch"),
    ) and None)


def f_read_gate(r):
    c = context(r)

    def run():
        row = live._read_committed_row(
            c.conn, room_id=r.get("room_id"), attachment_id=r.get("attachment_id"),
            recipient_member_id=r.get("recipient_member_id") or "",
            normalized_event=r.get("event_id"), viewer=bool(r.get("viewer")),
            now=float(r.get("now")), reconciled=bool(r.get("reconciled", True)),
        )
        return live._metadata(row)

    return guard(run)


def f_range_slice(r):
    size = int(r.get("size"))
    offset = int(r.get("offset"))
    length = int(r.get("length"))
    declared = r.get("file_size")

    def run():
        root = Path(tempfile.mkdtemp(prefix="w30p2b-"))
        blob_id = r.get("blob_id") or "blob_" + "0" * 32
        if declared is not None:
            (root / blob_id).write_bytes(b"x" * int(declared))
        data = live._read_blob_range(
            root, blob_id=blob_id, size=size, offset=offset, length=length)
        return {"bytes": len(data)}

    return guard(run)


DISPATCH = {
    "manifest_validate": f_manifest_validate,
    "retain": f_retain,
    "require_viewer_room": f_require_viewer_room,
    "read_gate": f_read_gate,
    "range_slice": f_range_slice,
    "constants": f_constants,
}


def expected(r):
    return DISPATCH[r["func"]](r)


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
            print("MISMATCH %s case=%r\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
