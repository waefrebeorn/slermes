#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/repair.py (gate w28_repair_1f5388_p1).

repair.py is an I/O module, so every sqlite connection, flock, stat() and
disk_usage() is a seam.  The oracle injects exactly what those seams return --
a FakePath stand-in for pathlib.Path (name/with_name/parent/stat) and a patched
``_disk_budget`` -- and then calls the LIVE module, so what is compared is the
real budget layer: the pinned constants, the volatile-header mask, the free
space headroom arithmetic, the three disk-guard verdicts with their exact
operator text, the snapshot timeout, and the two failure classifiers.
"""
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_repair_1f5388_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._state import repair as live  # noqa: E402

WANTED = {
    "constants", "sidecars", "mask_header", "headroom", "snapshot_timeout",
    "disk_budget_error", "scratch_space_error", "backup_free_space_error",
    "manual_hint", "failure_consumes_attempt", "temporary_file_failure",
    "schema_not_built",
}


def split_path(p):
    for i in range(len(p) - 1, -1, -1):
        if p[i] in "/\\":
            return p[: i + 1], p[i + 1:]
    return "", p


class FakePath:
    """The slice of pathlib.Path repair.py touches, driven by fixtures."""

    def __init__(self, path, sizes=None, exc=None, parent=None):
        self.path = path
        self.sizes = dict(sizes or {})
        self.exc = exc
        self._parent = parent

    def __str__(self):
        return self.path

    def __fspath__(self):
        return self.path

    @property
    def name(self):
        return split_path(self.path)[1]

    @property
    def parent(self):
        if self._parent is not None:
            return FakePath(self._parent)
        return FakePath(split_path(self.path)[0].rstrip("/\\") or ".")

    def with_name(self, new):
        return FakePath(split_path(self.path)[0] + new, self.sizes, self.exc)

    def stat(self):
        if self.exc is not None:
            raise OSError(self.exc)
        if self.path not in self.sizes:
            raise FileNotFoundError(2, "No such file", self.path)
        return type("Stat", (), {"st_size": self.sizes[self.path]})()

    def exists(self):
        return self.path in self.sizes


class Patch:
    def __init__(self, obj, attr, value):
        self.obj, self.attr, self.value, self.saved = obj, attr, value, None

    def __enter__(self):
        self.saved = getattr(self.obj, self.attr)
        setattr(self.obj, self.attr, self.value)
        return self

    def __exit__(self, *exc):
        setattr(self.obj, self.attr, self.saved)
        return False


def constants(_r):
    return {
        "repair_lock_timeout_seconds": float(live._REPAIR_LOCK_TIMEOUT_SECONDS),
        "repair_lock_poll_seconds": float(live._REPAIR_LOCK_POLL_SECONDS),
        "snapshot_min_throughput_bytes_per_second":
            float(live._REPAIR_SNAPSHOT_MIN_THROUGHPUT_BYTES_PER_SECOND),
        "max_persistent_repair_attempts": float(live._MAX_PERSISTENT_REPAIR_ATTEMPTS),
        "max_malformed_backups": float(live._MAX_MALFORMED_BACKUPS),
        "db_sidecar_suffixes": list(live._DB_SIDECAR_SUFFIXES),
        "fingerprint_sample_bytes": float(live._FINGERPRINT_SAMPLE_BYTES),
        "fingerprint_volatile_header_ranges": [list(r) for r in live._FINGERPRINT_VOLATILE_HEADER_RANGES],
        "repair_backup_min_free_bytes": float(live._REPAIR_BACKUP_MIN_FREE_BYTES),
        "repair_backup_free_fraction": float(live._REPAIR_BACKUP_FREE_FRACTION),
        "fts_tables": list(live._FTS_TABLES),
        "temporary_file_failure_codes": sorted(float(x) for x in live._TEMPORARY_FILE_FAILURE_CODES),
    }


def sidecars(r):
    db = FakePath(r.get("db_path") or "")
    return [str(p) for p in live._sidecars(db)]


def mask_header(r):
    return live._mask_volatile_header(bytes.fromhex(r.get("head_hex") or "")).hex()


def headroom(r):
    return float(live._repair_backup_headroom_bytes(int(r.get("total_bytes") or 0)))


def snapshot_timeout(r):
    db = r.get("db_path") or "state.db"
    sizes = {db: float(r.get("db_bytes") or 0)}
    raw = r.get("sidecar_bytes") or []
    suffixes = list(live._DB_SIDECAR_SUFFIXES)
    for i, size in enumerate(raw):
        if i < len(suffixes):
            sizes[db + suffixes[i]] = float(size)
    return float(live._repair_snapshot_timeout_seconds(FakePath(db, sizes=sizes)))


def disk_budget_error(r):
    db = FakePath(r.get("db_path") or "state.db", exc=r.get("exc") or "", parent=r.get("parent"))
    return live._repair_scratch_space_error(db)


def _budget(snapshot, need, free, headroom_b):
    return lambda db_path, refusal: (None, need, free, headroom_b)


def scratch_space_error(r):
    db = FakePath(r.get("db_path") or "state.db", parent=r.get("parent"))
    budget = _budget(r.get("snapshot_bytes"), r.get("snapshot_bytes"),
                     r.get("free_bytes"), r.get("headroom_bytes"))
    with Patch(live, "_disk_budget", budget):
        return live._repair_scratch_space_error(db)


def backup_free_space_error(r):
    db = FakePath(r.get("db_path") or "state.db", parent=r.get("parent"))
    budget = _budget(r.get("snapshot_bytes"), r.get("need_bytes"),
                     r.get("free_bytes"), r.get("headroom_bytes"))
    with Patch(live, "_disk_budget", budget):
        return live._backup_free_space_error(db)


def manual_hint(r):
    return live._MANUAL_RECOVER_HINT.format(db_path=r.get("db_path"))


def _exc(r):
    message = r.get("message") or ""
    if not r.get("is_db_error"):
        return Exception(message)
    exc = sqlite3.DatabaseError(message)
    if r.get("errorcode_present"):
        try:
            exc.sqlite_errorcode = int(r.get("errorcode") or 0)
        except (AttributeError, TypeError):
            class Coded(sqlite3.DatabaseError):
                pass
            exc = Coded(message)
            exc.sqlite_errorcode = int(r.get("errorcode") or 0)
    return exc


def failure_consumes_attempt(r):
    return bool(live._repair_failure_consumes_attempt(_exc(r)))


def temporary_file_failure(r):
    exc = _exc(r)
    return bool(live._is_temporary_file_failure(exc))


def schema_not_built(r):
    return bool(live._schema_not_built(Exception(r.get("message") or "")))


DISPATCH = {
    "constants": constants,
    "sidecars": sidecars,
    "mask_header": mask_header,
    "headroom": headroom,
    "snapshot_timeout": snapshot_timeout,
    "disk_budget_error": disk_budget_error,
    "scratch_space_error": scratch_space_error,
    "backup_free_space_error": backup_free_space_error,
    "manual_hint": manual_hint,
    "failure_consumes_attempt": failure_consumes_attempt,
    "temporary_file_failure": temporary_file_failure,
    "schema_not_built": schema_not_built,
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
            py = DISPATCH[r["func"]](r)
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
