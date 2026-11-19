#!/usr/bin/env python3
"""Oracle for persistence.sqlite._state.errors.py pure defs: the classification
predicates (_is_no_more_rows, is_transient_sqlite_error,
is_malformed_schema_error, classify_persistence_error) and the message
formatting (CompressionSessionClosedError.__init__ + the FATAL constants).

The harness (t_port_state_errors.c) models exception types with a "kind" tag;
this side rebuilds the LIVE exception instance (sqlite3 classes, the backend
contract types, plain strings, None) and compares verdicts / exact strings.
"""
import json
import os
import sqlite3
import sys

for _quarry in ("C:/Users/eman5/hermes-agent", os.path.expanduser("~/hermes-agent")):
    if os.path.isdir(os.path.join(_quarry, "persistence")):
        sys.path.insert(0, _quarry)
        break

try:
    from persistence.sqlite._state.errors import (                      # noqa: E402
        CompressionSessionBusyError, CompressionSessionClosedError,
        DeletedWalGenerationError, SessionCompressionInProgressError,
        SessionTurnLeaseLostError, StateDbCorruptError, StateDbReplacedError,
        _DELETED_WAL_GENERATION_MSG, _STATE_DB_CORRUPT_MSG,
        _STATE_DB_REPLACED_MSG, _is_no_more_rows,
        classify_persistence_error, is_malformed_schema_error,
        is_transient_sqlite_error,
    )
except ImportError:
    # The suite interpreter is the quarry venv; re-exec under it when the
    # current interpreter lacks the quarry import chain (stdin survives execv).
    _venv = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"
    if not os.path.isfile(_venv):
        _venv = os.path.expanduser("~/hermes-agent/.venv/bin/python")
    if os.path.isfile(_venv) and \
            os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

_KIND_CLS = {
    "operational": sqlite3.OperationalError,
    "database": sqlite3.DatabaseError,
    "other": RuntimeError,
    "turn_lease": SessionTurnLeaseLostError,
    "compression_closed": CompressionSessionClosedError,
    "compression_busy": CompressionSessionBusyError,
    "compression_in_progress": SessionCompressionInProgressError,
    "deleted_wal": DeletedWalGenerationError,
    "replaced": StateDbReplacedError,
    "corrupt": StateDbCorruptError,
}


def make_exc(i):
    kind = i.get("kind", "string")
    msg = i.get("message")
    if kind == "none":
        return None
    if kind == "string":
        return msg if msg is not None else ""
    exc = _KIND_CLS[kind](msg if msg is not None else "")
    if i.get("code") is not None:
        exc.sqlite_errorcode = i["code"]
    return exc


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "_is_no_more_rows":
                want = bool(_is_no_more_rows(make_exc(i)))
            elif fn == "is_transient_sqlite_error":
                want = bool(is_transient_sqlite_error(make_exc(i)))
            elif fn == "is_malformed_schema_error":
                want = bool(is_malformed_schema_error(make_exc(i)))
            elif fn == "classify_persistence_error":
                want = classify_persistence_error(make_exc(i))
            elif fn == "compression_session_closed_message":
                want = str(CompressionSessionClosedError(i.get("session_id")))
            elif fn == "state_db_replaced_message":
                want = _STATE_DB_REPLACED_MSG
            elif fn == "deleted_wal_generation_message":
                want = _DELETED_WAL_GENERATION_MSG
            elif fn == "state_db_corrupt_message":
                want = _STATE_DB_CORRUPT_MSG
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:200]} "
                  f"PY={want}")
    if total == 0:
        print("ORACLE state_errors: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE state_errors: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
