#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/services.py (gate w21_services_87bff3).

The live module imports cleanly against the quarry tree, so the oracle runs its
real `probe_state_db` and its real registered repository:

* the store's situation comes from the fixture -- a `FakePath` answers
  `exists()`, the process-wide latch (`storage_state`) and the latch's verdict
  (`note_storage_error`) are the module's names patched to the fixture's
  answers, the read-only URI comes from `holders.read_only_db_uri` patched to a
  constant, and `connect_tracked` is scripted to succeed or raise;
* `SQLiteStorageHealth` is built through the factory the module registered
  (`_backend._REPOSITORY_FACTORIES["health"]`), so the answers are the live
  class's.

The probe's own ordering (no file before the latch, the latch before the
connection) is the part under test; the seam values are inputs.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "services.py"
GATE = "w21_services_87bff3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("persistence.sqlite.services", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["persistence.sqlite.services"] = live
_spec.loader.exec_module(live)

import hermes_cli.sqlite_safe_read as safe_read  # noqa: E402
import persistence.sqlite._backend as backend  # noqa: E402
import persistence.sqlite._state.holders as holders  # noqa: E402

assert "health" in backend._REPOSITORY_FACTORIES, "services.py did not register the health repository"
_HEALTH_FACTORY = backend._REPOSITORY_FACTORIES["health"]

_REAL = {
    "storage_state": live.storage_state,
    "note_storage_error": live.note_storage_error,
    "connect_tracked": safe_read.connect_tracked,
    "read_only_db_uri": holders.read_only_db_uri,
}

_TYPES = {}


def exc_class(name):
    if not name:
        return sqlite3_error()
    if name not in _TYPES:
        _TYPES[name] = type(name, (Exception,), {})
    return _TYPES[name]


def sqlite3_error():
    import sqlite3

    return sqlite3.DatabaseError


class FakePath:
    def __init__(self, exists):
        self._exists = exists

    def exists(self):
        return self._exists

    def __str__(self):
        return "/store/state.db"


class _Conn:
    """What `connect_tracked` hands back: a connection that is its own context
    manager (as sqlite3's is), so `closing()` can close it."""

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def execute(self, sql):
        return self

    def fetchone(self):
        return None

    def close(self):
        return None


def install(req):
    state = req.get("state") or "ok"
    note = req.get("note") or "none"
    connect = req.get("connect") or "ok"
    exc_type = req.get("exc_type") or ""

    live.storage_state = lambda path: state
    live.note_storage_error = lambda path, exc: note == "latched"
    holders.read_only_db_uri = lambda path: "file:/store/state.db?mode=ro"

    def connect_tracked(*args, **kwargs):
        if connect == "raise":
            raise exc_class(exc_type)("probe failed")
        return _Conn()

    safe_read.connect_tracked = connect_tracked


def restore():
    live.storage_state = _REAL["storage_state"]
    live.note_storage_error = _REAL["note_storage_error"]
    safe_read.connect_tracked = _REAL["connect_tracked"]
    holders.read_only_db_uri = _REAL["read_only_db_uri"]


def expected_probe(req):
    install(req)
    try:
        probe = live.probe_state_db(FakePath(bool(req.get("exists"))))
    finally:
        restore()
    return {"status": probe.status, "detail": probe.detail}


def expected_health_state(req):
    install(req)
    try:
        health = _HEALTH_FACTORY(type("Store", (), {"store_path": FakePath(True)})())
        return health.storage_state().name
    finally:
        restore()


def expected(r):
    f = r.get("func")
    if f == "probe":
        return expected_probe(r)
    if f == "health_state":
        return expected_health_state(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"probe", "health_state"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
