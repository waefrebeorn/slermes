#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_rate_limit_backoff.py (gate w25_auxiliaryr_1beb43).

The live module is imported as-is and called with a stand-in exception object:
an exception is reduced to the three values upstream reads off it
(``response.headers``, ``body``, the botocore ``ResponseMetadata.HTTPHeaders``
dict), so no provider client, no network and no retry ladder is involved.  The
one stochastic input -- ``retry_utils.jittered_backoff`` seeds a
``random.Random`` from time + counter -- is pinned to the fixture's uniform draw
by substituting the Random class for the duration of the call.
"""
import contextlib
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_auxiliaryr_1beb43"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent import auxiliary_rate_limit_backoff as live  # noqa: E402
from agent import retry_utils  # noqa: E402

WANTED = {"retry_after", "wait", "constants"}


def make_exc(e):
    """The exception object, built only from the value seams upstream reads."""
    e = e if isinstance(e, dict) else {}
    if "headers" in e:
        response = SimpleNamespace(headers=e.get("headers"))
    elif "metadata" in e:
        md = e.get("metadata")
        response = ({"ResponseMetadata": {"HTTPHeaders": md}} if md is not None
                    else {"ResponseMetadata": {}})
    else:
        response = None
    return SimpleNamespace(response=response, body=e.get("body"))


class _PinnedRandom:
    """random.Random whose uniform() returns a + draw * (b - a)."""

    def __init__(self, draw):
        self._draw = draw

    def uniform(self, a, b):
        return a + self._draw * (b - a)


@contextlib.contextmanager
def pinned_random(draw):
    original = retry_utils.random.Random

    class _R(_PinnedRandom):
        def __init__(self, seed):
            super().__init__(draw)

    retry_utils.random.Random = _R
    try:
        yield
    finally:
        retry_utils.random.Random = original


def retry_after(r):
    return live.rate_limit_retry_after(make_exc(r.get("exc")))


def wait(r):
    exc = make_exc(r.get("exc"))
    attempt = r.get("attempt", 1)
    draw = float(r.get("jitter", 0.0))
    with pinned_random(draw):
        return live.rate_limit_wait(exc, attempt)


def constants():
    return {
        "retries": float(live.RATE_LIMIT_RETRIES),
        "backoff_base": float(live.RATE_LIMIT_BACKOFF_BASE),
        "backoff_cap": float(live.RATE_LIMIT_BACKOFF_CAP),
        "retry_after_cap": float(live.RATE_LIMIT_RETRY_AFTER_CAP),
    }


def expected(r):
    f = r.get("func")
    if f == "retry_after":
        return retry_after(r)
    if f == "wait":
        return wait(r)
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
