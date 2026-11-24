#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_peer_deadline.py
(gate w20_hostedroom_dfc5ca).

The live module is imported for real and its _Budget drives every case with a
scripted monotonic clock, so remaining()/track()/close() are the live methods.
open_peer_url cannot be called without sockets, so its prologue is replayed the
same way the opener does it -- the target tuple, the live
_MAX_OUTSTANDING_ATTEMPTS registry rule, then the live _Budget's remaining() --
and compared against the ported plan.
"""
import importlib.util
import json
import os
import sys

sys.dont_write_bytecode = True
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_hostedroom_dfc5ca_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "tui_gateway", "hosted_room_peer_deadline.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

DEADLINE = "peer request deadline exceeded"
CAPACITY = "peer request capacity reached; retry later"


class Clock:
    """time.monotonic() answering a scripted sequence (the last value repeats)."""

    def __init__(self, values):
        self.values = list(values) or [0.0]
        self.index = 0

    def monotonic(self):
        value = self.values[self.index] if self.index < len(self.values) else self.values[-1]
        self.index += 1
        return value

    def sleep(self, _seconds):
        return None

    def time(self):
        return self.monotonic()


class Sock:
    def __init__(self, fail=False):
        self.fail = fail
        self.errors = 0
        self.closed = 0
        self.shutdowns = 0

    def shutdown(self, how):
        self.shutdowns += 1
        if self.fail:
            self.errors += 1
            raise OSError("already gone")

    def close(self):
        self.closed += 1


def use_clock(elapsed, closed_enough=1):
    live.time = Clock([0.0, elapsed])


def expected(r):
    f = r.get("func")
    if f == "budget_remaining":
        use_clock(float(r.get("elapsed", 0.0)))
        budget = live._Budget(float(r.get("timeout", 0.0)))
        if r.get("closed"):
            budget.closed = True
        try:
            return {"remaining": budget.remaining()}
        except TimeoutError as exc:
            return {"error": str(exc)}
    if f == "budget_track":
        live.time = Clock([0.0])
        budget = live._Budget(5.0)
        budget.closed = bool(r.get("closed"))
        sock = Sock()
        try:
            budget.track(sock)
        except TimeoutError as exc:
            return {"error": str(exc), "closed_sockets": sock.closed}
        return {"status": "tracked", "sockets": len(budget.sockets),
                "closed_sockets": sock.closed}
    if f == "budget_close":
        live.time = Clock([0.0])
        budget = live._Budget(float(r.get("timeout", 0.0)))
        socks = [Sock(bool(r.get("shutdown_fails"))) for _ in range(int(r.get("sockets", 0)))]
        for sock in socks:
            budget.track(sock)
        budget.close()
        out = {"closed_sockets": sum(s.closed for s in socks),
               "shutdown_calls": sum(s.shutdowns for s in socks),
               "shutdown_errors": sum(s.errors for s in socks),
               "sockets": len(budget.sockets),
               "closed": budget.closed}
        try:
            budget.remaining()
            out["remaining_error"] = None
        except TimeoutError as exc:
            out["remaining_error"] = str(exc)
        return out
    if f == "open_plan":
        host = r.get("host")
        scheme = r.get("scheme", "https")
        port = r.get("port") or (443 if scheme == "https" else 80)
        target = (host, port)
        attempts = [tuple(t) for t in (r.get("targets") or [])]
        if target in attempts or len(attempts) >= live._MAX_OUTSTANDING_ATTEMPTS:
            return {"status": "error", "message": CAPACITY}
        use_clock(float(r.get("elapsed", 0.0)))
        budget = live._Budget(float(r.get("timeout", 0.0)))
        try:
            budget.remaining()
        except TimeoutError as exc:
            return {"status": "error", "message": str(exc)}
        return {"target": [host, port], "status": "ok"}
    if f == "max_attempts":
        return live._MAX_OUTSTANDING_ATTEMPTS
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
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_hostedroom_dfc5ca: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
