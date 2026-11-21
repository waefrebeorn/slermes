#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow9.jsonl.

Each case: {"func": <name>, "in": {"args": {...}, "state": {...}, "io": {...}}}.
io keys are a subset of the C/oracle seam registry. At least 6 cases per
ported function. `fc: true` marks a deliberate fail-closed case.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow9.jsonl"
OUT.parent.mkdir(parents=True, exist_ok=True)

cases = []


def add(func, args=None, io=None, state=None, fc=False, tag=""):
    rec = {"func": func, "in": {"args": args or {}, "io": io or {}}}
    if state is not None:
        rec["in"]["state"] = state
    if fc:
        rec["fc"] = True
    if tag:
        rec["tag"] = tag
    cases.append(rec)


rv = lambda t, m: {"__script__": True, "raise": {"type": t, "message": m}}
sl = lambda v: {"__script__": True, "ret": v}

# ---- 1 _clean_discord_id (no seams) ----
F = "_clean_discord_id"
for v in ["  <@123>  ", "user:Alice", "  USER:Bob  ", "<@!456>", "plain", "", "user:",
          "<@>", "<@!abc>", "User:123 ", "  spaced name  "]:
    add(F, {"entry": v})

# ---- 2 _command_sync_state_path ----
F = "_command_sync_state_path"
add(F, io={"get_hermes_home": "/home/u/.hermes", "mkdir": None})
add(F, io={"get_hermes_home": "", "mkdir": None})
add(F, io={"get_hermes_home": "C:/h", "mkdir": rv("RuntimeError", "nope")})
add(F, io={"get_hermes_home": "C:/h", "mkdir": rv("ValueError", "bad")})
add(F, io={"get_hermes_home": None, "mkdir": None})
add(F, io={"get_hermes_home": "C:/h", "mkdir": rv("SystemExit", "bye")}, fc=True)
add(F, io={"mkdir": None}, fc=True, tag="fail-closed: no home script")

# ---- 3 _cancel_bot_task ----
F = "_cancel_bot_task"
T = {"id": "1"}
add(F, state={})
add(F, state={"_bot_task": False})
add(F, state={"_bot_task": T},
    io={"bot_task.done": True})
add(F, state={"_bot_task": T},
    io={"bot_task.done": False, "bot_task.cancel": None, "bot_task.await": None})
add(F, state={"_bot_task": T},
    io={"bot_task.done": False, "bot_task.cancel": None, "bot_task.await": rv("CancelledError", "c")})
add(F, state={"_bot_task": T},
    io={"bot_task.done": False, "bot_task.cancel": None, "bot_task.await": rv("RuntimeError", "boom")})
add(F, state={"_bot_task": T},
    io={"bot_task.done": False, "bot_task.cancel": None, "bot_task.await": rv("SystemExit", "bye")}, fc=True)
add(F, state={"_bot_task": T}, fc=True, tag="fail-closed: no done script")
add(F, state={"_bot_task": T},
    io={"bot_task.done": False, "bot_task.cancel": rv("SystemExit", "bye")}, fc=True)

# ---- 4 _cancel_liveness_task ----
F = "_cancel_liveness_task"
N1 = {"id": "1"}
N2 = {"id": "2"}
add(F, io={"asyncio.current_task": "x"}, state={"name": "d"})
add(F, io={"asyncio.current_task": "x", "liveness.done": True,
           "liveness.await": None},
    state={"name": "d", "_liveness_task": N1})
add(F, io={"asyncio.current_task": "x", "liveness.done": False,
           "liveness.cancel": None, "liveness.await": rv("CancelledError", "c")},
    state={"name": "d", "_liveness_task": N1})
add(F, io={"asyncio.current_task": "x", "liveness.done": False,
           "liveness.cancel": None, "liveness.await": rv("RuntimeError", "boom"),
           "logger.debug": None},
    state={"name": "d", "_liveness_task": N1})
add(F, io={"asyncio.current_task": "x", "liveness.done": False,
           "liveness.cancel": None, "liveness.await": rv("SystemExit", "bye")},
    state={"name": "d", "_liveness_task": N1}, fc=True)
