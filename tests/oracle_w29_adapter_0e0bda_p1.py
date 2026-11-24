#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/a2a/adapter.py, gate p1.

The live inbound adapter module is imported from the quarry tree and its
module-level value helpers are called directly.  The socket/thread layer is the
seam: nothing binds, no handler runs, no task store is touched -- what is
compared is exactly the arithmetic the HTTP worker threads do around it:

  * ``_reply_timeout`` / ``_orphan_timeout`` over the captured
    A2A_REPLY_TIMEOUT value (the env var is set from the fixture line),
  * ``_clean_slug`` / ``_safe_context_slug`` (attacker-provided ids),
  * ``_join_url`` (reverse-proxy public URLs),
  * ``_http_server_class`` / ``A2ARequestHandler._client_ip``, and
  * the module budget constants.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_0e0bda_p1"

# A login shell can start without a Windows home; the live package resolves
# hermes_home at import time, so anchor it on the quarry tree's parent.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.a2a import adapter as live  # noqa: E402

WANTED = {"constants", "reply_timeout", "orphan_timeout", "clean_slug",
          "join_url", "safe_context_slug", "server_class", "client_ip"}

ENV_KEY = "A2A_REPLY_TIMEOUT"


def _set_env(raw):
    if raw is None:
        os.environ.pop(ENV_KEY, None)
    else:
        os.environ[ENV_KEY] = raw if isinstance(raw, str) else str(raw)


def constants(_r):
    return {
        "default_port": live._DEFAULT_PORT,
        "min_orphan_timeout": float(live._MIN_ORPHAN_TIMEOUT),
        "max_orphan_timeout": float(live._MAX_ORPHAN_TIMEOUT),
        "watchdog_interval": live._WATCHDOG_INTERVAL,
        "max_body": live._MAX_BODY,
        "sse_keepalive": live._SSE_KEEPALIVE,
        "max_forwarded_inputs": live.A2AAdapter._MAX_FORWARDED_INPUTS,
        "default_description": live._DEFAULT_DESCRIPTION,
    }


def _env_call(r, fn):
    _set_env(r.get("env"))
    try:
        return fn()
    finally:
        os.environ.pop(ENV_KEY, None)


def reply_timeout(r):
    return _env_call(r, live._reply_timeout)


def orphan_timeout(r):
    return _env_call(r, live._orphan_timeout)


def clean_slug(r):
    return live._clean_slug(r.get("value"))


def join_url(r):
    return live._join_url(r.get("base"), r.get("prefix"))


def safe_context_slug(r):
    max_len = r.get("max_len")
    if max_len is None:
        return live._safe_context_slug(r.get("value"))
    return live._safe_context_slug(r.get("value"), max_len)


def server_class(r):
    cls = live._http_server_class(r.get("host"))
    return "v6" if cls is live._ThreadingHTTPServerV6 else "v4"


def client_ip(r):
    handler = live.A2ARequestHandler.__new__(live.A2ARequestHandler)
    addr = r.get("addr")
    handler.client_address = (addr,) if addr is not None else ()
    return handler._client_ip()


HANDLERS = {
    "constants": constants,
    "reply_timeout": reply_timeout,
    "orphan_timeout": orphan_timeout,
    "clean_slug": clean_slug,
    "join_url": join_url,
    "safe_context_slug": safe_context_slug,
    "server_class": server_class,
    "client_ip": client_ip,
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
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
