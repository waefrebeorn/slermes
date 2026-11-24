#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p11).

The live adapter module is imported as a package module; the pure functions are
called on an instance built with ``__new__`` (no bot, no socket, no __init__), so
every seam -- discord.py objects, aiohttp, the SQLite recovery store, the
localization catalog and config-file reads -- never starts.  What is compared is
exactly the value those seams decide.
"""
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_1f12c3_p11"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# A home dir is needed by the import chain (hermes_constants); use the repo
# parent when the launching shell has none, so the gate is shell-independent.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)

from plugins.platforms.discord import adapter as live  # noqa: E402

# ── hermetic gate: strip ambient policy env so only the fixture decides ───
for _k in list(os.environ):
    if _k.startswith(("DISCORD_", "GATEWAY_", "HERMES_GATEWAY_", "SLERMES_GATE")):
        os.environ.pop(_k, None)

_MISSING = object()


class _TimeShim:
    def __init__(self, now):
        self._now = now

    def time(self):
        return self._now

    def monotonic(self):
        return self._now

    def perf_counter(self):
        return self._now


_EXC = {
    "TimeoutError": TimeoutError,
    "ConnectionError": ConnectionError,
    "ConnectionResetError": ConnectionResetError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "BrokenPipeError": BrokenPipeError,
    "OSError": OSError,
    "IOError": OSError,
    "ValueError": ValueError,
    "RuntimeError": RuntimeError,
}


def exc(name, msg="", **attrs):
    cls = _EXC.get(name)
    if cls is None:
        cls = type(str(name or "Exception"), (Exception,), {})
    e = cls(msg)
    for k, v in attrs.items():
        setattr(e, k, v)
    return e


def bare(**attrs):
    a = live.DiscordAdapter.__new__(live.DiscordAdapter)
    a.platform = live.Platform.DISCORD
    a.config = types.SimpleNamespace(extra={})
    a._client = None
    for k, v in attrs.items():
        setattr(a, k, v)
    return a


def ns(**kw):
    return types.SimpleNamespace(**kw)


def S(r, key, dflt=None):
    v = r.get(key, dflt)
    return dflt if v is None else v


def L(r, key):
    v = r.get(key)
    return list(v) if isinstance(v, list) else []


def _env_apply(r):
    name = r.get("env_name") or r.get("env_key")
    if not name:
        return None
    saved = os.environ.get(name, _MISSING)
    val = r.get("env")
    if val is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = str(val)
    return (name, saved)


def _env_restore(tok):
    if not tok:
        return
    name, saved = tok
    if saved is _MISSING:
        os.environ.pop(name, None)
    else:
        os.environ[name] = saved


WANTED = {'record_debounce', 'debounce_continuation', 'free_response_channels', 'raw_mentions', 'self_raw_mentioned'}



def expected(r):
    f = r.get("func")
    if f == 'record_debounce':
        a = bare()
        a._text_batch_delay_seconds = float(r.get("delay") or 0)
        a._text_batch_split_delay_seconds = float(r.get("split_delay") or 0)
        a._bot_tag_debounce_until = {}
        saved_msg = live.is_automated_message
        saved_time = live.time
        live.is_automated_message = lambda m: bool(r.get("automated"))
        live.time = _TimeShim(float(r.get("now") or 0))
        a._self_is_explicitly_mentioned = lambda m: bool(r.get("mentioned"))
        try:
            a._record_bot_tag_debounce(ns(channel=ns(id=1), author=ns(id=2)))
        finally:
            live.is_automated_message = saved_msg
            live.time = saved_time
        return a._bot_tag_debounce_until.get("1:2")
    if f == 'debounce_continuation':
        a = bare()
        a._text_batch_delay_seconds = float(r.get("delay") or 0)
        a._text_batch_split_delay_seconds = float(r.get("window") or 0)
        a._bot_tag_debounce_until = {"1:2": float(r.get("until") or 0)}
        saved_msg = live.is_automated_message
        saved_time = live.time
        live.is_automated_message = lambda m: bool(r.get("automated"))
        live.time = _TimeShim(float(r.get("now") or 0))
        try:
            return bool(a._is_bot_tag_debounce_continuation(ns(channel=ns(id=1), author=ns(id=2))))
        finally:
            live.is_automated_message = saved_msg
            live.time = saved_time
    if f == 'free_response_channels':
        a = bare()
        a.config.extra = r.get("extra") if isinstance(r.get("extra"), dict) else {}
        return sorted(a._discord_free_response_channels())
    if f == 'raw_mentions':
        return sorted(live.DiscordAdapter._raw_mentioned_user_ids(bare(), ns(content=r.get("content"))))
    if f == 'self_raw_mentioned':
        a = bare()
        a._client = ns(user=ns(id=r.get("client_user_id"))) if r.get("client_user_id") is not None else None
        return bool(a._self_is_raw_mentioned(ns(content=r.get("content"))))
    raise AssertionError("unknown func %r" % (r.get("func"),))


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
        tok = _env_apply(r)
        try:
            py = expected(r)
        except Exception as exc_:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc_), file=sys.stderr)
            continue
        finally:
            _env_restore(tok)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
