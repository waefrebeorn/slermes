#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow12.jsonl.

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
    'BYTES_PER_MS',
    'ForumChannel',
    'MessageReference',
    'abort_transport',
    'add_done_callback',
    'archived_call',
    'archived_threads',
    'author',
    'author_id',
    'await_close',
    'backfill_config',
    'backfill_limit',
    'backfill_window_seconds',
    'bot_id',
    'bot_user',
    'cancel',
    'channel_id',
    'channel_type',
    'client',
    'close',
    'closing_task',
    'content',
    'create_task',
    'created_at',
    'current_task',
    'decode_json_list_literal',
    'deliver_alert',
    'disconnecting',
    'discord_object',
    'exists',
    'fetch_channel',
    'free_response_channels',
    'gate_csv_set',
    'gate_env',
    'gateway_runner',
    'get_allowed_channels',
    'get_channel',
    'guild',
    'guild_id',
    'guilds',
    'health',
    'history',
    'history_call',
    'history_patterns',
    'interval',
    'is_down_notice',
    'isinstance_forum',
    'iter_messages',
    'logger.debug',
    'logger.error',
    'logger.info',
    'logger.warning',
    'message_channel',
    'message_id',
    'notif_task',
    'notify_fatal_error',
    'now',
    'obfuscated',
    'owner_profile',
    'pattern_match',
    'profile_dir',
    'profile_scope',
    'read_raw_config',
    'read_text',
    'recovery_cursor',
    'ref_message_id',
    'reference',
    'running',
    'set_closing_task',
    'set_disconnecting',
    'set_fatal_error',
    'set_notif_task',
    'sleep',
    'snowflake_time',
    'specs',
    'state_path',
    't_discord',
    'terminal_reasons',
    'text_channels',
    'text_limit',
    'thread',
    'thread_id',
    'threads',
    'threshold',
    'type_value',
    'voice_fx_cfg',
    'voice_mixer_module',
    'wait',
    'ws',
]

DEFAULT_IO = {s: None for s in SEAMS}
DEFAULT_IO.update({'BYTES_PER_MS': 48,
 'ForumChannel': 'ForumChannelCls',
 'MessageReference': 'ref-1',
 'abort_transport': False,
 'add_done_callback': None,
 'archived_call': {'__script__': True, 'ret': []},
 'archived_threads': None,
 'author': 'author-1',
 'author_id': 42,
 'await_close': None,
 'backfill_config': {},
 'backfill_limit': 3,
 'backfill_window_seconds': 60,
 'bot_id': 42,
 'bot_user': 'bot-user',
 'cancel': None,
 'channel_id': 'chan-1',
 'channel_type': 15,
 'client': 'client-1',
 'close': 'close-task',
 'closing_task': None,
 'content': 'hello',
 'create_task': 'task-1',
 'created_at': 1000,
 'current_task': 'task-1',
 'decode_json_list_literal': {'__script__': True, 'ret': []},
 'deliver_alert': None,
 'disconnecting': False,
 'discord_object': 'obj-1',
 'exists': False,
 'fetch_channel': 'chan-fetch',
 'free_response_channels': {'__script__': True, 'ret': ['chan-b']},
 'gate_csv_set': {'__script__': True, 'ret': ['chan-a', 'chan-b']},
 'gate_env': '',
 'gateway_runner': None,
 'get_allowed_channels': {'__script__': True, 'ret': ['chan-a']},
 'get_channel': 'chan-1',
 'guild': 'guild-1',
 'guild_id': 100,
 'guilds': {'__script__': True, 'ret': []},
 'health': {'__script__': True, 'ret': [True, 'ok']},
 'history': 'callable',
 'history_call': {'__script__': True, 'ret': ['m1', 'm2']},
 'history_patterns': {'__script__': True, 'ret': []},
 'interval': 1,
 'is_down_notice': False,
 'isinstance_forum': False,
 'iter_messages': {'__script__': True, 'ret': ['m1', 'm2']},
 'logger.debug': None,
 'logger.error': None,
 'logger.info': None,
 'logger.warning': None,
 'message_channel': 'chan-1',
 'message_id': 'm1',
 'notif_task': 'task-1',
 'notify_fatal_error': None,
 'now': 1000,
 'obfuscated': False,
 'owner_profile': None,
 'pattern_match': False,
 'profile_dir': 'profile-dir-1',
 'profile_scope': 'scope-1',
 'read_raw_config': {},
 'read_text': '["1","2"," 3 "]',
 'recovery_cursor': None,
 'ref_message_id': 'm1',
 'reference': 'ref-1',
 'running': False,
 'set_closing_task': None,
 'set_disconnecting': None,
 'set_fatal_error': None,
 'set_notif_task': None,
 'sleep': None,
 'snowflake_time': 500,
 'specs': {'__script__': True,
           'ret': [['ping',
                    'desc.ping',
                    [['arg1', 3, 'd', 'desc.arg', [['choice.a', 1], ['plain', 2]]]],
                    'tmpl.ping',
                    'follow.ping'],
                   ['empty', 'desc.empty', [], 'tmpl.empty', None]]},
 'state_path': 'state.json',
 't_discord': 'translated',
 'terminal_reasons': {'__script__': True, 'ret': ['transport_closed']},
 'text_channels': {'__script__': True, 'ret': []},
 'text_limit': 100,
 'thread': None,
 'thread_id': 'thread-1',
 'threads': {'__script__': True, 'ret': []},
 'threshold': 2,
 'type_value': 15,
 'voice_fx_cfg': {'lead_silence_ms': 200},
 'voice_mixer_module': 'voice_mixer',
 'wait': {'done': True},
 'ws': 'ws-1'})

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

