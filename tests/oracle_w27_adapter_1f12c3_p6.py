#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p6).

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
GATE = "w27_adapter_1f12c3_p6"

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


WANTED = {'voice_fx', 'int_config', 'voice_timeout', 'playback_timeout', 'voice_timeout_limit', 'playback_timeout_limit', 'mixer_active', 'channel_ids_allowed', 'warn_fail_closed'}

import hermes_cli.config as _hcfg

_SAVED_RRC = _hcfg.read_raw_config


def _patch_config(discord_cfg, fx=None):
    box = {"discord": dict(discord_cfg or {})}
    if fx is not None:
        box["discord"]["voice_fx"] = fx
    _hcfg.read_raw_config = lambda: box


def _restore_config():
    _hcfg.read_raw_config = _SAVED_RRC



def expected(r):
    f = r.get("func")
    if f == 'voice_fx':
        _patch_config({}, r.get("fx") if isinstance(r.get("fx"), dict) else {})
        try:
            return bare()._load_voice_fx_config()
        finally:
            _restore_config()
    if f == 'int_config':
        _patch_config(r.get("cfg") if isinstance(r.get("cfg"), dict) else {})
        try:
            return int(bare()._load_discord_int_config(r.get("key"), r.get("default"), minimum=r.get("minimum", 0)))
        finally:
            _restore_config()
    if f == 'voice_timeout':
        _patch_config(r.get("cfg") if isinstance(r.get("cfg"), dict) else {})
        try:
            return int(bare()._load_voice_timeout())
        finally:
            _restore_config()
    if f == 'playback_timeout':
        _patch_config(r.get("cfg") if isinstance(r.get("cfg"), dict) else {})
        try:
            return int(bare()._load_playback_timeout())
        finally:
            _restore_config()
    if f == 'voice_timeout_limit':
        return int(bare(_voice_timeout_seconds=r.get("stored"))._voice_timeout_limit())
    if f == 'playback_timeout_limit':
        return int(bare(_playback_timeout_seconds=r.get("stored"))._playback_timeout_limit())
    if f == 'mixer_active':
        m = r.get("mixers") if isinstance(r.get("mixers"), dict) else {}
        return bool(bare().voice_mixer_active(r.get("guild_id")) if False else bare(_voice_mixers={int(k): v for k, v in m.items()}).voice_mixer_active(int(r.get("guild_id") or 0)))
    if f == 'channel_ids_allowed':
        a = bare()
        a._get_allowed_channels = lambda: set(L(r, "allowed"))
        ids = set(r.get("channel_ids") or []) if isinstance(r.get("channel_ids"), list) else set()
        return bool(a._discord_channel_ids_allowed(ids))
    if f == 'warn_fail_closed':
        s = r.get("state") if isinstance(r.get("state"), dict) else {}
        a = bare()
        a._warned_fail_closed_default = bool(s.get("warned"))
        a._allowed_user_ids = set(s.get("allowed_users") or [])
        a._allowed_role_ids = set(s.get("allowed_roles") or [])
        a._get_allowed_channels = lambda: set(s.get("allowed_channels") or [])
        a._discord_allow_all_users = lambda: bool(s.get("discord_allow_all"))
        a._gateway_allow_all_users = lambda: bool(s.get("gateway_allow_all"))
        a._warn_if_fail_closed_default()
        return bool(a._warned_fail_closed_default)
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
