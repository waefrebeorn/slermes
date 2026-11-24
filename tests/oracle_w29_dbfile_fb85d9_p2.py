#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/dbfile.py (gate w29_dbfile_fb85d9_p2).

The live module is imported against the quarry tree and its real functions run.
Each case supplies the seam it needs:

  * ``_preopen_header`` / ``is_zeroed_sqlite_file`` / ``has_invalid_sqlite_header_preopen``
    answer from the fixture (the byte probe is a file seam);
  * quarantine runs on a REAL temporary directory, with the clock
    (``time.strftime``), the pid (``os.getpid``) and the cross-process lock
    scripted -- so the real collision loop and rename are what is measured;
  * the read-only stats snapshot runs against a scripted connection: every
    ``execute`` is recorded (text and bound parameters) and answered from the
    fixture, so the statement construction AND the derived fields are compared;
  * holder counting gets the process and descriptor listings from the fixture,
    and the platform (``sys.platform``) as a case input.
"""
import json
import os as _os
import posixpath
import shutil
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_dbfile_fb85d9_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._state import dbfile as live  # noqa: E402

WANTED = {
    "invalid_header", "zeroed", "quarantine", "stats",
    "count_holders", "orphan", "constants",
}

# Everything a handler may rebind on the live module.  Cases must not leak a
# previous case's answers into the next one.
_PATCHED = (
    "_preopen_header", "is_zeroed_sqlite_file", "is_zeroed_state_db",
    "has_invalid_sqlite_header_preopen",
    "_connect_tracked_db", "_iter_proc_fd_targets", "_iter_darwin_fd_targets",
    "quarantine_cross_process_lock", "time", "os", "sys",
)

_ORIGINAL = {name: getattr(live, name) for name in _PATCHED}


def reset():
    """Re-bind every seam the live module exposes to its import-time value."""
    for name, value in _ORIGINAL.items():
        setattr(live, name, value)


# ── seams ──────────────────────────────────────────────────────────────────────────────────────
class PathShim:
    def __init__(self, resolve=None):
        self._resolve = resolve or {}

    def __getattr__(self, name):
        return getattr(posixpath, name)

    def realpath(self, path, **kwargs):
        key = str(path)
        return self._resolve.get(key, key)


class Stat:
    __slots__ = ("st_uid", "st_dev", "st_ino", "st_size", "st_mode")

    def __init__(self, uid=0, dev=0, ino=0, size=0, mode=0o100644):
        self.st_uid, self.st_dev, self.st_ino = uid, dev, ino
        self.st_size, self.st_mode = size, mode


class OsShim:
    def __init__(self, path=None, stat_map=None, getpid=None):
        self.path = path or PathShim({})
        self._stat_map = stat_map or {}
        self._getpid = getpid

    def stat(self, p, **kwargs):
        entry = self._stat_map.get(str(p))
        if entry is None:
            raise OSError(2, "scripted stat failure", str(p))
        return entry

    def getpid(self):
        if self._getpid is None:
            return _os.getpid()
        return self._getpid() if callable(self._getpid) else int(self._getpid)

    def __getattr__(self, name):
        return getattr(_os, name)


class SysShim:
    def __init__(self, platform):
        self.platform = platform

    def __getattr__(self, name):
        return getattr(sys, name)


class TimeShim:
    def __init__(self, ts):
        self._ts = ts

    def strftime(self, fmt, *args):
        return self._ts

    def __getattr__(self, name):
        import time as _t
        return getattr(_t, name)


class ScriptError(Exception):
    pass


class FakeResult:
    def __init__(self, answer):
        self._a = answer

    def fetchone(self):
        a = self._a
        if a.get("none"):
            return None
        if "one" in a:
            return (a["one"],)
        if "all" in a:
            rows = a["all"] or []
            return tuple(rows[0]) if rows else None
        return None

    def fetchall(self):
        a = self._a
        if a.get("none"):
            return []
        if "all" in a:
            return [tuple(r) for r in a["all"]]
        if "one" in a:
            return [(a["one"],)]
        return []


class FakeConn:
    """Records every statement (text + bound params) and answers from the case."""

    def __init__(self, answers):
        self._answers = answers
        self.log = []

    def execute(self, sql, params=()):
        plist = [p for p in params]
        self.log.append({"sql": sql, "params": plist})
        for a in self._answers:
            if a.get("sql") != sql:
                continue
            if [str(x) for x in (a.get("params") or [])] != [str(x) for x in plist]:
                continue
            return FakeResult(a)
        raise ScriptError("unscripted statement")

    def cursor(self):
        return object()

    def close(self):
        pass


def head_bytes(value):
    return None if value is None else bytes.fromhex(value)


_live_connect = live._connect_tracked_db


def h_invalid_header(r):
    head = head_bytes(r.get("head"))
    live._preopen_header = lambda path, probe_bytes, force: head
    return live.has_invalid_sqlite_header_preopen("/srv/hermes/state.db")


def h_zeroed(r):
    head = head_bytes(r.get("head"))
    probe = r.get("probe") or {}
    live._preopen_header = lambda path, probe_bytes, force: head

    def is_zeroed(path, **kwargs):
        if probe.get("raises"):
            raise RuntimeError("zeroed probe unavailable")
        return bool(probe.get("answer"))

    live.is_zeroed_sqlite_file = is_zeroed
    return live.is_zeroed_state_db("/srv/hermes/state.db")


class _Lock:
    def __init__(self, acquired):
        self._acquired = acquired

    def __call__(self, *args, **kwargs):
        import contextlib

        @contextlib.contextmanager
        def _cm():
            yield self._acquired
        return _cm()


def h_quarantine(r):
    tmp = Path(tempfile.mkdtemp(prefix="q29_"))
    try:
        name = r["name"]
        db = tmp / name
        if r.get("exists"):
            db.write_bytes(b"not a database")
        for taken in r.get("taken") or []:
            (tmp / taken).write_bytes(b"older")
        live._preopen_header = lambda path, probe_bytes, force: b"\x00" * 100
        live.has_invalid_sqlite_header_preopen = lambda path, **kw: bool(r.get("invalid"))
        live.is_zeroed_state_db = lambda path, **kw: bool(r.get("zeroed"))
        live.time = TimeShim(r["ts"])
        live.os = OsShim(getpid=int(r["pid"]))
        live.quarantine_cross_process_lock = _Lock(bool(r.get("locked", True)))
        dest = live.quarantine_invalid_state_db(db)
        return None if dest is None else Path(dest).name
    finally:
        live.os = _os
        shutil.rmtree(tmp, ignore_errors=True)


def h_stats(r):
    tmp = Path(tempfile.mkdtemp(prefix="s29_"))
    try:
        db = Path(tmp) / "state.db"
        db.write_bytes(b"")
        if r.get("wal_size") is not None:
            Path(str(db) + "-wal").write_bytes(b"x" * int(r["wal_size"]))
        conn = FakeConn(r.get("answers") or [])
        if (r.get("connect") or "ok") == "raise":
            def boom(*args, **kwargs):
                raise OSError(14, "unable to open database file")

            live._connect_tracked_db = boom
        else:
            live._connect_tracked_db = lambda *args, **kwargs: conn
        mod = types.ModuleType("persistence.sqlite._schema_ddl")

        def drift(cursor):
            flag = r.get("drift", False)
            if flag == "raise":
                raise RuntimeError("drift probe failed")
            return bool(flag)

        mod.fts_layout_drift = drift
        sys.modules["persistence.sqlite._schema_ddl"] = mod
        stats = live.collect_state_db_stats(db)
        return {"stats": stats, "sql": conn.log}
    finally:
        live._connect_tracked_db = _live_connect
        sys.modules.pop("persistence.sqlite._schema_ddl", None)
        shutil.rmtree(tmp, ignore_errors=True)


def h_count_holders(r):
    platform = r.get("platform") or "linux"
    live.sys = SysShim(platform)
    resolve = r.get("resolve") or {}
    target = resolve.get(r["path"], r["path"])
    st = r.get("stat")
    live.os = OsShim(PathShim(resolve),
                     {target: Stat(dev=int(st[0]), ino=int(st[1]))} if st else {})
    try:
        if platform == "darwin":
            fds = [(int(e["pid"]), int(e.get("fd", 0)), e.get("path", ""), tuple(int(v) for v in e["identity"]))
                   for e in r.get("fds") or []]
            live._iter_darwin_fd_targets = lambda: iter(fds)
        else:
            fds = [(int(e["pid"]), e.get("link", ""), e.get("fd_path", ""))
                   for e in r.get("fds") or []]
            live._iter_proc_fd_targets = lambda: iter(fds)
        return live.count_db_holders(r["path"])
    finally:
        live.sys = sys
        live.os = _os


def h_orphan(r):
    return live._is_inactive_orphan_desktop_holder(
        ppid=int(r["ppid"]), age_seconds=float(r["age_seconds"]),
        min_age_seconds=float(r["min_age_seconds"]),
        ephemeral_backend=bool(r.get("ephemeral_backend")),
        connection_statuses=list(r.get("connection_statuses") or []))


CONSTANTS = (
    ("header_probe_lock_timeout_s", "_HEADER_PROBE_LOCK_TIMEOUT"),
    ("retired_generation_main_image_max_bytes", "RETIRED_GENERATION_MAIN_IMAGE_MAX_BYTES"),
    ("retired_generation_manifest_version", "RETIRED_GENERATION_MANIFEST_VERSION"),
    ("retired_generation_dir_suffix", "RETIRED_GENERATION_DIR_SUFFIX"),
    ("retired_generation_manifest", "RETIRED_GENERATION_MANIFEST"),
    ("capture_chunk_bytes", "_CAPTURE_CHUNK_BYTES"),
    ("sqlite_header_bytes", "_SQLITE_HEADER_BYTES"),
    ("fts_table_names", "_FTS_TABLE_NAMES"),
)


def h_constants(r):
    # Tuples travel as JSON arrays.
    return {key: list(getattr(live, attr)) if isinstance(getattr(live, attr), tuple)
            else getattr(live, attr) for key, attr in CONSTANTS}


HANDLERS = {
    "invalid_header": h_invalid_header, "zeroed": h_zeroed, "quarantine": h_quarantine,
    "stats": h_stats, "count_holders": h_count_holders, "orphan": h_orphan,
    "constants": h_constants,
}


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
            reset()
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
