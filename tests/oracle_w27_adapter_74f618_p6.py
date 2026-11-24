#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p6 (task-card/stream frames, status label, bot policy)."""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p6"

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
    "task_update_chunk", "strip_stream_cursor", "stream_key", "mrkdwn_insensitive",
    "stream_relation", "default_status_text", "cron_continuable_surface",
    "slack_allow_bots", "api_human_users", "event_declares_bot_sender",
    "format_tool_preview", "markdown_block_payload", "append_feedback_block",
}


def bare():
    return AD.__new__(AD)


def fake_extra_or_secret(extra, key, env, default="", *, blank_is_unset=True):
    """The documented reader contract: env (when set) wins, then YAML `extra`."""
    return _fake_reader(extra, key, env, default, blank_is_unset)


_fake_reader = None
_env_value = None


def extra_or_secret_shim(extra, key, env, default="", *, blank_is_unset=True):
    ev = _env_value
    if env and ev is not None and str(ev).strip():
        return ev
    value = (extra or {}).get(key)
    if value is None or (blank_is_unset and isinstance(value, str) and not value.strip()):
        return default
    return value


def expected(r):
    global _env_value
    f = r.get("func")
    if f == "task_update_chunk":
        return AD._task_update_chunk(r.get("task") or {})
    if f == "strip_stream_cursor":
        inst = bare()
        if "glyphs" in r:
            inst._STREAM_CURSOR_GLYPHS = tuple(r["glyphs"])
        return inst._strip_stream_cursor(r.get("text", ""))
    if f == "stream_key":
        inst = bare()
        inst.config = SimpleNamespace(extra={"reply_in_thread": r.get("reply_in_thread", True)})
        inst._channel_team = {}
        inst._channel_teams = {}
        scope = r.get("scope_id")
        inst._team_clients = {scope: object()} if scope else {}
        key = inst._stream_key(r.get("chat_id", ""), r.get("metadata"))
        return list(key) if isinstance(key, tuple) else key
    if f == "mrkdwn_insensitive":
        return AD._mrkdwn_insensitive(r.get("text", ""))
    if f == "stream_relation":
        return list(AD._stream_relation(r.get("sent", ""), r.get("text", "")))
    if f == "default_status_text":
        saved_mono = live.time.monotonic
        live.time.monotonic = lambda: r.get("now", 0.0)
        try:
            return AD._default_status_text(r.get("started"))
        finally:
            live.time.monotonic = saved_mono
    if f == "cron_continuable_surface":
        inst = bare()
        inst.config = SimpleNamespace(extra={"cron_continuable_surface": r.get("raw")})
        return inst._cron_continuable_surface()
    if f == "slack_allow_bots":
        inst = bare()
        inst.config = SimpleNamespace(extra={"allow_bots": r.get("raw")})
        saved = live._extra_or_secret
        live._extra_or_secret = extra_or_secret_shim
        _env_value = r.get("env")
        try:
            return inst._slack_allow_bots()
        finally:
            live._extra_or_secret = saved
    if f == "api_human_users":
        inst = bare()
        inst.config = SimpleNamespace(extra={"api_human_users": r.get("raw")})
        saved = live._get_scoped_secret
        live._get_scoped_secret = lambda key, default=None: r.get("env", default)
        try:
            return sorted(inst._slack_api_human_users())
        finally:
            live._get_scoped_secret = saved
    if f == "event_declares_bot_sender":
        inst = bare()
        inst._api_human_users_cache = frozenset(r.get("human_users") or [])
        return inst._event_declares_bot_sender(r.get("event") or {})
    if f == "format_tool_preview":
        return bare().format_tool_preview(SimpleNamespace(text=r.get("preview_text", "")))
    if f == "markdown_block_payload":
        inst = bare()
        if "max_chars" in r:
            inst._MARKDOWN_BLOCK_MAX = r["max_chars"]
        return inst._markdown_block_payload(r.get("content", ""))
    if f == "append_feedback_block":
        inst = bare()
        enabled = bool(r.get("enabled"))
        inst._extra_flag = lambda key, default=False, _e=enabled: _e if key == "feedback_buttons" else default
        return inst._append_feedback_block(r.get("blocks"))
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
