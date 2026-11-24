#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/write.py (gate w24_write_5b6032).

Code objects arrive as value triples (filename, name, qualname) so the live
formatters can be called without a frame in sight.  ``maintain_after_commit`` is
driven against a stand-in db whose two maintenance hooks only record that they
ran -- no checkpoint, no FTS merge, no connection.  The writer-mutex refusal is
captured by actually timing out a lock that is already held.
"""
import json
import sys
import threading
import time
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_write_5b6032"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._state import write as live  # noqa: E402

WANTED = {"describe_code", "describe_site", "maintenance", "constants"}


def code_of(raw):
    """A code-object stand-in carrying only the attributes the request names."""
    if not isinstance(raw, dict):
        return None
    ns = types.SimpleNamespace()
    for key in ("co_filename", "co_name", "co_qualname"):
        if key in raw:
            setattr(ns, key, raw[key])
    return ns


def describe_site(raw):
    if not isinstance(raw, list):
        return live.describe_site(raw)
    return live.describe_site(tuple(code_of(item) for item in raw))


def maintenance(req):
    class DB:
        pass

    db = DB()
    db._write_count = req.get("write_count") - 1
    db._CHECKPOINT_EVERY_N_WRITES = req.get("checkpoint_every")
    db._FTS_MERGE_EVERY_N_WRITES = req.get("fts_every")
    calls = []
    db._try_wal_checkpoint = lambda: calls.append("checkpoint")
    db._try_incremental_merge_fts = lambda: calls.append("fts")
    try:
        live.maintain_after_commit(db)
    except ZeroDivisionError as exc:
        return {"error": str(exc)}
    return {"checkpoint": "checkpoint" in calls, "fts_merge": "fts" in calls}


def mutex_refusal():
    """Real refusal: a held lock and a deadline already in the past."""
    lock = threading.Lock()
    lock.acquire()
    try:
        with live.writer_mutex_until(lock, time.monotonic() - 1.0):
            pass
    except live.WriterMutexTimeout as exc:
        return str(exc), exc.sqlite_errorcode, exc.sqlite_errorname
    finally:
        lock.release()
    raise AssertionError("writer_mutex_until did not refuse a held lock")


def expected(req):
    func = req.get("func")
    if func == "describe_code":
        return live._describe_code(code_of(req.get("code")))
    if func == "describe_site":
        return describe_site(req.get("site"))
    if func == "maintenance":
        return maintenance(req)
    if func == "constants":
        message, code, name = mutex_refusal()
        return {
            "owner_log_interval_s": float(live._OWNER_LOG_INTERVAL_S),
            "wrapper_names": sorted(live._WRAPPER_NAMES),
            "mutex_message": message,
            "sqlite_busy": float(code),
            "sqlite_busy_name": name,
        }
    raise AssertionError("unknown func %r" % (func,))


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
        try:
            py = expected(req)
        except ValueError as exc:  # unpack errors the live code raises
            py = {"error": str(exc)}
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
