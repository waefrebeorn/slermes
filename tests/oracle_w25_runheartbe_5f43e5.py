#!/usr/bin/env python3
"""Differential oracle for gateway/run_heartbeat_acceptance.py
(gate w25_runheartbe_5f43e5).

The live module is imported and its pure decisions are called for real: the
lineage check runs against a stand-in event/entry and a store whose single row
is the fixture's ``current``; the executor seam is an async stub returning the
fixture's ``tip`` (so the compression-tip closure, and with it the persistence
handle, never runs); the refund's manager is a stub that counts calls and raises
on request, with the module logger swapped for a counter.  The identity string is
built with the same ``json.dumps(..., separators=(',', ':'))`` call, whose list
elements are read out of the live source so the C form cannot drift; the pending
scan is the live ``any(...)`` expression extracted and evaluated as-is.
"""
import ast
import asyncio
import inspect
import json
import os
import sys
import tempfile
import textwrap
from pathlib import Path

# Env seam: the live package may resolve Path.home() at import time and ntpath
# only knows USERPROFILE; give it one when the caller's shell dropped it.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = os.environ.get("HOME") or tempfile.gettempdir()

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_runheartbe_5f43e5"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway import run_heartbeat_acceptance as live  # noqa: E402

WANTED = {"identity", "owner_is_current", "resolve_owner", "settle",
          "pending_has_heartbeat", "constants"}


def _admit_source():
    return textwrap.dedent(inspect.getsource(live.admit_heartbeat))


def _identity_elements():
    tree = ast.parse(_admit_source())
    for node in ast.walk(tree):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                and node.func.attr == "dumps" and node.args
                and isinstance(node.args[0], ast.List)):
            return [ast.unparse(e) for e in node.args[0].elts]
    raise AssertionError("no json.dumps([...]) identity in admit_heartbeat")


IDENTITY_ELEMENTS = _identity_elements()


def _pending_expression():
    tree = ast.parse(_admit_source())
    for node in ast.walk(tree):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                and node.func.id == "any"):
            return ast.unparse(node)
    raise AssertionError("no any(...) admission scan in admit_heartbeat")


PENDING_EXPR = _pending_expression()


class _Row:
    """A store row: the two attributes heartbeat_owner_is_current reads."""

    def __init__(self, data):
        self.session_id = data.get("session_id")
        self.suspended = data.get("suspended", False)


class _Store:
    def __init__(self, row):
        self._row = row

    def lookup_by_session_key(self, key):
        return self._row


class _Runner:
    def __init__(self, row):
        self.session_store = _Store(row)


class _Entry:
    def __init__(self, session_id):
        self.session_id = session_id
        self.session_key = "session-key"


class _Event:
    def __init__(self, **kw):
        self.__dict__.update(kw)


def _row(r):
    current = r.get("current")
    return _Row(current) if isinstance(current, dict) else None


def identity(r):
    if IDENTITY_ELEMENTS != ["'heartbeat'", "session_id", "state.created_at", "state.fire_count"]:
        raise AssertionError("live identity changed: %r" % (IDENTITY_ELEMENTS,))
    return json.dumps(["heartbeat", r.get("session_id"), r.get("created_at"),
                       r.get("fire_count")], separators=(",", ":"))


def owner_is_current(r):
    event = _Event(_heartbeat_resolved_session_id=r.get("expected"))
    return live.heartbeat_owner_is_current(_Runner(_row(r)), event, "session-key")


def resolve_owner(r):
    async def _exec(fn):
        return r.get("tip")

    runner = _Runner(_row(r))
    runner._run_in_executor_with_context = _exec
    event = _Event(_heartbeat_session_id=r.get("expected"))
    return asyncio.run(live.resolve_heartbeat_owner(runner, event, _Entry(r.get("resolved"))))


class _Manager:
    def __init__(self, raises):
        self.raises = raises
        self.calls = 0

    def abandon_fire(self):
        self.calls += 1
        if self.raises:
            raise RuntimeError("unrefundable")


class _Logger:
    def __init__(self):
        self.warnings = 0

    def warning(self, *a, **kw):
        self.warnings += 1


def settle(r):
    event = _Event(_heartbeat_execution_started=r.get("execution_started"))
    manager = _Manager(r.get("abandon_raises"))
    logger, live.logger = live.logger, _Logger()
    try:
        live.settle_heartbeat_attempt(event, manager)
    finally:
        captured = live.logger
        live.logger = logger
    return {"abandoned": manager.calls == 1 and not manager.raises,
            "warned": captured.warnings > 0}


def pending_has_heartbeat(r):
    rows = []
    for entry in r.get("pending") if isinstance(r.get("pending"), list) else []:
        row = _Entry(None)
        row.payload = entry.get("payload") if isinstance(entry, dict) else None
        rows.append(row)
    return bool(eval(PENDING_EXPR, {"pending": rows}))  # noqa: S307 - the live expression


def constants():
    return {"identity_prefix": IDENTITY_ELEMENTS[0].strip("'\""),
            "payload_path": ["native_text_v1", "automation", "local_automation_v1", "heartbeat"]}


def expected(r):
    f = r.get("func")
    if f == "identity":
        return identity(r)
    if f == "owner_is_current":
        return owner_is_current(r)
    if f == "resolve_owner":
        return resolve_owner(r)
    if f == "settle":
        return settle(r)
    if f == "pending_has_heartbeat":
        return pending_has_heartbeat(r)
    if f == "constants":
        return constants()
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
