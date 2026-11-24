#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/dbfile.py (gate w29_dbfile_fb85d9_p1).

The live module is imported against the quarry tree and its real functions are
called.  Everything that would touch a filesystem, a database or this process's
fd table is a fixture seam, installed here as a shim:

  * ``os.path`` is ``posixpath`` -- the port's semantics, so path derivation is
    compared as POSIX text on any host;
  * ``os.path.realpath`` is an explicit ``{literal: resolved}`` map supplied by
    the case (an unlisted path resolves to itself);
  * ``os.stat`` / ``os.geteuid`` / ``own_descriptors()`` /
    ``_stat_db_file_identity`` / ``_pread_db_header`` answer from the case.

What stays under test is the module's own logic: the header decode, the
sidecar spellings that get watched, the identity verdicts, and the holder-PID
classification.
"""
import errno
import json
import os as _os
import posixpath
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_dbfile_fb85d9_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._state import dbfile as live  # noqa: E402
from persistence.sqlite._state import holders  # noqa: E402

WANTED = {
    "header", "appid", "sidecars", "watched", "unlinked_identity",
    "unlinked_fd", "own_fd", "oplink", "holder_pids", "canon",
}


# ── seams ──────────────────────────────────────────────────────────────────────────────────────
class PathShim:
    """posixpath, except realpath: an exact-match map (the fixture's world)."""

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
    """Real os for everything unused; stat and geteuid answer from the case."""

    def __init__(self, path, stat_map=None, euid=1000):
        self.path = path
        self._stat_map = stat_map or {}
        self._euid = euid

    def stat(self, p, **kwargs):
        entry = self._stat_map.get(str(p))
        if entry is None or isinstance(entry, int):
            raise OSError(entry or errno.ENOENT, "scripted stat failure", str(p))
        return entry

    def geteuid(self):
        return self._euid

    def __getattr__(self, name):
        return getattr(_os, name)


def install(resolve=None, stat_map=None, euid=1000):
    shim = OsShim(PathShim(resolve), stat_map, euid)
    live.os = shim
    holders.os = shim
    return shim


def ident_of(value):
    return None if value is None else tuple(int(v) for v in value)


# ── handlers ───────────────────────────────────────────────────────────────────────────────────
_HEADER = {"value": None}
live._pread_db_header = lambda db_path, length: _HEADER["value"]

_SIDECARS = {"map": {}}
live._stat_db_file_identity = lambda p: _SIDECARS["map"].get(str(p)[-4:])


def h_header(r):
    return live._parse_sqlite_header(bytes.fromhex(r.get("hex") or ""))


def h_appid(r):
    head = r.get("header")
    _HEADER["value"] = None if head is None else bytes.fromhex(head)
    return live._read_sqlite_application_id("/srv/hermes/state.db")


def h_sidecars(r):
    raw = r.get("stat") or {}
    _SIDECARS["map"] = {k: ident_of(v) for k, v in raw.items()}
    got = live._stat_sqlite_sidecar_identity("/srv/hermes/state.db")
    # The JSON wire form of a (st_dev, st_ino) tuple is an array.
    return {suffix: list(ident) for suffix, ident in got.items()}


def h_watched(r):
    install(r.get("resolve") or {})
    return live._watched_sqlite_sidecar_paths(r["path"])


def h_unlinked_identity(r):
    st = r.get("stat")
    install(stat_map={"/w/state.db-wal": Stat(dev=int(st[0]), ino=int(st[1])) if st
                         else errno.ENOENT})
    return live._identity_is_truly_unlinked(tuple(r["identity"]), "/w/state.db-wal")


def h_unlinked_fd(r):
    fd_stat, watched = r["fd_stat"], r.get("watched_stat")
    script = {}
    script["/proc/42/fd/7"] = (Stat(dev=int(fd_stat[0]), ino=int(fd_stat[1]))
                               if isinstance(fd_stat, list) else int(fd_stat.get("errno")))
    script["/w/state.db-wal"] = (Stat(dev=int(watched[0]), ino=int(watched[1])) if watched
                                 else errno.ENOENT)
    install(stat_map=script)
    return live._fd_is_truly_unlinked("/proc/42/fd/7", "/w/state.db-wal")


def h_own_fd(r):
    fds = r.get("fds")
    live.own_descriptors = (lambda: None if fds is None
                            else [(int(fd), tuple(int(v) for v in idt)) for fd, idt in fds])
    return live._own_descriptor_for_identity(ident_of(r.get("identity")))


def h_oplink(r):
    script = {"/h/state.db": Stat(uid=r["target_uid"]) if r.get("target_exists") else errno.ENOENT}
    install(stat_map=script, euid=int(r["euid"]))
    return live._is_operator_store_link("/h/state.db", Stat(uid=int(r["link_uid"])))


def h_holder_pids(r):
    install()
    return live._concrete_state_db_holder_pids(
        r["path"], [(int(pid), path) for pid, path in r.get("holders") or []])


def h_canon(r):
    install()
    return holders.canonical_sqlite_path(r["path"])


HANDLERS = {
    "header": h_header, "appid": h_appid, "sidecars": h_sidecars,
    "watched": h_watched, "unlinked_identity": h_unlinked_identity,
    "unlinked_fd": h_unlinked_fd, "own_fd": h_own_fd, "oplink": h_oplink,
    "holder_pids": h_holder_pids, "canon": h_canon,
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
