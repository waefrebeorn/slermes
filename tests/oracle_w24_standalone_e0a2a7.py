#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/standalone_rest.py
(gate w24_standalone_e0a2a7).

The live module is imported and its pure helpers are called directly:

  * ``api_url(path)`` -- route URL builder (discord.py is not installed here,
    so the live ``Route.BASE`` import falls back to the same constant);
  * ``_seconds(value)`` -- the Retry-After/Reset-After clamp;
  * ``post_paced(session, url, read, ...)`` -- driven end to end against a
    scripted fake session, a fixed monotonic clock and an ``asyncio.sleep``
    that only advances that clock.  What comes back (waits, post count, final
    status, final clock, bucket refill instant) is exactly the pacing decision.

No socket, no thread, no real sleep: the fake session answers from the case's
``responses`` list and the ``_bucket_open_at`` module state is reset per case.
"""
import asyncio
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_standalone_e0a2a7"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

"""Load the single live file directly: the package __init__ imports the whole
adapter stack (and reads $HOME at import), which the module itself does not
need -- it only imports stdlib."""
import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "hermes_live_standalone_rest",
    QUARRY / "plugins" / "platforms" / "discord" / "standalone_rest.py",
)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)
WANTED = {"api_url", "seconds", "pace", "constants"}


def api_url(r):
    path = r.get("path")
    if not isinstance(path, str):
        raise TypeError("path needs str for .lstrip")
    return live.api_url(path)


def seconds(r):
    return live._seconds(r.get("value"))


class _Clock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now


def pace(r):
    specs = r.get("responses")
    specs = specs if isinstance(specs, list) else []
    clock = _Clock()
    waits = []
    seen = []

    class _Resp:
        def __init__(self, spec):
            spec = spec if isinstance(spec, dict) else {}
            self.status = spec.get("status", 200)
            self.headers = spec.get("headers") or {}
            self._body = spec.get("body", "") or ""

        async def read(self):
            return self._body.encode("utf-8")

    class _Post:
        def __init__(self, resp):
            self.resp = resp

        async def __aenter__(self):
            return self.resp

        async def __aexit__(self, *exc):
            return False

    class _Session:
        def post(self, url, **kwargs):
            if len(seen) >= len(specs):
                raise RuntimeError("script exhausted")
            seen.append(specs[len(seen)])
            return _Post(_Resp(seen[-1]))

    async def _read(resp, prefix):
        return {"status": resp.status, "prefix": prefix}

    async def _sleep(delay):
        waits.append(delay)
        clock.now += delay

    saved_time = live.time
    saved_sleep = asyncio.sleep
    saved_bucket = dict(live._bucket_open_at)
    live.time = clock
    asyncio.sleep = _sleep
    live._bucket_open_at.clear()
    try:
        got = asyncio.run(live.post_paced(_Session(), "u", _read, "p"))
        open_at = live._bucket_open_at.get("u", 0.0)
    except Exception:  # noqa: BLE001 - no scripted answer left: error seam
        return None
    finally:
        live.time = saved_time
        asyncio.sleep = saved_sleep
        live._bucket_open_at.clear()
        live._bucket_open_at.update(saved_bucket)
    return {"waits": waits, "posts": len(seen), "status": got["status"],
            "clock": clock.now, "open_at": open_at}


def constants():
    return {"max_rate_limit_retries": live._MAX_RATE_LIMIT_RETRIES,
            "api_base": "https://discord.com/api/v10"}


def expected(r):
    f = r.get("func")
    try:
        if f == "api_url":
            return api_url(r)
        if f == "seconds":
            return seconds(r)
        if f == "pace":
            return pace(r)
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
