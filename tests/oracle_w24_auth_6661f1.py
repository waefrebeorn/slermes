#!/usr/bin/env python3
"""Differential oracle for gateway/relay/auth.py (gate w24_auth_6661f1).

The live module is imported and its pure framing helpers are called directly:

  * ``sign(payload, secret)`` -> HMAC-SHA256 hexdigest over UTF-8;
  * ``make_token(payload, secret, ttl_seconds)`` -> unpadded base64url of
    ``f"{payload}:{exp}:{sig}"``;
  * ``make_upgrade_token(gateway_id, secret, ttl_seconds=300)``;
  * ``_DEFAULT_UPGRADE_TTL_SECONDS``.

``time.time()`` is replaced by a fixed clock driven by the case's ``now``
(no sleeping, no wall clock), so the exact bytes are comparable.  A case whose
inputs make the live helper raise (a non-string payload, a null ttl) is the
error seam: the C side must answer ``null`` there.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_auth_6661f1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.relay import auth as live  # noqa: E402

WANTED = {"sign", "make_token", "make_upgrade_token", "constants"}


class _Clock:
    """Stand-in for the ``time`` module: only ``time()`` is consulted."""

    def __init__(self, now):
        self._now = float(now)

    def time(self):
        return self._now


def with_clock(now, fn):
    saved = live.time
    live.time = _Clock(now)
    try:
        return fn()
    finally:
        live.time = saved


def _text_pair(r, a="payload", b="secret"):
    """Both arguments must be str, else the live call raises (error seam)."""
    pa, pb = r.get(a), r.get(b)
    if not isinstance(pa, str) or not isinstance(pb, str):
        raise TypeError("non-str argument")
    return pa, pb


def sign(r):
    payload, secret = _text_pair(r)
    return live.sign(payload, secret)


def make_token(r):
    payload, secret = _text_pair(r)
    now = r.get("now") or 0
    if "ttl_seconds" in r:
        ttl = r["ttl_seconds"]
        return with_clock(now, lambda: live.make_token(payload, secret, ttl))
    return with_clock(now, lambda: live.make_token(payload, secret))


def make_upgrade_token(r):
    gateway_id, secret = _text_pair(r, "gateway_id", "secret")
    now = r.get("now") or 0
    if "ttl_seconds" in r:
        ttl = r["ttl_seconds"]
        return with_clock(now, lambda: live.make_upgrade_token(gateway_id, secret, ttl))
    return with_clock(now, lambda: live.make_upgrade_token(gateway_id, secret))


def constants():
    return {"default_upgrade_ttl_seconds": live._DEFAULT_UPGRADE_TTL_SECONDS}


def expected(r):
    f = r.get("func")
    try:
        if f == "sign":
            return sign(r)
        if f == "make_token":
            return make_token(r)
        if f == "make_upgrade_token":
            return make_upgrade_token(r)
        if f == "constants":
            return constants()
    except Exception:  # noqa: BLE001 - the error seam: C must answer null
        return None
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
