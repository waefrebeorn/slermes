#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/lockowners.py
(gate w22_lockowners_a9d36b).

stat()/open() of state.db and /proc/locks are the filesystem seam; the module
itself imports only stdlib, so it loads live. One platform seam is shimmed:
this host is win32 and has no os.makedev, so the module's `os` global is
replaced by a view that implements Linux's glibc gnu_dev_makedev (the encoding
the live function relies on for the hex major:minor ident).

  describe_range -- live `_describe_range`.
  parse_locks    -- live `parse_proc_locks` over fixture text/inodes.
  warning        -- live `log_write_lock_holders` with the holder lookup stubbed
                    (it stats the DB); the logger text is captured verbatim.
"""
import importlib.util
import io
import json
import logging
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "_state" / "lockowners.py"
GATE = "w22_lockowners_a9d36b"


def linux_makedev(major, minor):
    return ((minor & 0xFF) | ((major & 0xFFF) << 8) | ((minor & ~0xFF) << 12)
            | ((major & ~0xFFF) << 32))


def load_live():
    spec = importlib.util.spec_from_file_location("lockowners_live", SRC)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["lockowners_live"] = mod
    spec.loader.exec_module(mod)
    mod.os = types.SimpleNamespace(makedev=linux_makedev)
    return mod


live = load_live()
WANTED = {"describe_range", "parse_locks", "warning"}


def inode_map(rows):
    return {(linux_makedev(int(r.get("major") or 0), int(r.get("minor") or 0)),
             int(r.get("ino") or 0)): r.get("sidecar") for r in rows or []}


def warning(path_name, patience, holders):
    buf = io.StringIO()
    handler = logging.StreamHandler(buf)
    handler.setFormatter(logging.Formatter("%(message)s"))
    orig = live.state_db_write_lock_holders
    live.state_db_write_lock_holders = lambda _p: list(holders or [])
    live.logger.addHandler(handler)
    live.logger.setLevel(logging.WARNING)
    live.logger.propagate = False
    try:
        live.log_write_lock_holders(Path("/x") / path_name, float(patience))
    finally:
        live.logger.removeHandler(handler)
        live.state_db_write_lock_holders = orig
    return buf.getvalue().rstrip("\n")


def expected(r):
    f = r.get("func")
    if f == "describe_range":
        return live._describe_range(r.get("sidecar"),
                                    int(r.get("start") or 0), int(r.get("end") or 0))
    if f == "parse_locks":
        rows = live.parse_proc_locks(r.get("text") or "", inode_map(r.get("inodes")))
        return [[pid, kind, sidecar] for pid, kind, sidecar in rows]
    if f == "warning":
        return warning(r.get("path_name"), r.get("patience"), r.get("holders"))
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