add(F, io={"asyncio.current_task": "1", "liveness.done": True,
           "liveness.await": None},
    state={"name": "d", "_liveness_task": N1}, tag="task is current: skipped")
add(F, io={"asyncio.current_task": "x",
           "liveness.done": [False, True],
           "liveness.cancel": [None, None],
           "liveness.await": [None, None]},
    state={"name": "d", "_liveness_task": N1, "_liveness_notification_task": N2})
add(F, state={"name": "d"}, fc=True, tag="fail-closed: no current_task script")

# ---- 5 _advance_discord_recovery_cursor ----
F = "_advance_discord_recovery_cursor"
add(F, {"channel_id": "c1", "message_id": "m1"},
    {"_utc_now_iso": "now1", "conn.execute": None})
add(F, {"channel_id": "", "message_id": "m1"}, {})
add(F, {"channel_id": "c1", "message_id": ""}, {})
add(F, {"channel_id": "c1", "message_id": "m1"},
    {"_utc_now_iso": rv("RuntimeError", "boom")}, fc=True)
add(F, {"channel_id": "c1", "message_id": "m1"},
    {"_utc_now_iso": "now", "conn.execute": rv("SystemExit", "bye")}, fc=True)
add(F, {"channel_id": "c1", "message_id": "m1"},
    {"_utc_now_iso": "now", "conn.execute": rv("ValueError", "bad")}, fc=True)
add(F, {"channel_id": "c1", "message_id": "m1"},
    {"conn.execute": None}, fc=True, tag="fail-closed: no now script")

# ---- 6 _alert_adapters_and_config ----
F = "_alert_adapters_and_config"
AS = {"adapters": ["a"], "config": {"k": 1}}
add(F, {}, {}, state=AS)
add(F, {"profile": ""}, {}, state=AS)
add(F, {"profile": "p"}, {"_adapters_for_profile": "SAME"}, state=AS)
add(F, {"profile": "p"},
    {"_adapters_for_profile": "other", "get_profile_dir": "/d",
     "load_gateway_config": {"cfg": 2}}, state=AS)
add(F, {"profile": "p"}, {"_adapters_for_profile": rv("RuntimeError", "boom")}, state=AS, fc=True)
add(F, {"profile": "p"},
    {"_adapters_for_profile": "o", "get_profile_dir": "/d",
     "load_gateway_config": rv("SystemExit", "bye")}, state=AS, fc=True)
add(F, {"profile": "p"},
    {"_adapters_for_profile": "o", "get_profile_dir": rv("RuntimeError", "no")}, state=AS, fc=True)
add(F, {"profile": "p"}, {}, state=AS, fc=True, tag="fail-closed: no adapters script")

# ---- 7 _apply_owner_only_visibility ----
F = "_apply_owner_only_visibility"
BASE7 = {"discord.Permissions": "perm"}
add(F, {}, {**BASE7, "tree.get_commands": sl([{"name": "a"}]),
            "set.default_permissions": None, "logger.info": None})
add(F, {}, {**BASE7, "tree.get_commands": sl([]), "logger.info": None})
add(F, {}, {**BASE7, "tree.get_commands": sl([{"name": "a"}, {"name": "b"}]),
            "set.default_permissions": None, "logger.info": None})
add(F, {}, {"discord.Permissions": rv("ValueError", "no"), "logger.warning": None})
add(F, {}, {"discord.Permissions": rv("SystemExit", "bye")}, fc=True)
add(F, {}, {**BASE7, "tree.get_commands": sl([{"name": "a"}]),
            "set.default_permissions": rv("ValueError", "bad"), "logger.debug": None,
            "logger.info": None})
add(F, {}, {**BASE7, "tree.get_commands": sl([{"name": "a"}]),
            "set.default_permissions": rv("SystemExit", "bye")}, fc=True)
add(F, {}, {**BASE7, "tree.get_commands": rv("RuntimeError", "boom")}, fc=True)
add(F, {}, {"tree.get_commands": []}, fc=True, tag="fail-closed: no permissions script")

