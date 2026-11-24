#!/usr/bin/env python3
"""Differential oracle for gateway/slash_commands_branch_thread.py (gate w25_slashcomma_2c9db9).

The live module is imported and its own pure helpers are called on real
``SessionSource`` values built from the fixture: no adapter, no thread and no
gateway connection exists.  ``branch_dest_source`` returns a whole dataclass,
so the oracle compares the field subset the C port returns instead of the
identity/scope fields the copy carries untouched.
"""
import dataclasses
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# gateway.session imports agent modules that resolve a home directory at import
# time; under an MSYS shell only HOME is exported, so give pathlib a Windows
# profile.  Nothing is written: only a path is derived.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(tempfile.gettempdir()))

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_slashcomma_2c9db9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.config import Platform  # noqa: E402
from gateway.session import SessionSource  # noqa: E402
from gateway import slash_commands_branch_thread as live  # noqa: E402

WANTED = {"parse_branch_args", "branch_thread_parent", "branch_dest_source",
          "format_thread_ref", "constants"}

# The fields the C port carries; per-message and identity fields are copied
# untouched by dataclasses.replace and are not part of this comparison.
DEST_FIELDS = ("chat_id", "chat_name", "chat_type", "parent_chat_id", "thread_id",
               "message_id", "prospective_thread_id", "auto_thread_created",
               "auto_thread_initial_name")


def _platform(value):
    if isinstance(value, str):
        try:
            return Platform(value)
        except Exception:  # noqa: BLE001 - an unknown name is not a member
            return value
    return value


def _source(raw):
    d = raw if isinstance(raw, dict) else {}
    return SessionSource(
        platform=_platform(d.get("platform")),
        chat_id=d.get("chat_id"),
        chat_name=d.get("chat_name"),
        chat_type=d.get("chat_type", "dm"),
        thread_id=d.get("thread_id"),
        parent_chat_id=d.get("parent_chat_id"),
    )


def parse_branch_args(req):
    stay_here, title = live.parse_branch_args(req.get("raw"))
    return {"stay_here": stay_here, "title": title}


def branch_thread_parent(req):
    return live.branch_thread_parent(_source(req.get("source")))


def branch_dest_source(req):
    dest = live.branch_dest_source(
        _source(req.get("source")),
        parent_id=req.get("parent_id"),
        thread_id=req.get("thread_id"),
        title=req.get("title"),
    )
    return {name: getattr(dest, name) for name in DEST_FIELDS}


def format_thread_ref(req):
    return live.format_thread_ref(_platform(req.get("platform")), req.get("thread_id"))


def constants(_req):
    return {
        "branch_thread_platforms": sorted(p.value for p in live.BRANCH_THREAD_PLATFORMS),
        "branch_here_flag": live.BRANCH_HERE_FLAG,
    }


def expected(r):
    f = r.get("func")
    return {
        "parse_branch_args": parse_branch_args,
        "branch_thread_parent": branch_thread_parent,
        "branch_dest_source": branch_dest_source,
        "format_thread_ref": format_thread_ref,
        "constants": constants,
    }[f](r)


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
