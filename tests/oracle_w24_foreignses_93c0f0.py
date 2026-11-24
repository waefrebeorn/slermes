#!/usr/bin/env python3
"""Differential oracle for hermes_cli/foreign_sessions_browser.py
(gate w24_foreignses_93c0f0).

Every case runs the live function from the Python quarry.  The I/O seams -- the
disk walk, the per-source log parser and the persistence lookup -- are patched to
answer from the request object, so no directory is scanned and no log is read;
what is compared is exactly what those values decide:

  * ``_candidates`` -> the sha256 handle (walk seam);
  * ``resolve_foreign_session`` -> both refusal texts (candidates + parse seams);
  * ``list_foreign_sessions`` -> "Invalid session page" and ``next_offset``;
  * ``_display_title`` -- pure, called as is;
  * ``preview_foreign_session`` -> the bounded message window (persistence seam);
  * ``_parse`` -> the 32 MB / empty-session refusals (parser seam).
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_foreignses_93c0f0"

# Import-time code in the live package resolves the user's home; a bare MSYS
# shell exports HOME but no USERPROFILE, which ``Path.home()`` refuses.
if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import foreign_sessions_browser as live  # noqa: E402

WANTED = {"handle_for", "resolve_error", "validate_page", "display_title",
          "preview", "parse_error", "next_offset", "constants"}


class FakePersistence:
    """The persistence seam: nothing imported, nothing written."""

    class maintenance:  # noqa: N801 - mirrors persistence.maintenance
        @staticmethod
        def find_foreign_import(origin):
            return False


def _parsed(req):
    parsed = req.get("parsed")
    return parsed if isinstance(parsed, dict) else {}


def _turns(parsed):
    turns = parsed.get("turns")
    return turns if isinstance(turns, list) else []


def handle_for(req):
    """_candidates() over a one-entry walk seam: the handle expression itself."""
    source = req.get("source") or "claude"
    path = req.get("path") or ""
    stat = SimpleNamespace(st_mtime=1.0, st_size=1)
    with mock.patch.object(live, "_walk", return_value=[(path, stat)]):
        rows = live._candidates(source)
    return rows[0][1]


def resolve_error(req):
    handle, known = req.get("handle"), req.get("known")
    known = known if isinstance(known, list) else []
    rows = [(0.0, k, "claude", "/x", 0) for k in known]
    stub = {"turns": [{"role": "user", "content": "x"}], "cwd": "/x",
            "session_id": "s", "title_guess": "t"}
    with mock.patch.object(live, "_candidates", return_value=rows), \
            mock.patch.object(live, "_parse", return_value=stub):
        try:
            live.resolve_foreign_session(handle)
            return None
        except ValueError as exc:
            return str(exc)


def validate_page(req):
    with mock.patch.object(live, "_candidates", return_value=[]):
        try:
            live.list_foreign_sessions(None, req.get("offset"), req.get("limit"))
            return None
        except ValueError as exc:
            return str(exc)


def display_title(req):
    return live._display_title(_parsed(req), req.get("source"))


def preview(req):
    parsed = _parsed(req)
    candidate = (0.0, "h" * 64, req.get("source") or "claude", "/x", 0)
    with mock.patch.object(live, "resolve_foreign_session", return_value=(candidate, parsed)):
        out = live.preview_foreign_session("h" * 64, None, persistence=FakePersistence)
    return {"messages": out["messages"], "total": out["total"],
            "truncated": out["truncated"]}


def parse_error(req):
    size = req.get("size") or 0
    count = req.get("turn_count")
    count = count if isinstance(count, int) else 0
    turns = [{"role": "user", "content": "x"} for _ in range(count)]
    candidate = (0.0, "h" * 64, "claude", "/x", size)
    with mock.patch.object(live, "_parser", return_value=lambda path: {"turns": turns}):
        try:
            live._parse(candidate)
            return None
        except ValueError as exc:
            return str(exc)


def next_offset(req):
    total = req.get("total")
    total = total if isinstance(total, int) else 0
    rows = [(0.0, "%064x" % i, "claude", "/x", 0) for i in range(total)]
    with mock.patch.object(live, "_candidates", return_value=rows), \
            mock.patch.object(live, "_parse", side_effect=ValueError("unreadable")):
        page = live.list_foreign_sessions(None, req.get("offset"), req.get("limit"))
    return page["next_offset"]


def constants(_req):
    return {"max_log_bytes": float(live.MAX_LOG_BYTES), "preview_turns": 40,
            "preview_chars": 8000, "title_chars": 180,
            "source_labels": dict(live._SOURCE_LABELS)}


def expected(r):
    f = r.get("func")
    handlers = {"handle_for": handle_for, "resolve_error": resolve_error,
                "validate_page": validate_page, "display_title": display_title,
                "preview": preview, "parse_error": parse_error,
                "next_offset": next_offset, "constants": constants}
    if f not in handlers:
        raise AssertionError("unknown func %r" % (f,))
    return handlers[f](r)


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