# ── _is_forum_parent (8) ──
CASES += [
    case("_is_forum_parent", {"channel": None}),
    case("_is_forum_parent", {"channel": "ch-1"}),
    case("_is_forum_parent", {"channel": "ch-1"}, {"isinstance_forum": True}),
    case("_is_forum_parent", {"channel": "ch-1"}, {"isinstance_forum": False, "channel_type": None}),
    case("_is_forum_parent", {"channel": "ch-1"}, {"channel_type": 15, "type_value": 9}),
    case("_is_forum_parent", {"channel": "ch-1"}, {"ForumChannel": None}),
    case("_is_forum_parent", {"channel": "ch-1"}, {"ForumChannel": sr("RuntimeError", "no cls")}),
    case("_is_forum_parent", {"channel": "ch-1"}, {"isinstance_forum": sr("SystemExit", "bye")}, note="base passthrough"),
]

# ── _is_length_overflow_error (6) ──
CASES += [
    case("_is_length_overflow_error", {"err": "HTTPException: error code: 50035: Invalid Form Body 2000 or fewer characters"}),
    case("_is_length_overflow_error", {"err": "error code: 50035: bad reply reference"}),
    case("_is_length_overflow_error", {"err": "ERROR CODE: 50035 SOMETHING FEWER IN LENGTH"}),
    case("_is_length_overflow_error", {"err": "error code: 50035"}),
    case("_is_length_overflow_error", {"err": None}),
    case("_is_length_overflow_error", {"err": 50035}),
]

# ── _iter_channel_and_thread_messages (9) ──
CASES += [
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": 100}),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": 100},
         {"threads": {"__script__": True, "ret": ["thread-1"]}, "channel_id": ["chan-1", "thread-1"],
          "history_call": [["m1", "m2"], ["m3"]]}),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": 100},
         {"recovery_cursor": "123", "snowflake_time": 500}),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": {"datetime": 1000}},
         {"recovery_cursor": "123", "snowflake_time": 500}),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": {"datetime": 1000}},
         {"recovery_cursor": "123", "snowflake_time": 2000}),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": 100},
         {"history_call": sr("RuntimeError", "db")}),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": 100},
         {"history_call": sr("SystemExit", "bye")}, note="base passthrough"),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": 100},
         {"channel_id": sr("RuntimeError", "no id")}),
    case("_iter_channel_and_thread_messages", {"channel": "chan-1", "limit": 10, "after": 100,
                                             "seen_channels": ["chan-1"]}),
]

