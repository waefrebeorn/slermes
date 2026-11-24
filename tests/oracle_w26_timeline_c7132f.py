#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/timeline.py
(gate w26_timeline_c7132f).

The connection is faked: a stand-in whose ``execute`` returns itself and whose
``fetchone`` answers the index probe, so ``_display_rows_sql`` runs its real
branching and string assembly with no database at all.  ``_prompt_preview``
takes ``db`` unused, so it is called with ``None`` -- the turn-projection seam
for a plain human prompt is the identity, which is what the fixtures feed it.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_timeline_c7132f"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import timeline as live  # noqa: E402

WANTED = {"display_rows_sql", "prompt_preview"}


class FakeConn:
    """Only the index probe touches the connection; nothing else is called."""

    def __init__(self, row):
        self._row = row

    def execute(self, *a, **k):
        return self

    def fetchone(self):
        return self._row


def display_rows_sql(req):
    indexed = bool(req.get("indexed"))
    conn = FakeConn(None if indexed else (1,))
    return live._display_rows_sql(conn, "sid", users_only=bool(req.get("users_only")))


def prompt_preview(req):
    return live._prompt_preview(None, req.get("content"), req.get("display_kind"),
                                req.get("summary"))


def expected(r):
    f = r.get("func")
    if f == "display_rows_sql":
        return display_rows_sql(r)
    if f == "prompt_preview":
        return prompt_preview(r)
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
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
