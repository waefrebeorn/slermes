#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/repair.py (gate w28_repair_1f5388_p3).

The report / preflight / ladder layer.  The entry point is driven for real:
``repair_state_db_schema`` runs with a temporary cwd (relative paths keep
``str(Path(x)) == x``), the cross-process lock, the exhaustion probe and the
live-writer scan patched at module level -- exactly the seams upstream's own
tests patch -- so the branch plan and its operator text come from the live code.

  * the report shape ``repair_state_db_schema`` returns;
  * ``_repair_skip`` and the private ``_repair_attempted`` marker;
  * ``_run_repair_strategies``' escalation ladder (clean / reason / unavailable);
  * the pre-surgery branches: missing file, lost lock (healed or not), live
    writer, exhausted cross-restart budget;
  * ``preflight_db_writability``'s skip rule and its exact refusal text, plus
    ``_rw_or_vanished``;
  * ``_unlink_db_triple``'s victim list and the ``_REPAIR_STRATEGIES`` table.
"""
import contextlib
import json
import os
import sqlite3
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_repair_1f5388_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._state import repair as live  # noqa: E402
from persistence.core import admin as admin_mod  # noqa: E402

WANTED = {
    "initial_report", "repair_skip", "run_strategies", "repair_flow",
    "preflight_skip", "rw_or_vanished", "preflight_error", "unlink_victims",
    "strategies",
}

REPORT_KEYS = ("repaired", "strategy", "backup_path", "error")


def split_path(p):
    for i in range(len(p) - 1, -1, -1):
        if p[i] in "/\\":
            return p[: i + 1], p[i + 1:]
    return "", p


class FakePath:
    """The slice of pathlib.Path repair.py touches, driven by fixtures."""

    def __init__(self, path, exists=True, parent=None, deleted=None,
                 file_ok=True, dir_ok=False):
        self.path = path
        self._exists = exists
        self._parent = parent
        self.deleted = deleted if deleted is not None else []
        self.file_ok = file_ok
        self.dir_ok = dir_ok

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
            return self._parent
        return FakePath(split_path(self.path)[0].rstrip("/\\") or ".", parent=self._parent)

    def with_name(self, new):
        return FakePath(split_path(self.path)[0] + new, self._exists, self._parent,
                        self.deleted, self.file_ok, self.dir_ok)

    def exists(self):
        return self._exists

    def is_file(self):
        return self.file_ok

    def is_dir(self):
        return self.dir_ok

    def resolve(self):
        return self

    def is_relative_to(self, other):
        return False

    def unlink(self, missing_ok=False):
        self.deleted.append(self.path)
        return None


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


class DummyConn:
    def __init__(self):
        self.in_transaction = False

    def execute(self, sql, params=()):
        return self

    def fetchall(self):
        return []

    def fetchone(self):
        return (0,)

    def commit(self):
        return None

    def close(self):
        return None


@contextlib.contextmanager
def _dummy_conn(_path, **_kw):
    yield DummyConn()


@contextlib.contextmanager
def _lock(value):
    yield value


def _exc(r):
    message = r.get("message") or ""
    if not r.get("is_db_error"):
        return Exception(message) if r.get("has_exc") else None
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


def repair_skip(r):
    report = r.get("report") or {}
    out = live._repair_skip(dict(report), "test", r.get("error"), _exc(r))
    return out


def run_strategies(r):
    reasons = []
    for entry in r.get("reasons") or []:
        if isinstance(entry, dict) and "unavailable" in entry:
            reasons.append(admin_mod.HealthCheckUnavailable(entry["unavailable"]))
        else:
            reasons.append(entry)
    state = {"i": 0}

    def probe(_path):
        i = state["i"]
        state["i"] = i + 1
        return reasons[i] if i < len(reasons) else None

    report = {"_repair_attempted": True}
    with Patch(live, "_repair_conn", _dummy_conn), \
            Patch(live, "_db_opens_cleanly", probe), \
            Patch(live, "_reapply_durability_barriers", lambda c: True):
        live._run_repair_strategies(FakePath("state.db"), report)
    return {k: report[k] for k in ("_repair_attempted", "repaired", "strategy", "error")
            if k in report}


def _run_flow(scenario, db_path, profile_arg):
    create = scenario != "missing"
    old = os.getcwd()
    with tempfile.TemporaryDirectory() as tmp:
        os.chdir(tmp)
        try:
            if create:
                with open(db_path, "w", encoding="utf-8"):
                    pass
            exhausted = scenario == "exhausted"
            lock_value = scenario in ("live_writer", "exhausted")
            probe_value = None if scenario == "lock_lost_healed" else "database disk image is malformed"
            import hermes_constants
            with Patch(live, "report_startup_progress", lambda *a, **k: None), \
                    Patch(live, "_persistent_repair_attempts_exhausted", lambda p: exhausted), \
                    Patch(live, "_cross_process_repair_lock", lambda p: _lock(lock_value)), \
                    Patch(live, "_db_opens_cleanly", lambda p: probe_value), \
                    Patch(live, "_live_writer_holds_db", lambda p: True), \
                    Patch(hermes_constants, "profile_cli_selector", lambda *a, **k: profile_arg):
                rep = live.repair_state_db_schema(Path(db_path))
            return {k: rep.get(k) for k in REPORT_KEYS}
        finally:
            os.chdir(old)


def repair_flow(r):
    return _run_flow(r.get("scenario") or "missing", r.get("db_path") or "state.db",
                     r.get("profile_arg") or "")


def initial_report(_r):
    rep = _run_flow("missing", "state.db", "")
    rep["error"] = None
    return rep


def preflight_skip(r):
    return str(Path(r.get("db_path") or "")).startswith("file:") or \
        str(Path(r.get("db_path") or "")) == ":memory:"


def rw_or_vanished(r):
    calls = {"n": 0, "map": [bool(r.get("rw_first")), bool(r.get("rw_second"))]}

    def access(_p, _mode):
        i = min(calls["n"], 1)
        calls["n"] += 1
        return calls["map"][i]

    p = FakePath(r.get("path") or "state.db", exists=bool(r.get("exists")))
    with Patch(os, "access", access):
        return bool(live._rw_or_vanished(p))


def preflight_error(r):
    label = r.get("db_label") or "state.db"
    path = r.get("path") or ""
    is_dir = bool(r.get("is_dir"))
    parent_rw = bool(r.get("parent_rw", True))
    file_rw = bool(r.get("file_rw", False))
    if is_dir:
        # `path` is db_path.parent: the directory the preflight refuses on.
        parent = FakePath(path, exists=True, dir_ok=True)
        p = FakePath(split_path(path)[0] + "state.db", exists=True, parent=parent)
    else:
        parent_path, _ = split_path(path)
        parent = FakePath(parent_path.rstrip("/\\") or ".", exists=True, dir_ok=True)
        p = FakePath(path, exists=True, parent=parent)
    access_ok = {str(parent): parent_rw, str(p): file_rw}

    def access(x, _mode):
        return access_ok.get(str(x), False)

    with Patch(live, "get_hermes_home", lambda *a, **k: "/nonexistent-hermes-home"), \
            Patch(os, "access", access):
        try:
            live.preflight_db_writability(p, db_label=label)
        except sqlite3.OperationalError as exc:
            return str(exc)
    return None


def unlink_victims(r):
    deleted = []
    p = FakePath(r.get("db_path") or "state.db", deleted=deleted)
    live._unlink_db_triple(p)
    return deleted


def strategies(_r):
    return [{"name": name, "success": ok, "failure": bad}
            for name, _body, ok, bad in live._REPAIR_STRATEGIES]


DISPATCH = {
    "initial_report": initial_report,
    "repair_skip": repair_skip,
    "run_strategies": run_strategies,
    "repair_flow": repair_flow,
    "preflight_skip": preflight_skip,
    "rw_or_vanished": rw_or_vanished,
    "preflight_error": preflight_error,
    "unlink_victims": unlink_victims,
    "strategies": strategies,
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