# ---- 8 _build_slash_event ----
F = "_build_slash_event"
INTER_DM = {"channel": {"__kind__": "dm", "name": "dm-c"},
            "user": {"name": "Alice", "id": 7, "display_name": "Al"}, "channel_id": 123}
INTER_TH = {"channel": {"__kind__": "thread", "id": 9},
            "user": {"name": "Bob", "id": 8, "display_name": "Bo"}, "channel_id": 456}
INTER_GR = {"channel": {"__kind__": "group", "name": "gen", "guild": {"name": "G"}},
            "user": {"name": "C", "id": 9, "display_name": "Cy"}, "channel_id": 789}
BASE8 = {"_get_effective_topic": "topic", "_interaction_guild_id": "g1",
         "_slash_role_grant": True, "build_source": "src",
         "_resolve_channel_prompt": "prompt", "_resolve_channel_skills": sl(["s1"]),
         "MessageEvent": "evt"}
add(F, {"interaction": INTER_DM, "text": "hello"}, BASE8)
add(F, {"interaction": INTER_DM, "text": "/x"},
    {**BASE8, "MessageEvent": "cmd-evt"})
add(F, {"interaction": INTER_TH, "text": "hi"},
    {**BASE8, "_format_thread_chat_name": "tc", "_get_parent_channel_id": "p1"})
add(F, {"interaction": INTER_TH, "text": "hi"},
    {**BASE8, "_format_thread_chat_name": "tc", "_get_parent_channel_id": None})
add(F, {"interaction": INTER_GR, "text": "yo"}, BASE8)
add(F, {"interaction": {"channel": {"__kind__": "group"},
                        "user": {"id": 1, "display_name": 1},
                        "channel_id": 1}, "text": "z"}, BASE8)
add(F, {"interaction": INTER_TH, "text": "hi"},
    {**BASE8, "_format_thread_chat_name": rv("RuntimeError", "boom")}, fc=True)
add(F, {"interaction": INTER_DM, "text": "hello"},
    {**BASE8, "build_source": rv("SystemExit", "bye")}, fc=True)
add(F, {"interaction": INTER_DM, "text": "hello"},
    {k: v for k, v in BASE8.items() if k != "_get_effective_topic"}, fc=True,
    tag="fail-closed: no topic script")

# ---- 9 _component_live_auth ----
F = "_component_live_auth"
IA1 = {"user": {"id": "1"}}
add(F, {"interaction": IA1},
    {"component_chat_context": sl(["dm", "c1", None]), "_is_sender_authorized": True})
add(F, {"interaction": {"user": {"id": 5}}},
    {"component_chat_context": sl(["thread", "c2", "t1"]), "_is_sender_authorized": False})
add(F, {"interaction": {}},
    {"component_chat_context": sl(["group", "g", None]), "_is_sender_authorized": None})
add(F, {"interaction": IA1},
    {"component_chat_context": sl([]), "_is_sender_authorized": True})
add(F, {"interaction": IA1},
    {"component_chat_context": rv("RuntimeError", "boom")}, fc=True)
add(F, {"interaction": IA1},
    {"component_chat_context": sl(["dm", "c1", None]),
     "_is_sender_authorized": rv("SystemExit", "bye")}, fc=True)
add(F, {"interaction": IA1}, {}, fc=True, tag="fail-closed: no context script")

# ---- 10 _auto_create_thread ----
F = "_auto_create_thread"
M = {"content": "hi", "author": {"display_name": "Al"}}
add(F, {"message": M},
    {"_derive_auto_thread_name": "thname", "create_thread": "thr", "stamp": "stamped"})
add(F, {"message": M},
    {"_derive_auto_thread_name": "thname",
     "create_thread": [rv("RuntimeError", "direct"), "thr2"],
     "t": "seed text", "channel.send": "seedmsg",
     "seed.create_thread": "fthr", "stamp": "stamped"})
