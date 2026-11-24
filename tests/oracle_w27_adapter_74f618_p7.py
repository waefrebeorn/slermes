#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p7 (thread_ts, reaction shapes, users.info)."""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p7"

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
    "resolve_thread_ts", "reactions_enabled", "extra_flag", "parse_users_info",
    "reaction_triggers", "reaction_trigger_target", "first_file_share",
    "synthetic_reaction_event", "build_identity_prompt",
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
    if f == "resolve_thread_ts":
        inst = bare()
        inst.config = SimpleNamespace(extra={"reply_in_thread": r.get("reply_in_thread", True)})
        return inst._resolve_thread_ts(r.get("reply_to"), r.get("metadata"))
    if f == "reactions_enabled":
        inst = bare()
        inst.config = SimpleNamespace(extra=r.get("extra") or {})
        saved = live._extra_or_secret
        live._extra_or_secret = shim_extra_or_secret
        _ENV = r.get("env")
        try:
            return inst._reactions_enabled()
        finally:
            live._extra_or_secret = saved
    if f == "extra_flag":
        inst = bare()
        inst.config = SimpleNamespace(extra=r.get("extra") or {})
        return inst._extra_flag(r.get("key", ""), bool(r.get("default", False)))
    if f == "parse_users_info":
        name, is_bot = AD._parse_users_info(r.get("payload") or {}, r.get("user_id", ""))
        return [name, is_bot]
    if f == "reaction_triggers":
        inst = bare()
        inst.config = SimpleNamespace(extra={"reaction_triggers": r.get("raw")})
        saved = live._get_scoped_secret
        live._get_scoped_secret = lambda key, default=None: default
        try:
            got = inst._slack_reaction_triggers()
        finally:
            live._get_scoped_secret = saved
        return None if got is None else sorted(got)
    if f == "reaction_trigger_target":
        inst = bare()
        inst.config = SimpleNamespace(extra={"reaction_trigger_target": r.get("raw")})
        saved = live._get_scoped_secret
        live._get_scoped_secret = lambda key, default=None: default
        try:
            return list(inst._slack_reaction_trigger_target())
        finally:
            live._get_scoped_secret = saved
    if f == "first_file_share":
        return AD._first_file_share(r.get("file_obj") or {}, r.get("channel_id", ""))
    if f == "synthetic_reaction_event":
        inst = bare()
        inst._slack_reaction_trigger_target = lambda: (r.get("target_channel") or "",
                                                       r.get("target_thread") or "")
        return inst._synthetic_reaction_event(
            r.get("event") or {}, r.get("action", "added"), r.get("thread_ts", ""),
            r.get("team_id", ""))
    if f == "build_identity_prompt":
        inst = bare()
        inst._bot_display_name = r.get("name", "")
        inst._team_bot_names = {}
        return inst._build_identity_prompt("")
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
