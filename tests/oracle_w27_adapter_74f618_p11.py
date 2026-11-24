#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p11 (standalone sender shaping + picker notice)."""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p11"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_constants  # noqa: E402
import plugins.platforms.slack.adapter as live  # noqa: E402

AD = live.SlackAdapter
WANTED = {"post_kwargs", "bot_tokens", "format_mrkdwn", "model_picker_notice",
          "trim_dm_cache", "is_connected"}


def expected(r):
    f = r.get("func")
    if f == "post_kwargs":
        return live._standalone_post_kwargs(r.get("chat_id", ""), r.get("text"),
                                            r.get("unfurl_kwargs") or {}, r.get("thread_id"))
    if f == "bot_tokens":
        saved = r.get("saved")
        tmp = Path(tempfile.mkdtemp())
        if saved is not None:
            (tmp / "slack_tokens.json").write_text(json.dumps(saved), encoding="utf-8")
        original = hermes_constants.get_hermes_home
        hermes_constants.get_hermes_home = lambda: tmp
        try:
            return live._load_slack_bot_tokens(str(r.get("raw_token") or ""), quiet=True)
        finally:
            hermes_constants.get_hermes_home = original
    if f == "format_mrkdwn":
        return live._standalone_format_mrkdwn(r.get("text", ""))
    if f == "model_picker_notice":
        return live._model_picker_expired_notice()
    if f == "trim_dm_cache":
        mapping = dict(r.get("mapping") or {})
        max_size = int(r.get("max_size", 5000))
        saved_cache = dict(live._slack_dm_cache)
        saved_max = live._SLACK_DM_CACHE_MAX
        live._slack_dm_cache.clear()
        live._slack_dm_cache.update(mapping)
        live._SLACK_DM_CACHE_MAX = max_size
        try:
            live._trim_slack_dm_cache()
            return dict(live._slack_dm_cache)
        finally:
            live._slack_dm_cache.clear()
            live._slack_dm_cache.update(saved_cache)
            live._SLACK_DM_CACHE_MAX = saved_max
    if f == "is_connected":
        token = r.get("token")
        import hermes_cli.gateway as gateway_mod
        saved = gateway_mod.get_env_value
        gateway_mod.get_env_value = lambda name: token if name == "SLACK_BOT_TOKEN" else ""
        try:
            return bool(live._is_connected(None))
        finally:
            gateway_mod.get_env_value = saved
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