add(F, {"message": M},
    {"_derive_auto_thread_name": "thname",
     "create_thread": [rv("RuntimeError", "d1"), rv("RuntimeError", "d2")],
     "t": "seed text",
     "channel.send": [rv("RuntimeError", "s1"), rv("RuntimeError", "s2")],
     "seed.create_thread": [rv("RuntimeError", "f1"), rv("RuntimeError", "f2")],
     "sleep": None, "logger.warning": None})
add(F, {"message": M}, {"_derive_auto_thread_name": rv("RuntimeError", "boom")}, fc=True)
add(F, {"message": M},
    {"_derive_auto_thread_name": "n", "create_thread": rv("SystemExit", "bye")}, fc=True)
add(F, {"message": M},
    {"_derive_auto_thread_name": "n", "create_thread": "thr",
     "stamp": rv("SystemExit", "bye")}, fc=True)
add(F, {"message": {"content": "", "author": {}}},
    {"_derive_auto_thread_name": "n", "create_thread": "thr", "stamp": "s"})
add(F, {"message": M}, {"_derive_auto_thread_name": "n"}, fc=True,
    tag="fail-closed: no create_thread script")

# ---- 11/12 cache image/audio ----
for F in ("_cache_discord_image", "_cache_discord_audio"):
    ARG = {"att": {"url": "http://x"}, "ext": ".png"}
    add(F, ARG, {"_read_attachment_bytes": "data", "cache_bytes": "/cached"})
    add(F, ARG, {"_read_attachment_bytes": None, "cache_url": "/url"})
    add(F, ARG, {"_read_attachment_bytes": "data",
                 "cache_bytes": rv("ValueError", "bad"), "logger.debug": None,
                 "cache_url": "/url"})
    add(F, ARG, {"_read_attachment_bytes": "data",
                 "cache_bytes": rv("SystemExit", "bye")}, fc=True)
    add(F, ARG, {"_read_attachment_bytes": rv("RuntimeError", "boom")}, fc=True)
    add(F, ARG, {"_read_attachment_bytes": "data"}, fc=True, tag="fail-closed: no cache script")
    add(F, ARG, {"_read_attachment_bytes": None, "cache_url": rv("SystemExit", "bye")}, fc=True)

# ---- 13 _cache_discord_document ----
F = "_cache_discord_document"
ARG13 = {"att": {"url": "http://x/file"}, "ext": ""}
BASE13 = {"is_safe_url": True, "resolve_proxy_url": "proxy",
          "proxy_kwargs": sl([{"p": 1}, {"r": 2}])}
add(F, ARG13, {"_read_attachment_bytes": "data"})
add(F, ARG13, {**BASE13, "_read_attachment_bytes": None,
               "session.get": {"status": 200}, "resp.read": "raw-bytes"})
add(F, ARG13, {"_read_attachment_bytes": None,
               "is_safe_url": False}, fc=True)
add(F, ARG13, {**BASE13, "_read_attachment_bytes": None,
               "session.get": {"status": 403}}, fc=True)
add(F, ARG13, {"_read_attachment_bytes": None,
               "is_safe_url": rv("RuntimeError", "boom")}, fc=True)
add(F, ARG13, {**BASE13, "_read_attachment_bytes": None,
               "session.get": {"status": 200}, "resp.read": rv("SystemExit", "bye")}, fc=True)
add(F, ARG13, {"_read_attachment_bytes": None}, fc=True, tag="fail-closed: no safe-url script")

# ---- 14 _cache_simple_media ----
F = "_cache_simple_media"
ATT = {"url": "http://cdn/x"}
add(F, {"att": ATT, "content_type": "image/png", "kind": "image",
        "exts": [".jpg", ".png"], "default_ext": ".jpg"},
    {"cache_media": "/c", "print": None})
add(F, {"att": ATT, "content_type": "image/x", "kind": "image",
        "exts": [".jpg"], "default_ext": ".jpg"},
    {"cache_media": "/c", "print": None})
add(F, {"att": ATT, "content_type": "audio/ogg; codecs=opus", "kind": "audio",
        "exts": [".ogg"], "default_ext": ".ogg"},
    {"cache_media": "/a", "print": None})
add(F, {"att": ATT, "content_type": "image/png", "kind": "image",
        "exts": [".png"], "default_ext": ".jpg"},
    {"cache_media": rv("ValueError", "bad"), "print": None})