# ── _iter_missed_message_backfill_candidates (8) ──
CASES += [
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["chan-x"]}),
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["*"]},
         {"guilds": ["g1"], "text_channels": ["c1", "c2"], "iter_messages": [["m1"], ["m2"]]}),
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["a", "b"]},
         {"get_channel": ["ch-a", "ch-b"], "obfuscated": [False, True], "iter_messages": [["m1"]]}),
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["a", "b"]},
         {"get_channel": [None, "ch-b"], "obfuscated": [False, False], "iter_messages": [["m1"], ["m2"]]}),
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["a", "b"]},
         {"get_channel": [None, "ch-b"], "fetch_channel": sr("RuntimeError", "nope"),
          "obfuscated": [False], "iter_messages": [["m2"]]}),
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["chan-x"]}, {"client": None}),
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["chan-x"]}, {"backfill_limit": 1}),
    case("_iter_missed_message_backfill_candidates", {"channel_ids": ["chan-x"]},
         {"iter_messages": sr("SystemExit", "bye")}, note="base passthrough"),
]

# ── _lead_silence_bytes (8) ──
CASES += [
    case("_lead_silence_bytes", {}),
    case("_lead_silence_bytes", {}, {"voice_fx_cfg": {}}),
    case("_lead_silence_bytes", {}, {"voice_fx_cfg": {"lead_silence_ms": 0}}),
    case("_lead_silence_bytes", {}, {"voice_fx_cfg": {"lead_silence_ms": -5}}),
    case("_lead_silence_bytes", {}, {"voice_fx_cfg": {"lead_silence_ms": "abc"}}),
    case("_lead_silence_bytes", {}, {"voice_fx_cfg": {"lead_silence_ms": "10"}}),
    case("_lead_silence_bytes", {}, {"voice_mixer_module": sr("RuntimeError", "no mixer")}),
    case("_lead_silence_bytes", {}, {"BYTES_PER_MS": sr("SystemExit", "bye")}, note="base passthrough"),
]

# ── _liveness_loop (8) ──
CASES += [
    case("_liveness_loop", {}),
    case("_liveness_loop", {}, {"running": [True, True, False], "health": {"__script__": True, "ret": [True, "ok"]}}),
    case("_liveness_loop", {}, {"running": [True] * 6, "health": {"__script__": True, "ret": [False, "stale"]}, "threshold": 2}),
    case("_liveness_loop", {}, {"running": [True] * 4, "health": {"__script__": True, "ret": [False, "transport_closed"]}, "threshold": 3}),
    case("_liveness_loop", {}, {"running": [True] * 6, "health": sr("RuntimeError", "check"), "threshold": 2}),
    case("_liveness_loop", {}, {"running": [True, True, False], "client": None}),
    case("_liveness_loop", {}, {"sleep": sr("CancelledError", "bye")}, note="base passthrough"),
    case("_liveness_loop", {}, {"running": sr("SystemExit", "bye")}, note="base passthrough"),
]

# ── _load (8) ──
CASES += [
    case("_load", {}),
    case("_load", {}, {"exists": True}),
    case("_load", {}, {"exists": True, "read_text": '["1"," 2 ","","x"]'}),
    case("_load", {}, {"exists": True, "read_text": '{"a":1}'}),
    case("_load", {}, {"exists": True, "read_text": "not-json"}),
    case("_load", {}, {"exists": True, "read_text": sr("RuntimeError", "io")}),
    case("_load", {}, {"state_path": sr("SystemExit", "bye")}, note="base passthrough"),
    case("_load", {}, {"exists": True, "read_text": None}),
]

# ── _load_voice_fx_config (7) ──
CASES += [
    case("_load_voice_fx_config", {}),
    case("_load_voice_fx_config", {}, {"read_raw_config": {"discord": {"voice_fx": {"enabled": True, "ambient_gain": 0.5, "unknown": 1}}}}),
    case("_load_voice_fx_config", {}, {"read_raw_config": {"discord": {"voice_fx": {"lead_silence_ms": None}}}}),
    case("_load_voice_fx_config", {}, {"read_raw_config": {"discord": None}}),
    case("_load_voice_fx_config", {}, {"read_raw_config": sr("RuntimeError", "cfg")}),
    case("_load_voice_fx_config", {}, {"read_raw_config": sr("SystemExit", "bye")}, note="base passthrough"),
    case("_load_voice_fx_config", {}, {"read_raw_config": sr("RuntimeError", "cfg"), "logger.debug": sr("RuntimeError", "log")}),
]

