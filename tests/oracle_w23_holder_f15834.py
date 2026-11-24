#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_holder.py (gate w23_holder_f15834).

The live ``StoreHolder`` is imported as a package module (its relative imports
need the package context) and its pure decision methods are called on an
instance made with ``__new__``: no connection, no schema, no thread.  That is
the seam -- the SQLite writer/read pool/accounting thread never starts, and
what is compared is exactly what those values decide:

  * ``_coalesce_token_deltas`` over a batch of (session_id, kwargs) deltas;
  * ``_quarantine_reason`` over a handle's sticky damage flags;
  * the contention-budget constants the same class is tuned with.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w23_holder_f15834"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite import _holder as live  # noqa: E402

HOLDER = live.StoreHolder

WANTED = {"coalesce", "quarantine_reason", "constants"}

# json key -> where the live value lives
CONSTANTS = (
    ("read_open_retry_seconds", (live, "_READ_OPEN_RETRY_SECONDS")),
    ("read_only_ioerr_retry_attempts", (live, "_READ_ONLY_IOERR_RETRY_ATTEMPTS")),
    ("read_only_ioerr_retry_backoff_s", (live, "_READ_ONLY_IOERR_RETRY_BACKOFF_S")),
    ("read_busy_timeout_s", (live, "_READ_BUSY_TIMEOUT_S")),
    ("write_patience_s", (HOLDER, "_WRITE_PATIENCE_S")),
    ("transcript_write_patience_s", (HOLDER, "_TRANSCRIPT_WRITE_PATIENCE_S")),
    ("activity_write_patience_s", (HOLDER, "_ACTIVITY_WRITE_PATIENCE_S")),
    ("compression_busy_wait_s", (HOLDER, "_COMPRESSION_BUSY_WAIT_S")),
    ("write_retry_min_s", (HOLDER, "_WRITE_RETRY_MIN_S")),
    ("write_retry_max_s", (HOLDER, "_WRITE_RETRY_MAX_S")),
    ("write_retry_slow_after_s", (HOLDER, "_WRITE_RETRY_SLOW_AFTER_S")),
    ("write_retry_slow_min_s", (HOLDER, "_WRITE_RETRY_SLOW_MIN_S")),
    ("write_retry_slow_max_s", (HOLDER, "_WRITE_RETRY_SLOW_MAX_S")),
    ("checkpoint_every_n_writes", (HOLDER, "_CHECKPOINT_EVERY_N_WRITES")),
    ("fts_merge_every_n_writes", (HOLDER, "_FTS_MERGE_EVERY_N_WRITES")),
    ("fts_merge_max_pages_per_index", (HOLDER, "_FTS_MERGE_MAX_PAGES_PER_INDEX")),
    ("fts_merge_commands_per_pass", (HOLDER, "_FTS_MERGE_COMMANDS_PER_PASS")),
    ("token_writer_idle_seconds", (HOLDER, "_TOKEN_WRITER_IDLE_SECONDS")),
)


def bare():
    """An instance with no connection: only class constants are consulted."""
    return HOLDER.__new__(HOLDER)


def coalesce(req):
    raw = req.get("batch")
    batch = []
    for entry in raw if isinstance(raw, list) else []:
        if not isinstance(entry, dict):
            continue
        kwargs = entry.get("kwargs")
        batch.append((entry.get("session_id"), dict(kwargs) if isinstance(kwargs, dict) else kwargs))
    out = bare()._coalesce_token_deltas(batch)
    return [{"session_id": sid, "kwargs": kw} for sid, kw in out]


def quarantine_reason(req):
    state = req.get("state")
    state = state if isinstance(state, dict) else {}
    holder = bare()
    holder._db_corrupt = bool(state.get("db_corrupt"))
    reason = state.get("reason")
    holder._db_corrupt_reason = "" if reason is None else str(reason)
    holder._db_replaced = bool(state.get("db_replaced"))
    holder._db_wal_generation_lost = bool(state.get("wal_generation_lost"))
    return holder._quarantine_reason()


def constants():
    return {key: float(getattr(owner, attr)) for key, (owner, attr) in CONSTANTS}


def expected(r):
    f = r.get("func")
    if f == "coalesce":
        return coalesce(r)
    if f == "quarantine_reason":
        return quarantine_reason(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
