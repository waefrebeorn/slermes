#!/usr/bin/env python3
"""Differential oracle for gateway/run_topics.py (gate w6_runtopics_d5aa88)."""
import importlib.util
import json
import sys
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/run_topics.py"
NAME = "w6_rtopics_live"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

# Pin the i18n default and the platform enum to deterministic values.
live.t = lambda key, **kw: "DEFAULT_TITLE" if key == "gateway.topic.default_title" else key
class P:
    TELEGRAM = "telegram"
    DISCORD = "discord"
live.Platform = P
M = live.GatewayTopicThreadsMixin

def source(r):
    return SimpleNamespace(
        profile=r.get("profile", ""), chat_id=r.get("chat_id", ""),
        thread_id=r.get("thread_id", ""), platform=r.get("platform", ""),
        chat_type=r.get("chat_type", ""),
        auto_thread_created=r.get("auto_thread_created", False),
        auto_thread_initial_name=r.get("auto_thread_initial_name", ""),
        delivered_via_upstream_relay=r.get("delivered_via_upstream_relay", False),
    )

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func, got = r.get("func"), r.get("out")
    title = r.get("title", "")
    if func == "collapse_title":
        want = live._collapse_title(title)
    elif func == "sanitize_telegram_topic_title":
        want = M._sanitize_telegram_topic_title(None, title)
    elif func == "sanitize_discord_thread_title":
        want = M._sanitize_discord_thread_title(None, title)
    elif func == "telegram_topic_profile_name":
        want = M._telegram_topic_profile_name(source(r))
    elif func == "telegram_topic_key":
        want = list(M._telegram_topic_key(source(r)))
    elif func == "telegram_topic_cooldown_key":
        want = M._telegram_topic_cooldown_key(M, source(r))
    elif func == "is_telegram_dm":
        want = M._is_telegram_dm(source(r))
    elif func == "is_discord_auto_thread_lane":
        want = M._is_discord_auto_thread_lane(None, source(r))
    elif func == "is_relay_discord_channel_lane":
        want = M._is_relay_discord_channel_lane(None, source(r))
    elif func == "topic_restore_steps":
        want = list(live._topic_restore_steps())
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_runtopics_d5aa88: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