add(F, {"att": ATT, "content_type": "image/png", "kind": "image",
        "exts": [".png"], "default_ext": ".jpg"},
    {"cache_media": rv("SystemExit", "bye")}, fc=True)
add(F, {"att": ATT, "content_type": "", "kind": "image",
        "exts": [".png"], "default_ext": ".jpg"},
    {"cache_media": "/c", "print": None})
add(F, {"att": ATT, "content_type": "image/png", "kind": "image",
        "exts": [".png"], "default_ext": ".jpg"}, {}, fc=True,
    tag="fail-closed: no cache script")

# ---- 15 _collect_attachment_media ----
F = "_collect_attachment_media"
IMG = {"content_type": "image/png", "url": "u"}
AUD = {"content_type": "audio/ogg", "url": "u2"}
DOC = {"content_type": "text/plain", "filename": "notes.txt", "size": 100}
BIG = {"content_type": "text/plain", "filename": "big.txt", "size": 10000000}
add(F, {"all_attachments": []}, {})
add(F, {"all_attachments": [IMG]}, {"cache_simple": "/img"})
add(F, {"all_attachments": [AUD]}, {"cache_simple": "/aud"})
add(F, {"all_attachments": [DOC]},
    {"max_doc_bytes": 1000, "cache_document": "hello",
     "cache_doc_bytes": "/doc", "logger.info": None,
     "re.sub": "notes.txt"})
add(F, {"all_attachments": [BIG]},
    {"max_doc_bytes": 1000, "logger.warning": None})
add(F, {"all_attachments": [DOC]},
    {"max_doc_bytes": 1000, "cache_document": rv("ValueError", "bad"),
     "logger.warning": None}, fc=True)
add(F, {"all_attachments": [DOC]},
    {"max_doc_bytes": 1000, "cache_document": rv("SystemExit", "bye")}, fc=True)
add(F, {"all_attachments": [IMG, DOC]},
    {"cache_simple": "/img", "max_doc_bytes": 1000, "cache_document": "hello",
     "cache_doc_bytes": "/doc", "logger.info": None, "re.sub": "notes.txt"})
add(F, {"all_attachments": [DOC]}, {}, fc=True, tag="fail-closed: no max-doc script")

# ---- 16 _apply_yaml_config ----
F = "_apply_yaml_config"
add(F, {"yaml_cfg": {}, "discord_cfg": {}}, {"env.set": None})
add(F, {"yaml_cfg": {}, "discord_cfg": {"require_mention": True,
        "thread_require_mention": False}}, {"env.set": None})
add(F, {"yaml_cfg": {"platforms": {"discord": {"extra": {
        "allow_from": ["u1", "u2"], "allow_all_users": True}}}},
        "discord_cfg": {"allowed_roles": "r"}}, {"env.set": None})
add(F, {"yaml_cfg": {}, "discord_cfg": {"approval_mentions": True,
        "auto_thread": False, "reactions": True}}, {"env.set": None})
add(F, {"yaml_cfg": {}, "discord_cfg": {"reply_to_mode": False}}, {"env.set": None})
add(F, {"yaml_cfg": {}, "discord_cfg": {"history_backfill": True,
        "history_backfill_limit": 50}}, {"env.set": None})
add(F, {"yaml_cfg": {}, "discord_cfg": {"allow_mentions": {
        "everyone": True, "roles": False},
        "websocket_liveness_interval_seconds": 10,
        "liveness_failure_threshold": 3}}, {"env.set": None})
add(F, {"yaml_cfg": {}, "discord_cfg": {"require_mention": True}},
    {"env.set": rv("SystemExit", "bye")}, fc=True)
add(F, {"yaml_cfg": {}, "discord_cfg": {"require_mention": True}},
    {}, fc=True, tag="fail-closed: no env script")

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

counts = {}
for c in cases:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"WROTE {OUT}: {len(cases)} cases, {len(counts)} funcs")
for k in sorted(counts):
    print(f"  {k}: {counts[k]}")
