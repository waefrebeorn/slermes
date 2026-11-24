#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_backup.py (gate w32_backup_1d0d4e_p2).

Exercised against the LIVE module:

  * the module/rollback budget constants (module + class attributes);
  * ``_LEDGER_PROBE`` verbatim;
  * the two statements of ``_restore_epoch_source``, read out of the live
    function's own source text so the bytes cannot drift from it;
  * ``_uses_wal`` over real header files;
  * ``verify_sqlite_integrity`` over real files: a missing path, a sub-minimum
    file, a wrong magic, a valid database, and a valid database above
    ``max_bytes`` so the O(1) header+schema probe is taken;
  * the runtime-epoch staging decision, with the source epoch read through the
    live ``_restore_epoch`` on a real database.

All filesystem/sqlite effects live here; the C side receives the same observed
values and must shape the identical manifest.
"""
import inspect
import json
import os
import re
import sqlite3
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_backup_1d0d4e_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.admin import _backup as live  # noqa: E402

WANTED = {"constants", "ledger_probe", "epoch_sql", "uses_wal", "verify", "epoch_decision"}


def constants():
    return {
        "sqlite_header_hex": live._SQLITE_HEADER.hex(),
        "integrity_max_bytes": float(live.DEFAULT_INTEGRITY_CHECK_MAX_BYTES),
        "abandoned_staging_seconds": float(live._ABANDONED_STAGING_SECONDS),
        "rollback_step_seconds": float(live._ROLLBACK_STEP_SECONDS),
        "rollback_first_step_pages": float(live._ROLLBACK_FIRST_STEP_PAGES),
        "rollback_busy_sleep_seconds": float(live._ROLLBACK_BUSY_SLEEP_SECONDS),
        "rollback_max_restarts": float(live._ROLLBACK_MAX_RESTARTS),
        "rollback_lock_hold_seconds": float(live._ROLLBACK_LOCK_HOLD_SECONDS),
    }


def ledger_probe():
    return {"probe": live._LEDGER_PROBE}


def epoch_sql():
    source = inspect.getsource(live._restore_epoch_source)
    blocks = re.findall(r'"""(.*?)"""', source, re.S)
    ddl = next(b for b in blocks if "CREATE TABLE" in b)
    upsert = next(b for b in blocks if "INSERT INTO" in b)
    return {"epoch_ddl": ddl, "epoch_upsert": upsert}


def uses_wal(req):
    blob = bytes.fromhex(req.get("header_hex") or "")
    with tempfile.TemporaryDirectory() as work:
        path = Path(work) / "h.db"
        path.write_bytes(blob)
        return {"wal": bool(live._uses_wal(path))}


def _make_sqlite(path):
    conn = sqlite3.connect(str(path))
    conn.execute("CREATE TABLE IF NOT EXISTS t(x)")
    conn.commit()
    conn.close()


def verify(req):
    spec = req.get("file") or {}
    kind = spec.get("kind", "missing")
    previous = os.getcwd()
    with tempfile.TemporaryDirectory() as work:
        os.chdir(work)
        try:
            path = Path(req["path"])
            if kind == "tiny":
                path.write_bytes(b"x" * int(spec["size"]))
            elif kind == "bytes":
                blob = bytes.fromhex(spec.get("header_hex") or "")
                if len(blob) < int(spec["size"]):
                    blob += b"\x00" * (int(spec["size"]) - len(blob))
                path.write_bytes(blob)
            elif kind == "sqlite":
                _make_sqlite(path)
                actual = path.stat().st_size
                if actual != int(spec["size"]):
                    raise AssertionError("sqlite size %d != fixture %r" % (actual, spec["size"]))
            return live.verify_sqlite_integrity(
                path,
                check_header=bool(req.get("check_header", True)),
                run_pragma=bool(req.get("run_pragma", True)),
                max_bytes=int(req.get("max_bytes", live.DEFAULT_INTEGRITY_CHECK_MAX_BYTES)),
            )
        finally:
            os.chdir(previous)


def epoch_decision(req):
    dst_name = req.get("dst_name", "")
    dst_resolved_name = req.get("dst_resolved_name", "")
    src_epoch = int(req.get("src_epoch", 0))
    with tempfile.TemporaryDirectory() as work:
        path = Path(work) / "snapshot.db"
        conn = sqlite3.connect(str(path))
        conn.execute("CREATE TABLE IF NOT EXISTS runtime_epoch ("
                     "singleton INTEGER PRIMARY KEY CHECK (singleton = 1), "
                     "epoch INTEGER NOT NULL CHECK (epoch > 0), instance_id TEXT NOT NULL)")
        conn.execute("INSERT OR REPLACE INTO runtime_epoch VALUES(1, ?, 'fixture')", (src_epoch,))
        conn.commit()
        conn.close()
        observed = live._restore_epoch(path)
    if observed != src_epoch:
        raise AssertionError("live _restore_epoch %r != fixture %r" % (observed, src_epoch))
    is_state = dst_name == "state.db" or dst_resolved_name == "state.db"
    stage = is_state and int(req.get("floor", 0)) > observed
    return {"stage": stage,
            "action": "stage" if stage else "passthrough",
            "work_prefix": ".restore-epoch-",
            "prepared": "state.db"}


def expected(r):
    func = r.get("func")
    if func == "constants":
        return constants()
    if func == "ledger_probe":
        return ledger_probe()
    if func == "epoch_sql":
        return epoch_sql()
    if func == "uses_wal":
        return uses_wal(r)
    if func == "verify":
        return verify(r)
    if func == "epoch_decision":
        return epoch_decision(r)
    raise AssertionError("unknown func %r" % (func,))


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
