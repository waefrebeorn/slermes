#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_errors.py (w20_errors_93204b).

The live ``refusals`` context manager is imported straight out of the quarry and
driven for real: the fixture names a store failure, this oracle raises it inside
``refusals`` and records which contract class (and with which constructor
arguments) came back out. The contract family is stubbed as recording exception
classes -- the module's own mapping is what is under test, not the contract's
bodies -- and the two live seams (``translated`` and ``get_compression_tip``) are
stubbed where the module reads them.
"""
import importlib
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

GATE = "w20_errors_93204b"
TARGETS = ["UnknownExecution", "CompressionInProgress", "CompressionBusy", "CompressionClosed",
           "TurnLeaseLost", "SessionWriteGuarded", "HeldRowRewritten", "PruneRowStale",
           "PruneRowUnresolved", "ResumeTooLarge", "ExportTooLarge", "RewindTargetUnavailable"]
STORE = ["SessionCompressionInProgressError", "CompressionSessionBusyError",
         "CompressionSessionClosedError", "SessionTurnLeaseLostError",
         "SessionActiveWriteGuardError", "HeldRowRewrittenError", "PruneRowStaleError",
         "PruneRowUnresolvedError", "SessionResumeTooLargeError",
         "SessionExportTooLargeError", "RewindTargetUnavailableError"]


class StoreError(Exception):
    """A store failure carrying the attributes the live arms read."""

    def __init__(self, **attrs):
        self.attrs = attrs
        for k, v in attrs.items():
            setattr(self, k, v)

    def __str__(self):
        return str(self.attrs.get("message", ""))


class AuthorityRefusal(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


class _NullCtx:
    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        return False


MADE = []


def _make(name):
    def __init__(self, *args, **kwargs):
        MADE.append((name, list(args)))
    return type(name, (Exception,), {"__init__": __init__})


def live_module(tip):
    """A fresh import of the live module with every stub the gate needs."""
    mod = importlib.import_module("persistence.sqlite.conversation._errors")
    for name in TARGETS:
        setattr(mod, name, _make(name))
    for name in STORE:
        setattr(mod, name, type(name, (StoreError,), {}))
    mod.AuthorityRefusal = AuthorityRefusal
    mod.translated = lambda: _NullCtx()
    mod.get_compression_tip = lambda holder, sid: tip
    return mod


def expected(r):
    fields = r.get("fields") if isinstance(r.get("fields"), dict) else {}
    sid = r.get("session_id") or ""
    holder = object() if r.get("holder_present") else None
    tip = r.get("compression_tip")
    mod = live_module(tip)
    exc_type = r.get("exc_type")
    if exc_type == "AuthorityRefusal":
        exc = AuthorityRefusal(fields.get("reason", ""))
    elif exc_type in STORE:
        exc = getattr(mod, exc_type)(**fields)
    else:
        exc = type(exc_type or "Exception", (Exception,), {})(str(fields.get("message", "")))
    MADE.clear()
    try:
        with mod.refusals(holder, sid):
            raise exc
    except BaseException as caught:  # noqa: BLE001 - the chain's answer IS the case
        if MADE:
            name, args = MADE[-1]
            return {"status": "raise", "class": name, "args": args}
        return {"status": "reraise", "class": None, "exc": type(caught).__name__}
    return {"status": "fell-through"}


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
        if not isinstance(r, dict) or r.get("func") is None:
            continue
        cases += 1
        try:
            exp = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH translate case=%s live-error=%r" % (r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            mism += 1
            print("MISMATCH translate case=%s py=%r c=%r" % (r.get("case"), exp, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