# ── _looks_like_nonconversational_history_message (6) ──
CASES += [
    case("_looks_like_nonconversational_history_message", {"content": "hello"}),
    case("_looks_like_nonconversational_history_message", {"content": "hello"},
         {"history_patterns": {"__script__": True, "ret": ["p1", "p2"]}, "pattern_match": [False, True]}),
    case("_looks_like_nonconversational_history_message", {"content": "hello"},
         {"history_patterns": {"__script__": True, "ret": ["p1"]}, "pattern_match": False}),
    case("_looks_like_nonconversational_history_message", {"content": "hello"},
         {"history_patterns": {"__script__": True, "ret": ["p1"]}, "pattern_match": sr("RuntimeError", "regex")}),
    case("_looks_like_nonconversational_history_message", {"content": 42}),
    case("_looks_like_nonconversational_history_message", {"content": None}),
]

# ── _message_has_non_down_bot_response (8) ──
CASES += [
    case("_message_has_non_down_bot_response", {"message": "msg-1"}),
    case("_message_has_non_down_bot_response", {"message": "msg-1"}, {"author_id": 7}),
    case("_message_has_non_down_bot_response", {"message": "msg-1"}, {"is_down_notice": True}),
    case("_message_has_non_down_bot_response", {"message": "msg-1"},
         {"history_call": [[], ["m3"]], "thread": "thread-2"}),
    case("_message_has_non_down_bot_response", {"message": "msg-1"}, {"bot_id": None}),
    case("_message_has_non_down_bot_response", {"message": "msg-1"}, {"client": None}),
    case("_message_has_non_down_bot_response", {"message": "msg-1"},
         {"author_id": sr("SystemExit", "bye")}, note="base passthrough"),
    case("_message_has_non_down_bot_response", {"message": "msg-1"},
         {"history": sr("RuntimeError", "nope")}),
]

# ── _message_reference_from_ids (8) ──
CASES += [
    case("_message_reference_from_ids", {"message_id": 123, "channel": "ch"}),
    case("_message_reference_from_ids", {"message_id": "123", "channel": "ch"}),
    case("_message_reference_from_ids", {"message_id": 123, "channel": "ch"}, {"channel_id": None}),
    case("_message_reference_from_ids", {"message_id": 123, "channel": "ch"}, {"guild": None}),
    case("_message_reference_from_ids", {"message_id": 123, "channel": "ch"},
         {"channel_id": sr("RuntimeError", "x")}),
    case("_message_reference_from_ids", {"message_id": 123, "channel": "ch"},
         {"guild": sr("RuntimeError", "x")}),
    case("_message_reference_from_ids", {"message_id": 123, "channel": "ch"},
         {"guild_id": sr("RuntimeError", "x")}),
    case("_message_reference_from_ids", {"message_id": 123, "channel": "ch"},
         {"MessageReference": sr("SystemExit", "bye")}, note="base passthrough"),
]

# ── _missed_message_backfill_channels (7) ──
CASES += [
    case("_missed_message_backfill_channels", {}),
    case("_missed_message_backfill_channels", {}, {"backfill_config": {"channels": "a,b"}}),
    case("_missed_message_backfill_channels", {}, {"backfill_config": {"channels": "a,b"},
                                                   "gate_csv_set": [[]], "decode_json_list_literal": {"__script__": True, "ret": ["a"]}}),
    case("_missed_message_backfill_channels", {}, {"gate_env": "x,y"}),
    case("_missed_message_backfill_channels", {}, {"backfill_config": {"channels": "a"},
                                                   "gate_csv_set": sr("RuntimeError", "csv")}),
    case("_missed_message_backfill_channels", {}, {"backfill_config": sr("RuntimeError", "cfg")}),
    case("_missed_message_backfill_channels", {}, {"gate_env": "", "get_allowed_channels": ["z"],
                                                   "free_response_channels": ["a"]}),
]

