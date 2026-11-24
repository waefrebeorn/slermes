#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/base_kanban_media.py
(gate w24_basekanban_865998).

The live module is imported whole; the filesystem seams (``iterdir`` /
``is_symlink`` / ``is_file``) are fed from the fixture instead of the disk, so
the decisions under test are the live ones: the ``re.fullmatch`` gate, the root
ORDER (shared root first, then the per-board roots that pass), the env override
winning alone, and ``base._sqlite_files`` fan-out for a named board.

Paths are compared POSIX-rendered (``PurePath.as_posix``) so one fixture works on
any host; ``expanduser`` is a no-op seam, so fixtures carry no "~".
"""
import json
import os
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_basekanban_865998"

# git-bash login shells carry no USERPROFILE, which Path.home() needs.
if not (os.environ.get("USERPROFILE") or os.environ.get("HOMEDRIVE")):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.platforms import base_kanban_media as live  # noqa: E402
from gateway.platforms import base as live_base           # noqa: E402

WANTED = {"board_name_ok", "attachment_roots", "board_db_paths", "constants"}

#: The literal `_kanban_attachment_roots` fullmatches board dir names with.
PATTERN = r"[a-z0-9][a-z0-9_-]{0,63}"

# Fail loudly if the upstream literal drifts from ours.
assert 're.fullmatch(r"%s"' % PATTERN in __import__("inspect").getsource(
    live._kanban_attachment_roots), "board-name pattern drifted upstream"

SEAM_ERRORS = (TypeError, ValueError, AttributeError)


def board_name_ok(r):
    return re.fullmatch(PATTERN, r.get("name")) is not None


def _root(r):
    return Path(r.get("root") or "")


def attachment_roots(r):
    override = r.get("override")
    if override is None:
        override = ""
    override = override.strip()
    if override:
        return [Path(override).expanduser().as_posix()]
    roots = [(_root(r) / "kanban" / "attachments").as_posix()]
    for b in r.get("boards") or []:
        if b.get("is_symlink"):
            continue
        name = b.get("name", "")
        if not re.fullmatch(PATTERN, name):
            continue
        if not (b.get("has_db")):
            continue
        roots.append((_root(r) / "kanban" / "boards" / name / "attachments").as_posix())
    return roots


def board_db_paths(r):
    root = _root(r)
    return [(root / "kanban" / "boards" / b["name"] / name).as_posix()
            for b in (r.get("boards") or [])
            for name in live_base._sqlite_files("kanban.db")]


def constants():
    return {
        "board_name_pattern": PATTERN,
        "board_db_name": "kanban.db",
        "sqlite_files": list(live_base._sqlite_files("kanban.db")),
        "kanban_dir": "kanban",
        "boards_subdir": "boards",
        "attachments_subdir": "attachments",
    }


def expected(r):
    f = r.get("func")
    if f == "board_name_ok":
        return board_name_ok(r)
    if f == "attachment_roots":
        return attachment_roots(r)
    if f == "board_db_paths":
        return board_db_paths(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = seams = 0
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
        except SEAM_ERRORS as exc:
            seams += 1
            if r.get("out") is not None:
                mism += 1
                print("MISMATCH %s case=%r live raises %r but c=%r (expected null)"
                      % (r.get("func"), r.get("case"), exc, r.get("out")), file=sys.stderr)
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
