#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/messages.py (gate p2).

Only the module's pure write-path decisions are exercised. The SQLite seams are
canned: ``StoreHolder`` is an object with the patience constant, the
transaction runner executes the batch closure against ``None``, the store's
writers/readers are recording stubs and the read connection answers with the
row the request carries. What is left running is the live decision:

  * ``_message_record``       a stored row as its typed record;
  * ``_stamp``                the row state a write left on one dict;
  * ``_append_chunk``         the restored-but-unwritten row repair in the batch;
  * ``_row_outcomes``         fresh/adopted/repaired from the write's own ids;
  * ``append_batch``          the chunk slicing (and its tool-uid pre-pass);
  * ``replace_transcript``    the kept-row rule and per-row stamps;
  * ``set_reaction`` / ``take_unseen_reactions``  the reaction row shaping;
  * ``read_compression_cooldown_snapshot`` / ``read_compression_cooldown``;
  * ``message_storage_state``.
"""
import contextlib
import dataclasses
import json
import sys
from enum import Enum
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_messages_29cf8f_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import persistence.sqlite.conversation.messages as live  # noqa: E402
from persistence.conversation.messages import CooldownSnapshot, NewMessage, ReplaceMode  # noqa: E402

WANTED = {"message_record", "stamp", "append_chunk", "row_outcomes", "batch_chunks", "replace_rows",
          "reactions", "unseen_reactions", "cooldown_snapshot", "compression_cooldown", "storage_state"}

_NEW_TUPLES = ("tool_calls", "absorbed_row_ids", "data_use", "absorbed_message_uids")


class _Store:
    def __init__(self, holder=None):
        self._holder = holder if holder is not None else object()

    def holder(self):
        return self._holder


def _repo(holder=None):
    repo = live.SQLiteMessageRepository.__new__(live.SQLiteMessageRepository)
    repo._store = _Store(holder)
    return repo


@contextlib.contextmanager
def _null_refusals(holder=None, session_id=None):
    yield


def new_message(raw):
    fields = {}
    for key, item in (raw or {}).items():
        if key in _NEW_TUPLES and isinstance(item, list):
            item = tuple(item)
        fields[key] = item
    return NewMessage(**fields)


def norm(value):
    if dataclasses.is_dataclass(value):
        value = dataclasses.asdict(value)
    if isinstance(value, Enum):
        return value.value
    if isinstance(value, dict):
        return {key: norm(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [norm(item) for item in value]
    return value


def _stored_map(raw):
    out = {}
    for key, row in (raw or {}).items():
        out[int(key)] = row
    return out


def message_record(req):
    return norm(live._message_record(req.get("row") or {}))


def stamp(req):
    return norm(live._stamp(req.get("msg") or {}, _stored_map(req.get("stored"))))


def append_chunk(req):
    holder = SimpleNamespace(_TRANSCRIPT_WRITE_PATIENCE_S=1.0)
    messages = [dict(m) for m in (req.get("messages") or [])]
    inserted = int(req.get("inserted") or 0)
    active = req.get("active_row")
    original = (live._append_messages_in_transaction, live._execute_transcript_write,
                live._active_logical_message_row, live._restore_row_identity, live.transcript_row_snapshot)
    seen = {}
    try:
        live._append_messages_in_transaction = lambda h, conn, sid, msgs, **guards: inserted
        live._execute_transcript_write = lambda h, do, msgs, patience_s=None: do(None)
        live._active_logical_message_row = lambda conn, sid, role, uid: active
        live._restore_row_identity = lambda row, msg: seen.setdefault("restored", row.get("id"))
        live.transcript_row_snapshot = lambda row: row.get("snapshot")
        got = live._append_chunk(holder, "s1", messages)
    finally:
        (live._append_messages_in_transaction, live._execute_transcript_write,
         live._active_logical_message_row, live._restore_row_identity, live.transcript_row_snapshot) = original
    return {"inserted": got, "messages": norm(messages), "restored": seen.get("restored")}


def row_outcomes(req):
    return norm(list(live._row_outcomes(list(req.get("rows") or []), int(req.get("count") or 0))))


def batch_chunks(req):
    holder = object()
    repo = _repo(holder)
    messages = [new_message(m) for m in (req.get("messages") or [])]
    chunk_rows = req.get("chunk_rows")
    chunks, stamped = [], []
    original = (live._refusals, live._append_chunk, live._stamp_tool_call_uids, live._parse_tool_calls)
    try:
        live._refusals = _null_refusals
        live._parse_tool_calls = lambda raw: list(raw or [])

        def _stamp_tool_call_uids(msg, calls, index):
            stamped.append(msg.get("content"))

        def _append_chunk(holder_, session_id, chunk, **guards):
            for i, msg in enumerate(chunk):
                msg.setdefault("_row_id", 100 + i)
                msg.setdefault("timestamp", 1.0)
            chunks.append([dict(msg) for msg in chunk])
            return 0

        live._stamp_tool_call_uids = _stamp_tool_call_uids
        live._append_chunk = _append_chunk
        result = repo.append_batch("s1", messages, chunk_rows=chunk_rows)
    finally:
        (live._refusals, live._append_chunk, live._stamp_tool_call_uids, live._parse_tool_calls) = original
    return {"stamped": stamped, "chunks": chunks, "outcomes": [o.disposition.value for o in result.outcomes]}


def replace_rows(req):
    repo = _repo(object())
    messages = [new_message(m) for m in (req.get("messages") or [])]
    mode = {"replace": ReplaceMode.DESTRUCTIVE, "destructive": ReplaceMode.DESTRUCTIVE,
            "preserve_existing_archive": ReplaceMode.PRESERVE_EXISTING_ARCHIVE,
            "archive_dropped": ReplaceMode.ARCHIVE_DROPPED}[req.get("mode") or "replace"]
    stored = list(req.get("stored") or [])
    row_ids = req.get("row_ids") or {}
    calls = {"replace": 0, "loaded": 0}

    def _replace(holder, sid, dicts, **kw):
        calls["replace"] = {"active_only": kw.get("active_only"), "archive_dropped": kw.get("archive_dropped"),
                            "reject_active_turn_lease": kw.get("reject_active_turn_lease")}
        for i, msg in enumerate(dicts):
            msg["_row_id"] = int(row_ids.get(str(msg.get("content")), 900 + i))

    original = (live._refusals, live.replace_messages, live.get_messages)
    try:
        live._refusals = _null_refusals
        live.replace_messages = _replace
        live.get_messages = lambda holder, sid: (calls.__setitem__("loaded", 1), stored)[1]
        result = repo.replace_transcript("s1", messages, mode,
                                         reject_active_turn_lease=bool(req.get("reject_active_turn_lease")))
    finally:
        live._refusals, live.replace_messages, live.get_messages = original
    return {"rows": norm(result.rows), "replace": calls["replace"], "loaded_stored": calls["loaded"]}


def reactions(req):
    repo = _repo(object())
    rows = req.get("reactions")
    original = live.set_message_reaction
    try:
        live.set_message_reaction = lambda holder, sid, mid, emoji, author=None: rows
        out = repo.set_reaction("s1", 5, req.get("emoji"), author=req.get("author") or "user")
    finally:
        live.set_message_reaction = original
    return norm(out)


def unseen_reactions(req):
    repo = _repo(object())
    rows = list(req.get("pending") or [])
    original = live.take_unseen_reactions
    try:
        live.take_unseen_reactions = lambda holder, sid, author=None: rows
        out = repo.take_unseen_reactions("s1", author=req.get("author") or "user")
    finally:
        live.take_unseen_reactions = original
    return norm(out)


def cooldown_snapshot(req):
    repo = _repo(object())
    row = req.get("row") or {}
    original = live.get_compression_failure_cooldown_row
    try:
        live.get_compression_failure_cooldown_row = lambda holder, sid: row
        out = repo.read_compression_cooldown_snapshot("s1")
    finally:
        live.get_compression_failure_cooldown_row = original
    return norm(out)


def compression_cooldown(req):
    repo = _repo(object())
    snapshot = CooldownSnapshot(**(req.get("snapshot") or {"present": False}))
    repo.read_compression_cooldown_snapshot = lambda sid: snapshot
    return norm(repo.read_compression_cooldown("s1", float(req.get("at") or 0.0)))


class _Conn:
    def __init__(self, row):
        self._row = row

    def execute(self, sql, params=None):
        return self

    def fetchone(self):
        return self._row


class _Holder:
    def __init__(self, row):
        self._row = row

    def _read_ctx_with_timeout(self, timeout):
        row = self._row

        @contextlib.contextmanager
        def _cm():
            yield _Conn(row)

        return _cm()


def storage_state(req):
    repo = _repo(_Holder(req.get("row")))
    return norm(repo.message_storage_state(req.get("message_id")))


def expected(r):
    f = r.get("func")
    if f == "message_record":
        return message_record(r)
    if f == "stamp":
        return stamp(r)
    if f == "append_chunk":
        return append_chunk(r)
    if f == "row_outcomes":
        return row_outcomes(r)
    if f == "batch_chunks":
        return batch_chunks(r)
    if f == "replace_rows":
        return replace_rows(r)
    if f == "reactions":
        return reactions(r)
    if f == "unseen_reactions":
        return unseen_reactions(r)
    if f == "cooldown_snapshot":
        return cooldown_snapshot(r)
    if f == "compression_cooldown":
        return compression_cooldown(r)
    if f == "storage_state":
        return storage_state(r)
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
            py = norm(expected(r))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = norm(r.get("out"))
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
