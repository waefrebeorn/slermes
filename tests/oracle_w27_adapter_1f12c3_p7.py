#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p7).

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
GATE = "w27_adapter_1f12c3_p7"

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


WANTED = {'allowed_user', 'guild_role', 'interaction_guild', 'forum_parent', 'parent_channel_id', 'effective_topic', 'attachment_type', 'auto_thread_name'}

class _Guild:
    def __init__(self, data):
        d = data if isinstance(data, dict) else {}
        self.id = d.get("id")
        self._members = d.get("members") if isinstance(d.get("members"), dict) else {}

    def get_member(self, uid):
        m = self._members.get(str(uid))
        if m is None:
            return None
        return ns(id=uid, roles=[ns(id=rid) for rid in m])


def _guild(data):
    return _Guild(data) if isinstance(data, dict) else None


def _author(r):
    if r.get("author_roles") is None:
        return None
    gid = r.get("author_guild_id")
    return ns(roles=[ns(id=x) for x in r.get("author_roles") or []],
              guild=(ns(id=gid) if gid is not None else None))


def _auth_adapter(r):
    a = bare()
    a._refresh_live_gates = lambda: None
    a._allowed_user_ids = set(L(r, "allowed_users"))
    a._allowed_role_ids = set(L(r, "allowed_roles"))
    a._is_pairing_approved_user = lambda uid: bool(r.get("pairing"))
    a._get_allowed_channels = lambda: set(L(r, "allowed_channels"))
    a._discord_allow_all_users = lambda: bool(r.get("discord_allow_all"))
    a._gateway_allow_all_users = lambda: bool(r.get("gateway_allow_all"))
    guilds = r.get("client_guilds") if isinstance(r.get("client_guilds"), dict) else {}
    a._client = ns(user=ns(id=1), get_guild=lambda gid: _guild(guilds.get(str(gid))))
    saved = live._read_dm_role_auth_guild
    live._read_dm_role_auth_guild = lambda: r.get("dm_role_auth_guild")
    a._restore_dmg = lambda: setattr(live, "_read_dm_role_auth_guild", saved)
    return a


def _channel(data):
    if not isinstance(data, dict):
        return None
    parent = data.get("parent") if isinstance(data.get("parent"), dict) else None
    return ns(id=data.get("id"), name=data.get("name"), parent_id=data.get("parent_id"),
              topic=data.get("topic"), type=(ns(value=data["type"]["value"]) if isinstance(data.get("type"), dict) else data.get("type")),
              parent=(ns(id=parent.get("id"), topic=parent.get("topic"), type=parent.get("type")) if parent is not None else None))


def _att(data):
    d = data if isinstance(data, dict) else {}
    out = {"content_type": d.get("content_type")}
    if "is_voice_message" in d:
        out["is_voice_message"] = d.get("is_voice_message")
    if "duration" in d:
        out["duration"] = d.get("duration")
    if "waveform" in d:
        out["waveform"] = d.get("waveform")
    return out



def expected(r):
    f = r.get("func")
    if f == 'allowed_user':
        return bool(_auth_adapter(r)._is_allowed_user(str(r.get("user_id") or ""), _author(r), guild=_guild(r.get("guild")), is_dm=bool(r.get("is_dm")), channel_ids=(set(r["channel_ids"]) if isinstance(r.get("channel_ids"), list) else None)))
    if f == 'guild_role':
        return bool(live.DiscordAdapter._guild_member_has_role(_guild(r.get("guild")), str(r.get("user_id") or ""), set(L(r, "allowed_roles"))))
    if f == 'interaction_guild':
        i = r.get("interaction") if isinstance(r.get("interaction"), dict) else {}
        ch = i.get("channel") if isinstance(i.get("channel"), dict) else None
        resp = ns(guild_id=i.get("guild_id"), channel=ns(guild=ns(id=(ch.get("guild") or {}).get("id")) if isinstance(ch.get("guild"), dict) else None) if ch is not None else None)
        return bare()._interaction_guild_id(resp)
    if f == 'forum_parent':
        return bool(bare()._is_forum_parent(_channel(r.get("channel"))))
    if f == 'parent_channel_id':
        return bare()._get_parent_channel_id(_channel(r.get("channel")))
    if f == 'effective_topic':
        return bare()._get_effective_topic(_channel(r.get("channel")), bool(r.get("is_thread")))
    if f == 'attachment_type':
        t_ = live.DiscordAdapter._attachment_message_type(bare(), ns(**_att(r.get("att"))))
        return t_.name
    if f == 'auto_thread_name':
        return bare()._derive_auto_thread_name(r.get("content"))
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
