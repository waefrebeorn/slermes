#!/usr/bin/env python3
"""Differential oracle for gateway/windows_proactor_accept.py (w20_windowspro_18c6a5).

The live module is imported from the quarry. `dropped_peer` and
`install_proactor_accept_guard` are called on real exception/loop objects; the
accept choreography is replayed by driving a real `GuardedAccept` with fake
proactor futures on a real event loop, recording every warning, every closed
connection and the returned future's settled state.
"""
import asyncio
import importlib.util
import json
import os
import sys
from asyncio.proactor_events import BaseProactorEventLoop

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_windowspro_18c6a5_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "windows_proactor_accept.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

TYPES = {"OSError": OSError, "ConnectionResetError": ConnectionResetError,
         "ConnectionAbortedError": ConnectionAbortedError,
         "RuntimeError": RuntimeError, "TimeoutError": TimeoutError}


def mk_exc(spec):
    """An exception object shaped like the fixture describes."""
    spec = spec if isinstance(spec, dict) else {}
    cls = TYPES.get(spec.get("type") or "OSError", OSError)
    exc = cls(spec.get("type") or "OSError")
    if spec.get("winerror") is not None:
        exc.winerror = int(spec["winerror"])
    return exc


def exc_json(exc):
    return {"type": type(exc).__name__, "winerror": getattr(exc, "winerror", None)}


class FakeLoop(BaseProactorEventLoop):
    """A BaseProactorEventLoop instance (isinstance-true) with no selector."""

    def __init__(self, proactor):
        self._proactor = proactor
        self._closed = True

    def close(self):
        pass


class FakeProactor:
    def __init__(self, script, loop):
        self.script = list(script)
        self.loop = loop
        self.conns = []

    def accept(self, listener):
        item = self.script.pop(0) if self.script else {"kind": "error", "winerror": 10061}
        kind = item.get("kind", "ok")
        exc = mk_exc(item)
        if kind == "raise":
            raise exc
        fut = self.loop.create_future()
        if kind == "ok":
            conn = Conn(item.get("conn", ""))
            self.conns.append(conn)
            fut.set_result((conn, None))
        elif kind == "cancel":
            fut.cancel()
        else:
            fut.set_exception(exc)
        return fut


class Conn:
    def __init__(self, name):
        self.name = name
        self.closed = False

    def close(self):
        self.closed = True


class RecordLogger:
    def __init__(self):
        self.warnings = []

    def warning(self, *args, **kwargs):
        exc = args[1] if len(args) > 1 else None
        self.warnings.append(exc_json(exc) if isinstance(exc, BaseException) else {"type": None, "winerror": None})

    def debug(self, *args, **kwargs):
        pass


def pump(loop, rounds=12):
    for _ in range(rounds):
        loop.run_until_complete(asyncio.sleep(0))


def replay(script, outer_cancelled):
    loop = asyncio.new_event_loop()
    rec = RecordLogger()
    old_logger = live.logger
    live.logger = rec
    proactor = FakeProactor(script if isinstance(script, list) else [], loop)
    try:
        guard = live.GuardedAccept(loop, proactor.accept)
        state, conn_name, error = "pending", None, None
        try:
            outer = guard(object())
        except BaseException as exc:  # noqa: BLE001 - submission-time raise
            state, error = "raised", exc_json(exc)
            outer = None
        if outer is not None:
            if outer_cancelled:
                outer.cancel()
            pump(loop)
            if outer.cancelled():
                state = "cancelled"
            elif outer.done():
                exc = outer.exception()
                if exc is None:
                    result = outer.result()
                    state = "result"
                    conn_name = result[0].name if isinstance(result, tuple) else None
                else:
                    state, error = "exception", exc_json(exc)
            else:
                state = "pending"
        return {
            "events": [{"event": "warning", **w} for w in rec.warnings]
                      + [{"event": "close", "conn": c.name} for c in proactor.conns if c.closed],
            "outer": {"state": state, "conn": conn_name, "error": error},
            "closed": [c.name for c in proactor.conns if c.closed],
        }
    finally:
        live.logger = old_logger
        try:
            loop.close()
        except Exception:  # noqa: BLE001 - a fake loop may already be shut
            pass


def expected(r):
    f = r.get("func")
    if f == "dropped_peer":
        return bool(live.dropped_peer(mk_exc(r)))
    if f == "should_install":
        proactor_present = bool(r.get("has_proactor", True))
        callable_accept = bool(r.get("accept_callable", True))
        guarded = bool(r.get("already_guarded", False))

        class P:
            pass

        p = P()
        if proactor_present:
            p.accept = (lambda listener: None) if callable_accept else 5
            if guarded:
                p.accept = live.GuardedAccept(asyncio.new_event_loop(), lambda listener: None)
        if r.get("is_proactor_loop", True):
            loop = FakeLoop(p if proactor_present else None)
        else:
            loop = P()
            loop._proactor = p if proactor_present else None
        live.install_proactor_accept_guard(loop)
        # "Installed" means the guard replaced the accept hook, not that the hook
        # happens to be a GuardedAccept already.
        return isinstance(getattr(p, "accept", None), live.GuardedAccept) and not guarded
    if f == "replay":
        return replay(r.get("script"), bool(r.get("outer_cancelled", False)))
    raise AssertionError("unknown func %r" % (f,))


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
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_windowspro_18c6a5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
