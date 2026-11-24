#!/usr/bin/env python3
"""Differential oracle for hermes_cli/dashboard_auth/ws_tickets.py
(gate w24_wstickets_fa7658).

The live module is imported whole and driven through its own store, with the
clock patched to the fixture's ``now`` and the store seeded from the fixture
(``_tickets`` entry, ``_internal_credential``).  So the mint, the single-use
pop, the expiry sweep, the ``TicketInvalid`` wording and the constant-time
compare are all the upstream ones; the C port only replaces the dict and the
lock.

A live TypeError/ValueError/AttributeError is the raise seam: the C port has no
exception path there, so the expected value is null.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_wstickets_fa7658"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli.dashboard_auth import ws_tickets as live  # noqa: E402

WANTED = {"mint_info", "consume", "gc_keep", "consume_internal", "constants"}

SEAM_ERRORS = (TypeError, ValueError, AttributeError, KeyError, IndexError)


class _Clock:
    def __init__(self, now):
        self._now = now

    def time(self):
        return self._now


class _Patched:
    """Patch live.time (int() of the fixture's clock) and restore it after."""

    def __init__(self, now):
        self.now = now

    def __enter__(self):
        self._saved = live.time
        live.time = _Clock(self.now)
        return self

    def __exit__(self, *exc):
        live.time = self._saved
        return False


def _verdict(fn):
    try:
        return {"ok": True, "info": fn()}
    except live.TicketInvalid as exc:
        return {"ok": False, "error": str(exc)}


def mint_info(r):
    live._reset_for_tests()
    with _Patched(r.get("now")):
        ticket = live.mint_ticket(user_id=r.get("user_id"), provider=r.get("provider"),
                                  issuer=r.get("issuer", ""), extra=r.get("extra"))
        return live.consume_ticket(ticket)      # what the WS handler receives


def consume(r):
    live._reset_for_tests()
    with _Patched(r.get("now")):
        entry = r.get("entry")
        if entry is not None:
            expires_at, info = entry
            live._tickets[r.get("ticket")] = (expires_at, info)
        return _verdict(lambda: live.consume_ticket(r.get("ticket")))


def gc_keep(r):
    live._reset_for_tests()
    with _Patched(r.get("now")):
        for row in r.get("tickets") or []:
            live._tickets[row["ticket"]] = (int(row["expires_at"]), {})
        live._gc_expired_locked()
        return list(live._tickets.keys())


def consume_internal(r):
    live._reset_for_tests()
    expected = r.get("expected")
    if expected is not None:
        live._internal_credential = expected
    return _verdict(lambda: live.consume_internal_credential(r.get("value")))


def constants():
    return {
        "ttl_seconds": live.TTL_SECONDS,
        "internal_user_id": live.INTERNAL_USER_ID,
        "internal_provider": live.INTERNAL_PROVIDER,
    }


def expected(r):
    f = r.get("func")
    if f == "mint_info":
        return mint_info(r)
    if f == "consume":
        return consume(r)
    if f == "gc_keep":
        return gc_keep(r)
    if f == "consume_internal":
        return consume_internal(r)
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
