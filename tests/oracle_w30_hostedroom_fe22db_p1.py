#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_attachments.py
(gate w30_hostedroom_fe22db_p1): row shape, ownership and reclamation.

The live module is imported by package path from the quarry tree and its own
functions are called.  The seams are fed as fixture inputs:

  * ``db`` is a snapshot of the two files' tables (rooms, events, attachments,
    blobs, quarantine, fences) plus the names pre-created in the blob root.
    The oracle materialises it into a real in-memory ``sqlite3`` database and a
    real temporary blob directory, so every statement the module writes is
    actually executed by SQLite -- what is compared is the module's own answer,
    never a re-statement of it in Python;
  * ``path``/``file`` I/O (blob bytes, ``iterdir``, ``unlink``) is the real
    temporary directory.

Ported here:
  * ``_metadata``                -> the attachment envelope
  * ``_BLOB_ID_RE.fullmatch``    -> blob id predicate
  * ``_blob_path``               -> the blob file name + its integrity refusal
  * ``_owner_event``             -> the durable event that owns an id
  * ``_named_by_owner_event``    -> the reconciliation's naming rule
  * ``_sweep_orphans``           -> the orphan selection
  * ``_prune``                   -> the expiry/disband/ref-count reclamation

Every response is ``{"ok": true, "value": ...}`` or
``{"ok": false, "type": <exception class>, "message": <str or null>}``; the
message is compared only for the contract's own ``AttachmentError`` family.
"""
import json
import os
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
GATE = "w30_hostedroom_fe22db_p1"

# tests/ must be appended (not prepended): tests/hermes_cli shadows the quarry package.
if str(HERE) not in sys.path:
    sys.path.append(str(HERE))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.gateway import hosted_room_attachments as live  # noqa: E402

WANTED = {
    "metadata", "blob_id_ok", "blob_path", "owner_event", "named_by_owner_event",
    "sweep_select", "prune_plan",
}

ATTACHMENT_COLUMNS = (
    "attachment_id", "upload_id", "room_id", "event_id", "kind", "name", "catalog_name",
    "size", "mime", "sha256", "blob_id", "recipient_member_ids_json", "viewer_access",
    "state", "created_at", "updated_at", "expires_at",
)

# The room-side tables the attachment verbs read.  Only these columns are read by
# this gate's functions; a file without the table is modelled by omitting the key.
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
    "attachments": "hosted_room_attachments",
    "blobs": "hosted_room_attachment_blobs",
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


class FakeFiles:
    """``HostedFiles`` with the file identity kept: the database is a real
    in-memory connection, the blob root a real temporary directory."""

    def __init__(self, conn, root):
        self.conn = conn
        self.root = root

    def refuse_read_only(self):
        return None

    def path(self, placement):
        return self.root / "state.db"

    def attachment_root(self, placement):
        return self.root / "hosted-room-attachments"

    def _hosted_write(self, placement, fn, immediate=True):
        try:
            self.conn.execute("BEGIN IMMEDIATE" if immediate else "BEGIN")
            out = fn(self.conn)
            self.conn.commit()
            return out
        except BaseException:
            self.conn.rollback()
            raise

    def _hosted_read(self, placement, fn):
        return fn(self.conn)


def context(r):
    """Materialise the fixture's db snapshot and blob root."""
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

    root = Path(tempfile.mkdtemp(prefix="w30p1-"))
    blob_root = root / "hosted-room-attachments" / "blobs"
    blob_root.mkdir(parents=True)
    for name in snapshot.get("files") or ():
        (blob_root / str(name)).write_bytes(b"x" * 8)
    repo = live.SQLiteHostedRoomAttachmentRepository.__new__(live.SQLiteHostedRoomAttachmentRepository)
    repo._store = SimpleNamespace(read_only=False, require_open=lambda: None)
    repo._files = FakeFiles(conn, root)
    return SimpleNamespace(conn=conn, root=root, blob_root=blob_root, repo=repo, db=snapshot)


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

def f_metadata(r):
    row = r.get("row")
    return guard(lambda: live._metadata(row, idempotent=bool(r.get("idempotent"))))


def f_blob_id_ok(r):
    return guard(lambda: live._BLOB_ID_RE.fullmatch(r.get("value")) is not None)


def f_blob_path(r):
    return guard(lambda: live._blob_path(Path("."), r.get("blob_id")).name)


def f_owner_event(r):
    c = context(r)

    def run():
        row = live._owner_event(c.conn, r.get("room_id"), r.get("event_id"))
        return None if row is None else {"payload_json": row["payload_json"]}

    return guard(run)


def f_named_by_owner_event(r):
    c = context(r)
    return guard(lambda: bool(live._named_by_owner_event(c.conn, r.get("row"))))


def f_sweep_select(r):
    c = context(r)

    def run():
        before = sorted(p.name for p in c.blob_root.iterdir())
        c.repo._sweep_orphans("profile")
        after = sorted(p.name for p in c.blob_root.iterdir())
        return {"removed": sorted(set(before) - set(after)), "remaining": after}

    return guard(run)


def f_prune_plan(r):
    c = context(r)

    def run():
        before = {
            str(row["attachment_id"]): str(row["blob_id"])
            for row in c.conn.execute("SELECT attachment_id, blob_id FROM hosted_room_attachments")
        }
        blobs_before = {
            str(row["blob_id"]) for row in c.conn.execute("SELECT blob_id FROM hosted_room_attachment_blobs")
        }
        removed = c.repo._prune("profile", float(r.get("now")))
        rows = [
            {
                "attachment_id": str(row["attachment_id"]),
                "state": str(row["state"]),
                "expires_at": None if row["expires_at"] is None else float(row["expires_at"]),
            }
            for row in c.conn.execute(
                "SELECT attachment_id, state, expires_at FROM hosted_room_attachments"
                " ORDER BY attachment_id"
            )
        ]
        blobs_after = {
            str(row["blob_id"]): float(row["ref_count"])
            for row in c.conn.execute("SELECT blob_id, ref_count FROM hosted_room_attachment_blobs")
        }
        assert removed == len(before) - len(rows), (removed, before, rows)
        return {
            "removed": removed,
            "deleted_blobs": sorted(blobs_before - set(blobs_after)),
            "ref_counts": blobs_after,
            "attachments": rows,
            "files": sorted(p.name for p in c.blob_root.iterdir()),
        }

    return guard(run)


DISPATCH = {
    "metadata": f_metadata,
    "blob_id_ok": f_blob_id_ok,
    "blob_path": f_blob_path,
    "owner_event": f_owner_event,
    "named_by_owner_event": f_named_by_owner_event,
    "sweep_select": f_sweep_select,
    "prune_plan": f_prune_plan,
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
