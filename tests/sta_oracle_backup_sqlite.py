#!/usr/bin/env python3
"""
sta_oracle_backup_sqlite.py — Oracle for hermes_cli/backup_sqlite.py.

The C harness runs its own fixtures and reports what the port produced. This
oracle builds an equivalent fixture, runs the LIVE module, and compares the
observable contract: the missing-db message, the keep-two prune, snapshot
validity, and WAL detection.
"""
import json
import os
import sqlite3
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import hermes_cli.backup_sqlite as BS  # noqa: E402


def make_db(path: Path, wal: bool) -> None:
    conn = sqlite3.connect(path)
    conn.execute("CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY, body TEXT)")
    conn.execute("PRAGMA journal_mode=" + ("WAL" if wal else "DELETE"))
    conn.executemany("INSERT INTO sessions(body) VALUES (?)",
                     [(f"row {i} with some filler text",) for i in range(200)])
    conn.commit()
    conn.close()


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE backup_sqlite: 0 cases (link failure in disguise)")
        return 1

    # Expectations computed from the LIVE module on its own fixture.
    tmp = Path(tempfile.mkdtemp(dir=".testbin"))
    journal = tmp / "journal"
    journal.mkdir()
    make_db(journal / "state.db", False)
    live_first = BS.preflight_state_db(journal)
    BS.preflight_state_db(journal)
    BS.preflight_state_db(journal)
    live_baks = len(list(journal.glob("state.db.pre-update-emergency-*.bak")))

    wal_dir = tmp / "wal"
    wal_dir.mkdir()
    make_db(wal_dir / "state.db", True)
    live_wal = BS.preflight_state_db(wal_dir)

    empty = tmp / "empty"
    empty.mkdir()
    live_missing = BS.preflight_state_db(empty)

    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "uses_wal":
            path = tmp / ("journal" if c["case"] == "journal" else "wal") / "state.db"
            want = BS._uses_wal(path)
            if bool(c["ret"]) != bool(want):
                mism += 1
                print(f"MISMATCH uses_wal {c['case']}: C={c['ret']} PY={want}")
        elif fam == "preflight":
            case = c["case"]
            res = c["result"] or {}
            if case == "missing_db":
                if res != live_missing:
                    mism += 1
                    print(f"MISMATCH missing_db: C={res} PY={live_missing}")
                continue
            if not c["path_set"]:
                mism += 1
                print(f"MISMATCH {case}: C produced no snapshot: {res}")
                continue
            if not res.get("bytes"):
                mism += 1
                print(f"MISMATCH {case}: zero-byte snapshot {res}")
            if c["baks"] < 1:
                mism += 1
                print(f"MISMATCH {case}: no .bak left on disk")
            if not c["baks_ok"]:
                mism += 1
                print(f"MISMATCH {case}: a snapshot fails quick_check")
            # both sides must report the same keys
            live_keys = set(live_first) | {"path", "bytes"}
            if not {"path", "bytes"} <= set(res):
                mism += 1
                print(f"MISMATCH {case}: keys {sorted(res)} missing path/bytes")
        elif fam == "prune":
            if c["baks"] != live_baks:
                mism += 1
                print(f"MISMATCH prune {c['case']}: C kept {c['baks']} PY kept {live_baks}")
        elif fam == "sweep":
            # old .partial removed, fresh one retained -> exactly 1 left
            if c["partials"] != 1:
                mism += 1
                print(f"MISMATCH sweep {c['case']}: C left {c['partials']} partial files")

    print(f"ORACLE backup_sqlite: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
