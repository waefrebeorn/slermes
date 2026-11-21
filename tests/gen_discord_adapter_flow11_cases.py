#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow11.jsonl.

Every case carries a full default io map (one benign script per universal-seam
name) plus per-case overrides. A result is either a plain JSON value, an ordered
list (one result per call), or {"__script__":true, "raise":{...}}. Unused keys
are harmless; this guarantees the fixture io-key set equals the C seam set
(SEAM-NAME SET EQUALITY).
"""
import json
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

SEAMS = [
    'MessageEvent',
    'VoiceMixer',
    'allow_all_users',
    'allowed_channels',
    'allowed_ids',
    'allowed_roles',
    'allowed_users',
    'ambient_cache',
    'ambient_enabled',
    'ambient_gain',
    'ambient_path',
    'append_task',
    'author_id',
    'auto_create_thread',
    'auto_thread_flag',
    'backfill_limit',
    'bot_tag_continuation',
    'bot_task',
    'build_source',
    'cached_id',
    'channel_id',
    'channel_ids_allowed',
    'channel_keys',
    'channel_members',
    'channel_send',
    'client',
    'conn_hook',
    'create_forum_thread',
    'create_task',
    'create_thread',
    'decode_to_pcm',
    'dedup_add',
    'defer',
    'derive_forum_name',
    'describe_proxy_error',
    'disconnecting',
    'dispatch',
    'dispatch_thread_session',
    'dm_role_auth_guild',
    'drain',
    'duck_gain',
    'edit',
    'existing_task',
    'followup_send',
    'format_message',
    'forum_thread_parts',
    'free_channels',
    'free_response_auto_thread',
    'gateway_allow_all',
    'gateway_runner',
    'get_allow_bots',
    'get_ambient_pcm',
    'get_guild',
    'get_no_thread_channels',
    'getenv',
    'guild_member_has_role',
    'has_ws',
    'history',
    'ignored_channels',
    'in_bot_thread',
    'is_authorized',
    'is_nonconversational',
    'is_playing',
    'is_reply',
    'isfile',
    'logger.debug',
    'logger.error',
    'logger.info',
    'logger.warning',
    'map_ssrc',
    'mark_async',
    'message_reference',
    'pairing_approved',
    'play',
    'proxy',
    'record_recovery_scan_complete',
    'refresh_live_gates',
    'require_mention',
    'resolve_channel_prompt',
    'running',
    'self_mentioned',
    'set_ambient',
    'set_ambient_cache',
    'set_fatal_error',
    'set_last_self_id',
    'set_mixer',
    'set_state_hook',
    'set_ws_hook',
    'slash_auth',
    'speech_gain',
    'startup_restore_in_progress',
    'startup_restore_tasks',
    'stop',
    'synth_ambient_pcm',
    't',
    'task_done',
    'task_exception',
    'to_reference',
    'truncate_message',
    'vc_bot_id',
    'vc_channel',
]

DEFAULT_IO = {s: None for s in SEAMS}
DEFAULT_IO.update({
    "MessageEvent": "ev-1", "VoiceMixer": "mixer-1",
    "allow_all_users": False, "allowed_channels": [[]], "allowed_ids": [[]],
    "allowed_roles": [[]], "allowed_users": [[]], "ambient_cache": None,
    "ambient_enabled": True, "ambient_gain": 0.5, "ambient_path": "bed.wav",
    "append_task": None, "author_id": "author-1", "auto_create_thread": None,
    "auto_thread_flag": True, "backfill_limit": 10,
    "bot_tag_continuation": False, "bot_task": None, "build_source": "source-1",
    "cached_id": None, "channel_id": "chan-1", "channel_ids_allowed": False,
    "channel_keys": [[]], "channel_members": [[10, 20]], "channel_send": {"id": "sent-1"},
    "client": "client-1", "conn_hook": None, "create_forum_thread": "thread-1",
    "create_task": "task-1",
    "create_thread": {"success": True, "thread_id": "t1", "thread_name": "n", "thread": "thr"},
    "decode_to_pcm": "pcm-file", "dedup_add": None, "defer": True,
    "derive_forum_name": "derived-name", "describe_proxy_error": None,
    "disconnecting": False, "dispatch": True, "dispatch_thread_session": None,
    "dm_role_auth_guild": None, "drain": None, "duck_gain": 0.25, "edit": None,
    "existing_task": None, "followup_send": None, "format_message": "formatted",
    "forum_thread_parts": [["ch", "thread-1", "starter", "msg-1"]],
    "free_channels": [[]], "free_response_auto_thread": False,
    "gateway_allow_all": False, "gateway_runner": None, "get_allow_bots": "all",
    "get_ambient_pcm": "pcm-1", "get_guild": "guild-1", "get_no_thread_channels": [[]],
    "getenv": "30", "guild_member_has_role": False, "has_ws": False, "history": [[]],
    "ignored_channels": [[]], "in_bot_thread": False, "is_authorized": True,
    "is_nonconversational": False, "is_playing": False, "is_reply": False,
    "isfile": True, "logger.debug": None, "logger.error": None,
    "logger.info": None, "logger.warning": None, "map_ssrc": None,
    "mark_async": None, "message_reference": "ref", "pairing_approved": False,
    "play": None, "proxy": "proxy-1", "record_recovery_scan_complete": None,
    "refresh_live_gates": None, "require_mention": True,
    "resolve_channel_prompt": "prompt-1", "running": True, "self_mentioned": False,
    "set_ambient": None, "set_ambient_cache": None, "set_fatal_error": None,
    "set_last_self_id": None, "set_mixer": None, "set_state_hook": None,
    "set_ws_hook": None, "slash_auth": True, "speech_gain": 1,
    "startup_restore_in_progress": False, "startup_restore_tasks": None,
    "stop": None, "synth_ambient_pcm": "pcm-synth", "t": "translated",
    "task_done": False, "task_exception": None, "to_reference": "ref-1",
    "truncate_message": [["chunk1", "chunk2"]], "vc_bot_id": 99, "vc_channel": "chan",
})


def sc(ret):
    return {"__script__": True, "ret": ret}


def sr(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def case(func, args, overrides=None, note=None):
    io = dict(DEFAULT_IO)
    for k, v in (overrides or {}).items():
        io[k] = v
    rec = {"func": func, "in": {"args": args, "io": io}}
    if note:
        rec["_note"] = note
    return rec


CASES = []

# ── _finish_recovery_scan (6) ──
CASES += [
    case("_finish_recovery_scan", {"scan_id": "s1", "status": "ok", "counts": {"a": 1}}),
    case("_finish_recovery_scan", {"scan_id": "s2", "status": "fail", "error": "boom", "counts": {"x": 2}}),
    case("_finish_recovery_scan", {"scan_id": "", "status": "ok"}),
    case("_finish_recovery_scan", {"scan_id": "s3", "status": "ok"},
         {"record_recovery_scan_complete": sr("RuntimeError", "db down")}),
    case("_finish_recovery_scan", {"scan_id": "s4", "status": "ok"},
         {"record_recovery_scan_complete": sr("SystemExit", "bye")}, note="base passthrough"),
    case("_finish_recovery_scan", {"scan_id": "s5", "status": "ok", "error": None, "counts": {}}),
]

# ── _command_sync_state_key (6) ──
CASES += [
    case("_command_sync_state_key", {"app_id": "12345"}),
    case("_command_sync_state_key", {"app_id": None}),
    case("_command_sync_state_key", {"app_id": ""}),
    case("_command_sync_state_key", {"app_id": 0}),
    case("_command_sync_state_key", {"app_id": 42}),
    case("_command_sync_state_key", {"app_id": True}),
]

# ── _bot_tag_debounce_key (6) ──
CASES += [
    case("_bot_tag_debounce_key", {}),
    case("_bot_tag_debounce_key", {}, {"channel_id": 100, "author_id": 200}),
    case("_bot_tag_debounce_key", {}, {"channel_id": "", "author_id": "z"}),
    case("_bot_tag_debounce_key", {}, {"channel_id": sr("RuntimeError", "no ch")}),
    case("_bot_tag_debounce_key", {}, {"author_id": sr("SystemExit", "x")}, note="base passthrough"),
    case("_bot_tag_debounce_key", {}, {"channel_id": None, "author_id": None}),
]

# ── _discord_ready_timeout_seconds (6) ──
CASES += [
    case("_discord_ready_timeout_seconds", {}, {"getenv": "45"}),
    case("_discord_ready_timeout_seconds", {}, {"getenv": ""}),
    case("_discord_ready_timeout_seconds", {}, {"getenv": None}),
    case("_discord_ready_timeout_seconds", {}, {"getenv": "-5"}, note="clamped to 0"),
    case("_discord_ready_timeout_seconds", {}, {"getenv": "4.5"}),
    case("_discord_ready_timeout_seconds", {}, {"getenv": "not-a-number"}),
    case("_discord_ready_timeout_seconds", {}, {"getenv": sr("RuntimeError", "env")}),
]

# ── _ensure_missed_message_backfill_task (7) ──
CASES += [
    case("_ensure_missed_message_backfill_task", {}),
    case("_ensure_missed_message_backfill_task", {}, {"existing_task": "old-task", "task_done": False}),
    case("_ensure_missed_message_backfill_task", {}, {"existing_task": "old-task", "task_done": True,
         "gateway_runner": "runner-1"}),
    case("_ensure_missed_message_backfill_task", {}, {"existing_task": "old-task", "task_done": True,
         "gateway_runner": "runner-1", "startup_restore_in_progress": True,
         "startup_restore_tasks": ["t0"]}),
    case("_ensure_missed_message_backfill_task", {}, {"existing_task": "old-task", "task_done": True,
         "gateway_runner": "runner-1", "startup_restore_in_progress": True,
         "startup_restore_tasks": None}),
    case("_ensure_missed_message_backfill_task", {}, {"existing_task": sr("RuntimeError", "db")}),
    case("_ensure_missed_message_backfill_task", {}, {"create_task": sr("SystemExit", "x")}, note="base passthrough"),
]

# ── _infer_user_for_ssrc (8) ──
CASES += [
    case("_infer_user_for_ssrc", {"ssrc": 1}),
    case("_infer_user_for_ssrc", {"ssrc": 2}, {"channel_members": [[10, 20, 99]]}),
    case("_infer_user_for_ssrc", {"ssrc": 3}, {"channel_members": [[10]]}),
    case("_infer_user_for_ssrc", {"ssrc": 4}, {"channel_members": [[10]], "allowed_ids": [["10"]]}),
    case("_infer_user_for_ssrc", {"ssrc": 5}, {"channel_members": [[10]], "allowed_ids": [["999"]]}),
    case("_infer_user_for_ssrc", {"ssrc": 6}, {"vc_channel": None}),
    case("_infer_user_for_ssrc", {"ssrc": 7}, {"vc_channel": sr("RuntimeError", "no vc")}),
    case("_infer_user_for_ssrc", {"ssrc": 8}, {"map_ssrc": sr("SystemExit", "x")}, note="base passthrough"),
]

# ── _get_ambient_pcm (8) ──
CASES += [
    case("_get_ambient_pcm", {}),
    case("_get_ambient_pcm", {}, {"ambient_cache": "cached-pcm"}),
    case("_get_ambient_pcm", {}, {"ambient_enabled": False}),
    case("_get_ambient_pcm", {}, {"ambient_path": ""}),
    case("_get_ambient_pcm", {}, {"ambient_path": "  "}),
    case("_get_ambient_pcm", {}, {"decode_to_pcm": None}),
    case("_get_ambient_pcm", {}, {"decode_to_pcm": sr("RuntimeError", "decode")}),
    case("_get_ambient_pcm", {}, {"synth_ambient_pcm": sr("SystemExit", "x")}, note="base passthrough"),
]

# ── _install_voice_mixer (7) ──
CASES += [
    case("_install_voice_mixer", {"guild_id": 1}),
    case("_install_voice_mixer", {"guild_id": 2}, {"get_ambient_pcm": None}),
    case("_install_voice_mixer", {"guild_id": 3}, {"is_playing": True}),
    case("_install_voice_mixer", {"guild_id": 4}, {"ambient_gain": sr("RuntimeError", "cfg")}),
    case("_install_voice_mixer", {"guild_id": 5}, {"VoiceMixer": sr("RuntimeError", "ctor")}),
    case("_install_voice_mixer", {"guild_id": 6}, {"play": sr("SystemExit", "x")}, note="base passthrough"),
    case("_install_voice_mixer", {"guild_id": 7}, {"set_mixer": sr("RuntimeError", "store")}),
]

# ── _handle_bot_task_done (8) ──
CASES += [
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"}),
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"}, {"disconnecting": True}),
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"}, {"bot_task": "other"}),
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"}, {"running": False}),
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"},
         {"task_exception": sr("asyncio.CancelledError", "")}),
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"},
         {"task_exception": sr("ValueError", "boom"), "describe_proxy_error": "masked"}),
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"},
         {"task_exception": sr("ValueError", "boom"), "describe_proxy_error": None}),
    case("_handle_bot_task_done", {"task_id": "t1", "name": "bot"},
         {"task_exception": sr("SystemExit", "x")}, note="base passthrough"),
]

# ── _install_speaking_hook (7) ──
CASES += [
    case("_install_speaking_hook", {}),
    case("_install_speaking_hook", {}, {"conn_hook": "original"}),
    case("_install_speaking_hook", {}, {"has_ws": True}),
    case("_install_speaking_hook", {}, {"has_ws": sr("RuntimeError", "ws")}),
    case("_install_speaking_hook", {}, {"set_ws_hook": sr("RuntimeError", "ws2")}),
    case("_install_speaking_hook", {}, {"has_ws": sr("SystemExit", "x")}, note="base passthrough"),
    case("_install_speaking_hook", {}, {"logger.info": sr("RuntimeError", "log")}),
]

# ── _forum_post_file (8) ──
CASES += [
    case("_forum_post_file", {"name": "bot", "thread_name": "my-thread", "content": "hello"}),
    case("_forum_post_file", {"name": "bot", "content": "  hello  "}),
    case("_forum_post_file", {"name": "bot", "file": "f1", "filename": "pic.png"}),
    case("_forum_post_file", {"name": "bot", "files": ["f1"], "filename": "pic.png",
         "attachments": ["a1"]}),
    case("_forum_post_file", {"name": "bot", "file": "f1", "filename": "pic.png", "attachments": []}),
    case("_forum_post_file", {"name": "bot", "content": "hi"},
         {"create_forum_thread": sr("RuntimeError", "no perm")}),
    case("_forum_post_file", {"name": "bot", "content": "hi"},
         {"create_forum_thread": sr("SystemExit", "x")}, note="base passthrough"),
    case("_forum_post_file", {"name": "bot", "content": "hi"},
         {"t": sr("RuntimeError", "i18n")}),
]

# ── _edit_overflow_split (9) ──
CASES += [
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1}),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1},
         {"truncate_message": [["only"]]}),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1},
         {"truncate_message": [["c1", "c2", "c3"]], "channel_send": [{"id": "s2"}, {"id": "s3"}]}),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1, "prev_has_ref": False},
         {"truncate_message": [["c1", "c2", "c3"]],
          "channel_send": [{"id": "s2"}, {"id": "s3"}]}),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1},
         {"edit": sr("RuntimeError", "edit fail")}),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1},
         {"edit": sr("SystemExit", "x")}, note="base passthrough"),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1},
         {"truncate_message": [["c1", "c2", "c3"]],
          "channel_send": [sr("RuntimeError", "send fail"), {"id": "s3"}]}),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1},
         {"truncate_message": [["c1", "c2", "c3"]],
          "channel_send": [sr("RuntimeError", "send fail"), sr("RuntimeError", "retry fail")]}),
    case("_edit_overflow_split", {"content": "hi", "message_id": "m1", "name": "bot", "channel_id_num": 1},
         {"truncate_message": ["c1", "c2"], "is_nonconversational": True, "channel_send": {"id": "s2"}}),
]

# ── _fetch_channel_context (8) ──
_H1 = {"id": "10", "content": "one", "name": "alice", "author_id": "a1"}
_H2 = {"id": "11", "content": "two", "name": "bob", "author_id": "a2", "is_bot": True}
_HSELF = {"id": "12", "content": "self", "name": "me", "author_is_self": True}
CASES += [
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"}),
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"}, {"backfill_limit": 0}),
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"}, {"backfill_limit": -1}),
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"}, {"history": [[_H1, _H2]]}),
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"}, {"history": [[_H1, _HSELF, _H2]]}),
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"},
         {"history": [[_H1]], "get_allow_bots": "none"}),
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"},
         {"history": [[_H1]], "is_authorized": False}),
    case("_fetch_channel_context", {"channel_id": "ch1", "before": "20"},
         {"history": sr("RuntimeError", "api")}, note="fail-closed"),
]

# ── _is_allowed_user (9) ──
CASES += [
    case("_is_allowed_user", {"user_id": "u1"}),
    case("_is_allowed_user", {"user_id": "u1"}, {"allowed_users": [["u1"]]}),
    case("_is_allowed_user", {"user_id": "u1"}, {"allowed_users": [["*"]]}),
    case("_is_allowed_user", {"user_id": "u1"}, {"pairing_approved": True}),
    case("_is_allowed_user", {"user_id": "u1"}, {"allow_all_users": True}),
    case("_is_allowed_user", {"user_id": "u1"}, {"gateway_allow_all": True}),
    case("_is_allowed_user", {"user_id": "u1", "channel_ids": ["ch1"], "is_dm": False, "guild": {"id": "g"}},
         {"channel_ids_allowed": True}),
    case("_is_allowed_user", {"user_id": "u1",
         "author_roles": ["r1"], "author_guild": "g", "guild": "g", "is_dm": False},
         {"allowed_roles": [["r1"]]}),
    case("_is_allowed_user", {"user_id": "u1"},
         {"refresh_live_gates": sr("SystemExit", "x")}, note="base passthrough"),
]

# ── _handle_thread_create_slash (8) ──
CASES += [
    case("_handle_thread_create_slash", {"name": "n", "message": "  hi  "}),
    case("_handle_thread_create_slash", {"name": "n", "message": "hi"}, {"slash_auth": False}),
    case("_handle_thread_create_slash", {"name": "n", "message": "hi"}, {"defer": False}),
    case("_handle_thread_create_slash", {"name": "n", "message": "hi"},
         {"create_thread": {"success": False, "error": "bad"}}),
    case("_handle_thread_create_slash", {"name": "n", "message": "hi"},
         {"create_thread": {"success": False, "error": ""}}),
    case("_handle_thread_create_slash", {"name": "n", "message": "hi"},
         {"create_thread": {"success": True, "thread_id": "", "thread_name": ""}}),
    case("_handle_thread_create_slash", {"name": "n", "message": ""}),
    case("_handle_thread_create_slash", {"name": "n", "message": "hi"},
         {"create_thread": sr("SystemExit", "x")}, note="base passthrough"),
]

# ── _handle_message (9) ──
CASES += [
    case("_handle_message", {"message_id": "m1", "content": "  hello  "}),
    case("_handle_message", {"message_id": "m1", "content": "hi", "is_dm": True}),
    case("_handle_message", {"message_id": "m1", "content": "hi", "is_thread": True}),
    case("_handle_message", {"message_id": "m1", "content": "hi"},
         {"allowed_channels": [["other"]], "channel_keys": [["ch1"]]}),
    case("_handle_message", {"message_id": "m1", "content": "hi"},
         {"ignored_channels": [["ch1"]], "channel_keys": [["ch1"]]}),
    case("_handle_message", {"message_id": "m1", "content": "hi"},
         {"require_mention": True, "self_mentioned": False, "bot_tag_continuation": False,
          "free_channels": [[]]}),
    case("_handle_message", {"message_id": "m1", "content": "hi"},
         {"require_mention": True, "self_mentioned": True, "free_channels": [[]],
          "auto_thread_flag": False}),
    case("_handle_message", {"message_id": "m1", "content": "hi"},
         {"auto_thread_flag": True, "auto_create_thread": "new-t"}),
    case("_handle_message", {"message_id": "m1", "content": "hi"},
         {"channel_keys": sr("SystemExit", "x")}, note="base passthrough"),
]


def main():
    out = REPO / "tests" / "fixtures" / "discord_adapter_flow11.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {out}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
