#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/messages.py (gate p3).

Two kinds of pure logic are exercised, both with the SQLite seam canned:

  * the statement text: the read connection is a recorder that answers with the
    rows the request carries, so the SQL string and params the live method
    hands ``execute`` are compared byte-for-byte
    (``latest_dialogue_message``, ``message_storage_state``,
    ``terminal_tool_call_payloads``, ``blocked_tool_result_payloads``,
    ``has_any_message``, ``spillover_reference_rows``) plus the fragment and
    cap they are built from;
  * the read envelopes: the store reads are canned results and the live
    shaping runs over them (``read_session_timeline``, ``read_prompt_page``,
    ``read_around``, ``read_anchored_view``, ``find_pr_url_hits``,
    ``transcript_handoff_rows``, ``active_watermark``).
"""
import contextlib
import dataclasses
import json
import sys
from enum import Enum
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_messages_29cf8f_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import persistence.sqlite.conversation.messages as live  # noqa: E402
from persistence.sqlite.conversation._store.rules import SPILLOVER_SQL_FILTER  # noqa: E402

WANTED = {"sql_latest_dialogue", "sql_message_storage_state", "sql_terminal_payloads", "sql_blocked_payloads",
          "sql_has_any_message", "sql_spillover_reference", "sql_filter", "timeline_page", "prompt_page",
          "anchored_window", "anchored_view", "pr_url_hits", "handoff_map", "watermark"}


class _Cursor:
    def __init__(self, result):
        self._result = result
        self._spent = False

    def fetchone(self):
        return self._result

    def fetchmany(self, count):
        if self._spent:
            return []
        self._spent = True
        return list(self._result or [])

    def fetchall(self):
        return list(self._result or [])


class _Conn:
    def __init__(self, script):
        self._script = list(script or [])
        self.calls = []

    def execute(self, sql, params=None):
        self.calls.append({"sql": sql, "params": None if params is None else list(params)})
        index = len(self.calls) - 1
        return _Cursor(self._script[index] if index < len(self._script) else [])


class _Holder:
    def __init__(self, script):
        self._conn = _Conn(script)

    def _read_ctx_with_timeout(self, timeout):
        conn = self._conn

        @contextlib.contextmanager
        def _cm():
            yield conn

        return _cm()

    def _read_ctx(self):
        return self._read_ctx_with_timeout(None)


class _Store:
    def __init__(self, holder):
        self._holder = holder

    def holder(self):
        return self._holder


@contextlib.contextmanager
def _null_refusals(holder=None, session_id=None):
    yield


def _repo(holder):
    repo = live.SQLiteMessageRepository.__new__(live.SQLiteMessageRepository)
    repo._store = _Store(holder)
    return repo


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


def _run(repo_call, patch=None):
    original = live._refusals
    live._refusals = _null_refusals
    restored = []
    try:
        for owner_name, attr, value in (patch or ()):
            owner = live if owner_name == "live" else getattr(live, owner_name)
            restored.append((owner, attr, getattr(owner, attr)))
            setattr(owner, attr, value)
        return repo_call()
    finally:
        for owner, attr, value in reversed(restored):
            setattr(owner, attr, value)
        live._refusals = original


def sql_latest_dialogue(req):
    holder = _Holder([req.get("row")])
    repo = _repo(holder)

    def call():
        return repo.latest_dialogue_message(req.get("session_id"))

    out = _run(call)
    call_info = holder._conn.calls[0]
    return {"sql": call_info["sql"], "params": call_info["params"] or [], "out": out}


def sql_message_storage_state(req):
    holder = _Holder([req.get("row")])
    repo = _repo(holder)

    def call():
        return repo.message_storage_state(req.get("message_id"))

    _run(call)
    call_info = holder._conn.calls[0]
    return {"sql": call_info["sql"], "params": call_info["params"] or []}


def sql_terminal_payloads(req):
    holder = _Holder([req.get("rows")])
    repo = _repo(holder)

    def call():
        return list(repo.terminal_tool_call_payloads(since_ts=float(req.get("since_ts") or 0.0)))

    out = _run(call)
    call_info = holder._conn.calls[0]
    return {"sql": call_info["sql"], "params": call_info["params"] or [], "out": out}


def sql_blocked_payloads(req):
    holder = _Holder([req.get("rows")])
    repo = _repo(holder)

    def call():
        return list(repo.blocked_tool_result_payloads(since_ts=float(req.get("since_ts") or 0.0)))

    out = _run(call)
    call_info = holder._conn.calls[0]
    return {"sql": call_info["sql"], "params": call_info["params"] or [], "out": out}


def sql_has_any_message(req):
    table = [1] if req.get("table") else None
    any_row = [1] if req.get("any") else None
    holder = _Holder([table, any_row])
    repo = _repo(holder)

    def call():
        return repo.has_any_message()

    out = _run(call)
    return {"statements": [c["sql"] for c in holder._conn.calls], "out": out}


def sql_spillover_reference(req):
    holder = _Holder([req.get("rows")])
    repo = _repo(holder)

    def call():
        return list(repo.spillover_reference_rows())

    out = _run(call)
    call_info = holder._conn.calls[0]
    return {"sql": call_info["sql"], "out": norm(out)}


def sql_filter(_req):
    return {"spillover_filter": SPILLOVER_SQL_FILTER, "spillover_limit": 20001}


def timeline_page(req):
    result = req.get("result") or {}
    repo = _repo(object())
    out = _run(lambda: repo.read_session_timeline(req.get("session_id") or "s1"),
               [("timeline", "get_session_timeline", lambda holder, sid, limit=500, after_row_id=0: result)])
    return norm(out)


def prompt_page(req):
    result = req.get("result")
    repo = _repo(object())
    out = _run(lambda: repo.read_prompt_page(req.get("session_id") or "s1", req.get("row_id") or 1),
               [("timeline", "get_session_messages_around", lambda holder, sid, row_id, limit=120: result)])
    return norm(out)


def anchored_window(req):
    around = req.get("around") or {}
    repo = _repo(object())
    out = _run(lambda: repo.read_around(req.get("session_id") or "s1", req.get("anchor") or 1),
               [("live", "get_messages_around", lambda holder, sid, anchor, window=5: around)])
    return norm(out)


def anchored_view(req):
    view = req.get("view") or {}
    repo = _repo(object())
    out = _run(lambda: repo.read_anchored_view(req.get("session_id") or "s1", req.get("anchor") or 1),
               [("live", "get_anchored_view", lambda holder, sid, anchor, window=5, bookend=3, keep_roles=None: view)])
    return norm(out)


def pr_url_hits(req):
    hits = req.get("hits") or []
    repo = _repo(object())
    out = _run(lambda: repo.find_pr_url_hits(req.get("session_ids") or ["s1"]),
               [("live", "find_pr_url_messages", lambda holder, sids: hits)])
    return norm(out)


def handoff_map(req):
    found = req.get("found") or {}
    converted = {}
    for key, rows in found.items():
        converted[int(key)] = rows
    repo = _repo(object())
    out = _run(lambda: repo.transcript_handoff_rows(req.get("shown_ids") or [], req.get("segment_ids") or []),
               [("stored_reads", "transcript_handoff_rows", lambda holder, shown, segments: converted)])
    return {str(key): norm(rows) for key, rows in out.items()}


def watermark(req):
    last = req.get("last")
    repo = _repo(object())
    out = _run(lambda: repo.active_watermark(req.get("session_id") or "s1"),
               [("live", "get_active_message_watermark", lambda holder, sid: last)])
    return norm(out)


def expected(r):
    f = r.get("func")
    handlers = {
        "sql_latest_dialogue": sql_latest_dialogue,
        "sql_message_storage_state": sql_message_storage_state,
        "sql_terminal_payloads": sql_terminal_payloads,
        "sql_blocked_payloads": sql_blocked_payloads,
        "sql_has_any_message": sql_has_any_message,
        "sql_spillover_reference": sql_spillover_reference,
        "sql_filter": sql_filter,
        "timeline_page": timeline_page,
        "prompt_page": prompt_page,
        "anchored_window": anchored_window,
        "anchored_view": anchored_view,
        "pr_url_hits": pr_url_hits,
        "handoff_map": handoff_map,
        "watermark": watermark,
    }
    if f not in handlers:
        raise AssertionError("unknown func %r" % (f,))
    return handlers[f](r)


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
