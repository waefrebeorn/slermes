#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_media_retry.py
(gate w25_telegramme_bdc44d).

Three seams are pinned to the fixture's values, everything else runs live:

  * ``kinds.send_error_kind`` -- the PTB exception-graph classifier -- returns
    the fixture's ``kind``, so the policy is tested without a socket;
  * ``asyncio.sleep`` records the backoff instead of waiting;
  * ``_redact_telegram_error_text`` is left live but the log sink is silenced,
    and each fixture's ``error_text`` is plain prose (nothing secret to redact).

``send_media_retrying`` is then actually run: the trace (status, attempt count,
backoff list, upload rewinds, whether the request-error flag landed on the
raised exception) is compared, as are ``request_may_have_delivered`` and
``unconfirmed_media_result`` (normalised to a dict of the SendResult fields).
"""
import asyncio
import json
import logging
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_telegramme_bdc44d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The plugin package's __init__ pulls the gateway stack, whose module-level
# auth path resolves the Hermes home.  Give it a home when the invoking shell
# has none (the value only has to be stable, never the real one).
import os  # noqa: E402

os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))

from plugins.platforms.telegram import telegram_media_retry as live  # noqa: E402

kinds = live.kinds
logging.disable(logging.CRITICAL)
live.logger.disabled = True

WANTED = {"request_may_have_delivered", "unconfirmed_media_result", "retry_plan", "constants"}


class FakeErr(Exception):
    """An upload failure carrying the fixture's classifier verdict."""


def _err(req):
    e = FakeErr(req.get("error_text") or "")
    e._kind = req.get("kind")
    if req.get("request_error"):
        setattr(e, live._REQUEST_ERROR_ATTR, True)
    return e


def _norm(result):
    if result is None:
        return None
    return {
        "success": result.success,
        "message_id": result.message_id,
        "error": result.error,
        "retryable": result.retryable,
        "raw_response": result.raw_response,
    }


def request_may_have_delivered(req):
    kinds.send_error_kind = lambda _e: req.get("kind")
    return live.request_may_have_delivered(_err(req))


def unconfirmed_media_result(req):
    kinds.send_error_kind = lambda _e: req.get("kind")
    return _norm(live.unconfirmed_media_result(
        _err(req), req.get("what", "media"), as_failure=bool(req.get("as_failure"))))


def retry_plan(req):
    attempts = req.get("attempts") or []
    idx = {"i": 0}
    delays = []
    resets = {"n": 0}

    kinds.send_error_kind = lambda e: getattr(e, "_kind", None)

    def send_once():
        i = idx["i"]
        idx["i"] += 1
        spec = attempts[i] if i < len(attempts) else {}

        async def _attempt():
            if spec.get("ok"):
                return object()
            err = FakeErr(spec.get("error_text") or "boom")
            err._kind = spec.get("kind")
            raise err
        return _attempt()

    def reset_media():
        resets["n"] += 1

    async def _sleep(delay):
        delays.append(float(delay))

    saved_sleep = asyncio.sleep
    asyncio.sleep = _sleep
    loop = asyncio.new_event_loop()
    try:
        try:
            loop.run_until_complete(live.send_media_retrying(
                send_once, reset_media if req.get("reset") else None,
                req.get("what", "media")))
            status, flagged = "ok", False
        except FakeErr as exc:
            status = "raise"
            flagged = bool(getattr(exc, live._REQUEST_ERROR_ATTR, False))
    finally:
        loop.close()
        asyncio.sleep = saved_sleep

    return {
        "status": status,
        "attempts": idx["i"],
        "delays": delays,
        "resets": resets["n"],
        "error_attr": flagged,
    }


def constants(_req):
    return {
        "same_request_attempts": kinds.SAME_REQUEST_ATTEMPTS,
        "request_error_attr": live._REQUEST_ERROR_ATTR,
        "retryable_kinds": [kinds.UNSENT, kinds.SERVER_ERROR],
        "lost_kind": kinds.LOST,
    }


HANDLERS = {
    "request_may_have_delivered": request_may_have_delivered,
    "unconfirmed_media_result": unconfirmed_media_result,
    "retry_plan": retry_plan,
    "constants": constants,
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
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