# ── _native_slash_commands (6) ──
CASES += [
    case("_native_slash_commands", {}),
    case("_native_slash_commands", {}, {"specs": {"__script__": True, "ret": []}}),
    case("_native_slash_commands", {}, {"text_limit": 50}),
    case("_native_slash_commands", {}, {"t_discord": sr("RuntimeError", "i18n")}),
    case("_native_slash_commands", {}, {"specs": {"__script__": True, "ret": [["one", "d1", [["a", 1, 0, "ad", None]], "t", "f"]]}}),
    case("_native_slash_commands", {}, {"specs": {"__script__": True, "ret": [["one", "d1", [["a", 1, 0, "ad", [["x.y", 2]]]], "t", "f"]]}}),
]

# ── _normalize_permissions (6) ──
CASES += [
    case("_normalize_permissions", {"value": None}),
    case("_normalize_permissions", {"value": 12345}),
    case("_normalize_permissions", {"value": "8"}),
    case("_normalize_permissions", {"value": True}),
    case("_normalize_permissions", {"value": 0}),
    case("_normalize_permissions", {"value": False}),
]

# ── _notify_liveness_fatal_error (8) ──
CASES += [
    case("_notify_liveness_fatal_error", {"client": "client-1"}),
    case("_notify_liveness_fatal_error", {"client": "client-1"}, {"wait": {"done": False}}),
    case("_notify_liveness_fatal_error", {"client": "client-1"},
         {"wait": {"done": False}, "closing_task": {"task": True}, "abort_transport": True}),
    case("_notify_liveness_fatal_error", {"client": "client-1"}, {"close": sr("RuntimeError", "close")}),
    case("_notify_liveness_fatal_error", {"client": "client-1"}, {"wait": sr("RuntimeError", "wait")}),
    case("_notify_liveness_fatal_error", {"client": "client-1"},
         {"abort_transport": sr("RuntimeError", "abort"), "wait": {"done": False}}),
    case("_notify_liveness_fatal_error", {"client": "client-1"},
         {"notify_fatal_error": sr("SystemExit", "bye")}, note="base passthrough"),
    case("_notify_liveness_fatal_error", {"client": "client-1"},
         {"notif_task": "t1", "current_task": "t2"}),
]

# ── _notify_unauthorized_slash (7) ──
CASES += [
    case("_notify_unauthorized_slash", {"user_name": "u", "user_id": "1", "chan_id": "c",
                                        "guild_id": "g", "command_text": "/x", "reason": "r"}),
    case("_notify_unauthorized_slash", {"user_name": "u", "user_id": "1", "chan_id": "c",
                                        "guild_id": "g", "command_text": "/x", "reason": "r"},
         {"gateway_runner": "runner-1"}),
    case("_notify_unauthorized_slash", {"user_name": "u", "user_id": "1", "chan_id": "c",
                                        "guild_id": "g", "command_text": "/x", "reason": "r"},
         {"gateway_runner": "runner-1", "owner_profile": "prof-1"}),
    case("_notify_unauthorized_slash", {"user_name": "u", "user_id": "1", "chan_id": "c",
                                        "guild_id": "g", "command_text": "/x", "reason": "r"},
         {"gateway_runner": "runner-1", "deliver_alert": sr("RuntimeError", "send")}),
    case("_notify_unauthorized_slash", {"user_name": "u", "user_id": "1", "chan_id": "c",
                                        "guild_id": "g", "command_text": "/x", "reason": "r"},
         {"gateway_runner": "runner-1", "owner_profile": "prof-1", "profile_dir": sr("RuntimeError", "scope")}),
    case("_notify_unauthorized_slash", {"user_name": "u", "user_id": "1", "chan_id": "c",
                                        "guild_id": "g", "command_text": "/x", "reason": "r"},
         {"gateway_runner": "runner-1", "deliver_alert": sr("SystemExit", "bye")}, note="base passthrough"),
    case("_notify_unauthorized_slash", {"user_name": "u", "user_id": "1", "chan_id": "c",
                                        "guild_id": "g", "command_text": "/x", "reason": "r"},
         {"gateway_runner": sr("RuntimeError", "x")}),
]

def main():
    out = REPO / "tests" / "fixtures" / "discord_adapter_flow12.jsonl"
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
