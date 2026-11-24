#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/_mutation.py (gate
w23_mutation_6cb83f).

``mutate_runtime_session`` is one closed write: the input checks, the receipt
identity, the epoch fence, the receipt replay, the revision and generation
fences, then ``apply_action`` and the commit arithmetic.  The sqlite handle is
the OS-style seam, so the world the fixture describes is scripted with a real
in-memory database:

  * ``runtime_epoch``/``sessions``/``state_meta`` are real tables, filled from
    the fixture's ``store`` (the epoch singleton, the session row, the stored
    receipt);
  * ``apply_action`` is replaced by the fixture's ``affected``/``projection``
    pair, the (affected rows, projection) it would have returned;
  * ``holder._execute_write`` is the fixture's single transaction.

Everything else -- the ladder, ``parse_mutation``, ``mutation_receipt_key``,
the SELECT/readback arithmetic -- is the live function.

``mutation_receipt_key``, ``parse_mutation`` and ``_GENERATION_REQUIRED`` run
live with no seam at all.
"""
import importlib
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_mutation_6cb83f"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("persistence.sqlite.authority._mutation")
RUNTIME = importlib.import_module("persistence.authority.runtime")
WANTED = {"mutate", "receipt_key", "parse", "require_generation"}


class Holder:
    """The store holder: one supplied connection, one supplied transaction."""

    def __init__(self, conn):
        self.conn = conn

    def _execute_write(self, write):
        return write(self.conn)


def build_store(spec, request):
    """The fixture's row facts as the real tables the write reads."""
    conn = sqlite3.connect(":memory:")
    conn.row_factory = sqlite3.Row
    conn.execute("CREATE TABLE runtime_epoch(singleton INTEGER PRIMARY KEY, epoch INTEGER)")
    conn.execute("CREATE TABLE sessions(id TEXT PRIMARY KEY, runtime_revision INTEGER, "
                 "runtime_generation INTEGER, archived INTEGER)")
    conn.execute("CREATE TABLE state_meta(key TEXT PRIMARY KEY, value TEXT)")
    epoch = spec.get("epoch")
    if epoch is not None:
        conn.execute("INSERT INTO runtime_epoch VALUES(1, ?)", (epoch,))
    row = spec.get("session")
    if isinstance(row, dict):
        conn.execute("INSERT INTO sessions VALUES(?, ?, ?, ?)",
                     (row.get("id", request.get("session_id")),
                      row.get("runtime_revision"),
                      row.get("runtime_generation"),
                      1 if row.get("archived") else 0))
    kind = spec.get("receipt") or "none"
    if kind in {"exact", "conflict"}:
        try:
            key, digest = _live_key(request)
        except Exception:  # the ladder refuses before the receipt is read
            key = digest = None
        if key is not None:
            if kind == "conflict":
                digest = "0" * 64
            conn.execute("INSERT INTO state_meta VALUES(?, ?)",
                         (key, json.dumps({"digest": digest,
                                           "result": spec.get("receipt_result")})))
    return conn


def _live_key(request):
    return live.mutation_receipt_key(
        request.get("principal_id"), request.get("session_id"), request.get("request_id"),
        operation=request.get("operation"), payload=request.get("payload"),
        expected_revision=request.get("expected_revision"),
        expected_generation=request.get("expected_generation"))


def mutate(request):
    spec = request.get("store") or {}
    conn = build_store(spec, request)
    holder = Holder(conn)
    affected = list(spec.get("affected") or [])
    projection = dict(spec.get("projection") or {})

    def fake_apply(_holder, _conn, _session_id, _operation, _payload, **_kw):
        return affected, projection

    saved = live.apply_action
    live.apply_action = fake_apply
    try:
        result, gone, replayed = live.mutate_runtime_session(
            holder,
            epoch=request.get("epoch"),
            principal_id=request.get("principal_id"),
            session_id=request.get("session_id"),
            request_id=request.get("request_id"),
            expected_revision=request.get("expected_revision"),
            operation=request.get("operation"),
            payload=request.get("payload"),
            expected_generation=request.get("expected_generation"))
    except live.AuthorityRefusal as exc:
        return {"status": "refused", "reason": str(exc)}
    finally:
        live.apply_action = saved
    return {"status": "ok", "result": result,
            "gone": None if gone is None else {"session_ids": list(gone.session_ids),
                                               "reason": gone.reason},
            "replayed": replayed}


def receipt_key(request):
    key, digest = _live_key(request)
    return {"key": key, "digest": digest}


def parse(request):
    try:
        live.parse_mutation(request.get("operation"), request.get("payload"))
    except live.AuthorityRefusal:
        return False
    return True


def require_generation(request):
    return request.get("operation") in RUNTIME._GENERATION_REQUIRED


def expected(r):
    f = r.get("func")
    req = r.get("req")
    req = req if isinstance(req, dict) else {}
    if f == "mutate":
        return mutate(req)
    if f == "receipt_key":
        return receipt_key(req)
    if f == "parse":
        return parse(req)
    if f == "require_generation":
        return require_generation(req)
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
