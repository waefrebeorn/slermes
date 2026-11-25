#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_reconcile.py, browse
predicates and budget arithmetic (gate w34_reconcile_7686f7_p2).

sqlite3 is the seam.  Where the live helper opens a connection or reads a row
set, the oracle substitutes exactly the values the fixture carries, so what is
compared is the decision the module builds out of them:

  history_tables      _require_history_tables     (rows come from a fake cursor)
  integrity_problem   _integrity_problem          (the PRAGMA connection is stubbed;
                                                   parsing/shaping stays live)
  budget              _Budget.remaining/_Budget.busy_timeout  (clock stubbed)
  budget_check        _Budget.check
  constants           module-level browse budget constants
"""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_reconcile_7686f7_p2"

_TMPHOME = tempfile.mkdtemp(prefix="w34-reconcile-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.admin import _reconcile as live  # noqa: E402

WANTED = {"history_tables", "integrity_problem", "budget", "budget_check", "constants"}


class _FakeCatalog:
    """A connection whose only statement is the sqlite_master table survey."""

    def __init__(self, names):
        self._names = names

    def execute(self, sql):  # noqa: ARG002 - one query, one fake answer
        return [(n,) for n in self._names]


class _FakeIntegrityConn:
    """A read-only connection whose PRAGMA integrity_check(3) returns rows."""

    def __init__(self, rows):
        self._rows = rows

    def execute(self, sql):  # noqa: ARG002
        return self

    def fetchall(self):
        return self._rows

    def set_progress_handler(self, fn, n):  # noqa: ARG002
        return None

    def close(self):
        return None


def _rows(raw):
    out = []
    for row in raw if isinstance(raw, list) else []:
        out.append(tuple(row) if isinstance(row, list) else row)
    return out


def history_tables(r):
    names = r.get("names")
    names = [str(n) for n in names] if isinstance(names, list) else []
    try:
        live._require_history_tables(_FakeCatalog(names))
    except RuntimeError as exc:
        return {"ok": False, "error": str(exc)}
    return {"ok": True, "error": None}


def integrity_problem(r):
    rows = _rows(r.get("rows"))

    def _connect(*a, **k):  # noqa: ARG001 - the handle is the seam
        return _FakeIntegrityConn(rows)

    saved = live._connect_tracked_db
    live._connect_tracked_db = _connect
    try:
        return live._integrity_problem(Path(str(r.get("path") or "state.db")), live._Budget(5))
    finally:
        live._connect_tracked_db = saved


def _budget(r):
    b = live._Budget(0)
    b.deadline = float(r.get("deadline"))
    now = float(r.get("now"))
    b._clock = lambda: now
    return b


def budget(r):
    b = _budget(r)
    return {"remaining": b.remaining(), "busy_timeout": b.busy_timeout()}


def budget_check(r):
    b = live._Budget(0)
    b.deadline = 100.0
    left = float(r.get("remaining"))
    b._clock = lambda: 100.0 - left
    b.expired = bool(r.get("expired"))
    try:
        b.check()
    except live.BrowseBudgetExceeded as exc:
        return {"raise": True, "message": str(exc)}
    return {"raise": False, "message": None}


def constants(_r):
    return {
        "browse_reconcile_budget_s": float(live.BROWSE_RECONCILE_BUDGET_S),
        "browse_integrity_budget_s": float(live.BROWSE_INTEGRITY_BUDGET_S),
        "budget_progress_ops": float(live._BUDGET_PROGRESS_OPS),
        "sibling_poll_s": float(live._BROWSE_SIBLING_POLL_S),
    }


def expected(r):
    f = r.get("func")
    if f == "history_tables":
        return history_tables(r)
    if f == "integrity_problem":
        return integrity_problem(r)
    if f == "budget":
        return budget(r)
    if f == "budget_check":
        return budget_check(r)
    if f == "constants":
        return constants(r)
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
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
