#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p10 (mention/channel policy predicates)."""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p10"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import plugins.platforms.slack.adapter as live  # noqa: E402

AD = live.SlackAdapter
WANTED = {
    "require_mention", "extra_or_env_flag", "addressed_other_user", "is_free_channel",
    "reply_expected", "message_mentions_self", "extra_or_env_channel_set",
    "slash_channel_gated", "is_slack_cdn_url", "thread_rehydration_key",
}

_ENV = None


def bare():
    return AD.__new__(AD)


def shim_extra_or_secret(extra, key, env, default="", *, blank_is_unset=True):
    if env and _ENV is not None and str(_ENV).strip():
        return _ENV
    value = (extra or {}).get(key)
    if value is None or (blank_is_unset and isinstance(value, str) and not value.strip()):
        return default
    return value


def expected(r):
    global _ENV
    f = r.get("func")
    if f == "require_mention":
        inst = bare()
        inst.config = SimpleNamespace(extra=r.get("extra") or {})
        saved = live._get_scoped_secret
        live._get_scoped_secret = lambda key, default=None: (
            r["env"] if r.get("env") is not None else default)
        try:
            return inst._slack_require_mention()
        finally:
            live._get_scoped_secret = saved
    if f == "extra_or_env_flag":
        inst = bare()
        inst.config = SimpleNamespace(extra=r.get("extra") or {})
        saved = live._extra_or_secret
        live._extra_or_secret = shim_extra_or_secret
        _ENV = r.get("env")
        try:
            return inst._extra_or_env_flag(r.get("key", ""), r.get("env_var", "X"),
                                           strip=bool(r.get("strip")))
        finally:
            live._extra_or_secret = saved
    if f == "addressed_other_user":
        inst = bare()
        return inst._slack_message_addressed_to_other_user(r.get("text") or "",
                                                           set(r.get("self_uids") or []))
    if f == "is_free_channel":
        inst = bare()
        inst._slack_require_mention_channels = lambda: set(r.get("require_mention_channels") or [])
        inst._slack_free_response_channels = lambda: set(r.get("free_response_channels") or [])
        inst._slack_require_mention = lambda: bool(r.get("require_mention", True))
        return inst._slack_is_free_channel(r.get("channel_id", ""))
    if f == "reply_expected":
        inst = bare()
        inst._bot_user_id = r.get("self_bot_uid")
        inst._slack_thread_followup_silence = lambda: bool(r.get("thread_followup_silence"))
        inst._slack_is_free_channel = lambda channel_id: bool(r.get("free_channel"))
        return inst._slack_reply_expected(
            r.get("routing_text") or "", r.get("bot_uid"),
            channel_id=r.get("channel_id", ""), addressed=bool(r.get("addressed")),
            opens_own_session=bool(r.get("opens_own_session")),
            channel_thread_followup=bool(r.get("channel_thread_followup")))
    if f == "message_mentions_self":
        inst = bare()
        return inst._slack_message_mentions_self(r.get("text") or "",
                                                 set(r.get("self_uids") or []))
    if f == "extra_or_env_channel_set":
        inst = bare()
        inst.config = SimpleNamespace(extra={"channels": r.get("raw")})
        saved = live._extra_or_secret
        live._extra_or_secret = shim_extra_or_secret
        _ENV = r.get("env")
        try:
            got = inst._extra_or_env_channel_set("channels", "X",
                                                 coerce_scalar=bool(r.get("coerce_scalar")))
        finally:
            live._extra_or_secret = saved
        return sorted(got)
    if f == "slash_channel_gated":
        inst = bare()
        inst._is_ignored_channel = lambda channel_id: r.get("channel_id") in set(
            r.get("ignored") or [])
        inst._slack_allowed_channels = lambda: set(r.get("allowed") or [])
        return inst._slash_channel_gated(r.get("channel_id") or "")
    if f == "is_slack_cdn_url":
        return AD._is_slack_cdn_url(r.get("url") or "")
    if f == "thread_rehydration_key":
        inst = bare()
        inst._session_store = SimpleNamespace(
            config=SimpleNamespace(thread_sessions_per_user=bool(r.get("per_user"))))
        return inst._thread_rehydration_key(r.get("channel_id", ""), r.get("thread_ts", ""),
                                            r.get("user_id", ""), r.get("team_id", ""))
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
