#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p8).

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
GATE = "w27_adapter_1f12c3_p8"

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


WANTED = {'backfill_channels', 'extra_flag', 'require_mention', 'free_response_auto_thread', 'max_attachment_bytes', 'voice_attachment'}



def expected(r):
    f = r.get("func")
    if f == 'backfill_channels':
        a = bare()
        a.config.extra = r.get("extra") if isinstance(r.get("extra"), dict) else {}
        a._get_allowed_channels = lambda: set(L(r, "allowed_channels"))
        a._discord_free_response_channels = lambda: set(L(r, "free_response"))
        return sorted(a._missed_message_backfill_channels())
    if f == 'extra_flag':
        a = bare()
        a.config.extra = r.get("extra") if isinstance(r.get("extra"), dict) else {}
        return bool(a._extra_or_env_flag(r.get("key"), r.get("env_key") or "SLERMES_GATE", r.get("env_default"), truthy=bool(r.get("truthy"))))
    if f == 'require_mention':
        a = bare()
        a.config.extra = r.get("extra") if isinstance(r.get("extra"), dict) else {}
        return bool(a._discord_require_mention())
    if f == 'free_response_auto_thread':
        a = bare()
        a.config.extra = r.get("extra") if isinstance(r.get("extra"), dict) else {}
        return bool(a._discord_free_response_auto_thread())
    if f == 'max_attachment_bytes':
        a = bare()
        a.config.extra = r.get("extra") if isinstance(r.get("extra"), dict) else {}
        return int(a._discord_max_attachment_bytes())
    if f == 'voice_attachment':
        kind = r.get("callable")
        m = r.get("marker")
        if kind == "true": att = ns(is_voice_message=lambda: True)
        elif kind == "raise":
            def _boom(): raise RuntimeError("x")
            att = ns(is_voice_message=_boom)
        elif m is None: att = ns(duration=r.get("duration"), waveform=r.get("waveform"))
        else: att = ns(is_voice_message=m)
        return bool(live.DiscordAdapter._is_discord_voice_message_attachment(att))
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
