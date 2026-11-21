#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow10.jsonl.

Every case carries a full default io map (one benign script per universal-seam
name) plus per-case overrides. A result is either a plain JSON value, an ordered
list (one result per call), or {"__script__":true, "raise":{...}}. Unused keys
are harmless; this guarantees the fixture io-key set equals the C seam set.
"""
import json
import os
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

SEAMS = [
    "MessageEvent", "_is_discord_unknown_interaction", "adapter.send", "admission",
    "alert_adapters_and_config", "asyncio.sleep", "atomic_json_write",
    "bots_require_inline_mention", "build_source", "canonicalize_command",
    "channel_keys", "client_tree", "command_sync_mutation_interval_seconds",
    "command_sync_state_path", "create_thread", "datetime_cutoff",
    "format_thread_chat_name", "free_channels", "free_response_channels",
    "get_allow_bots", "get_commands", "get_effective_topic", "get_home_channel",
    "get_language", "get_parent_channel_id", "handle_message", "in_bot_thread",
    "interaction_guild_id", "is_allowed_user", "is_bot_tag_continuation",
    "logger.debug", "logger.warning", "max_attempts", "present_notification",
    "record_bot_tag_debounce", "recovery_call", "refresh_live_gates",
    "require_mention", "resolve_channel_prompt", "resolve_channel_skills",
    "resolve_interaction_channel", "response.defer", "scoped_gate_env",
    "seed.create_thread", "seed.send", "self_is_explicitly_mentioned",
    "self_is_raw_mentioned", "sha256_hex", "slash_role_grant", "t", "thread.send",
    "thread_channel_prompt", "thread_created", "thread_parent_channel", "to_dict",
    "wait_for_ready", "warn_if_fail_closed_default",
]

DEFAULT_IO = {s: None for s in SEAMS}
DEFAULT_IO.update({
    "t": "translated",
    "admission": [[True, False]],
    "handle_message": True,
    "recovery_call": False,
    "datetime_cutoff": "2026-01-01T00:00:00+00:00",
    "max_attempts": 3,
    "get_language": "en",
    "sha256_hex": "deadbeef",
    "command_sync_mutation_interval_seconds": 4.5,
    "get_parent_channel_id": "parent-1",
    "format_thread_chat_name": "thread-name",
    "get_effective_topic": "topic",
    "interaction_guild_id": "guild-1",
    "slash_role_grant": True,
    "build_source": "source-1",
    "resolve_channel_skills": [[]],
    "thread_channel_prompt": "prompt",
    "MessageEvent": "event-1",
    "create_thread": "thread-1",
    "thread_created": {"id": "thread-1"},
    "seed.send": "seed-1",
    "seed.create_thread": "thread-2",
    "alert_adapters_and_config": [[{"telegram": {"ok": True}}, {"telegram": {"chat_id": "c1"}}]],
    "get_home_channel": {"chat_id": "c1"},
    "present_notification": True,
    "adapter.send": {"success": True},
    "client_tree": "tree-1",
    "get_commands": [["cmd-b", "cmd-a"]],
    "to_dict": {"type": 1, "name": "x"},
    "canonicalize_command": {"type": 1, "name": "x"},
    "command_sync_state_path": "/tmp/state.json",
    "atomic_json_write": None,
    "resolve_channel_prompt": "channel-prompt",
    "dedup.is_duplicate": False,
    "dedup.contains": False,
    "refresh_live_gates": None,
    "get_allow_bots": "all",
    "is_bot_tag_continuation": False,
    "bots_require_inline_mention": False,
    "self_is_raw_mentioned": False,
    "is_allowed_user": True,
    "warn_if_fail_closed_default": None,
    "scoped_gate_env": "true",
    "response.defer": None,
    "wait_for_ready": None,
    "record_bot_tag_debounce": None,
    "in_bot_thread": False,
    "self_is_explicitly_mentioned": False,
    "require_mention": False,
    "free_channels": [[]],
    "free_response_channels": [[]],
    "resolve_interaction_channel": {"is_dm": False},
    "thread_parent_channel": "parent-1",
    "channel_keys": [[]],
    "thread.send": None,
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

# _create_thread (6)
CASES += [
    case("_create_thread", {"name": "  general  ", "message": "  hi  ", "auto_archive_duration": 1440,
                            "interaction": {"display_name": "alice"}}),
    case("_create_thread", {"name": "   ", "message": "", "auto_archive_duration": 1440}),
    case("_create_thread", {"name": "general", "auto_archive_duration": 7}),
    case("_create_thread", {"name": "general", "auto_archive_duration": 60},
         {"resolve_interaction_channel": None}),
    case("_create_thread", {"name": "general", "auto_archive_duration": 60},
         {"resolve_interaction_channel": {"is_dm": True}}),
    case("_create_thread", {"name": "general", "message": "seed", "auto_archive_duration": 60},
         {"create_thread": sr("Exception", "no perm"), "seed.send": "seed-2",
          "seed.create_thread": "thread-3"}),
]

# _defer_unless_expired (6)
CASES += [
    case("_defer_unless_expired", {"warn_fmt": "warn %s", "warn_args": ["x"]}),
    case("_defer_unless_expired", {"warn_fmt": "warn", "warn_args": []},
         {"response.defer": sr("ValueError", "bad")}),
    case("_defer_unless_expired", {"warn_fmt": "warn", "warn_args": ["a", "b"]},
         {"response.defer": sr("ValueError", "unknown interaction"),
          "_is_discord_unknown_interaction": True}),
    case("_defer_unless_expired", {"warn_fmt": "warn", "warn_args": []},
         {"response.defer": sr("SystemExit", "bye")}),
    case("_defer_unless_expired", {"warn_fmt": "w", "warn_args": ["z"]},
         {"response.defer": None}),
    case("_defer_unless_expired", {"warn_fmt": "w", "warn_args": ["q"]},
         {"response.defer": sr("ValueError", "unknown interaction"),
          "_is_discord_unknown_interaction": False}, note="fail-closed"),
]

# _define_discord_view_classes (6) — no seams, no args
for i in range(6):
    CASES.append(case("_define_discord_view_classes", {"phase": i}))

# _deliver_unauthorized_slash_alert (6)
CASES += [
    case("_deliver_unauthorized_slash_alert", {"profile": "p", "user_name": "u", "user_id": "1",
         "chan_id": "c", "guild_id": "g", "command_text": "/x", "reason": "r"}),
    case("_deliver_unauthorized_slash_alert", {"profile": "p"},
         {"alert_adapters_and_config": sr("RuntimeError", "no profile")}),
    case("_deliver_unauthorized_slash_alert", {"profile": "p"},
         {"get_home_channel": None}),
    case("_deliver_unauthorized_slash_alert", {"profile": "p"},
         {"get_home_channel": {"chat_id": None}}),
    case("_deliver_unauthorized_slash_alert", {"profile": "p"},
         {"present_notification": False}),
    case("_deliver_unauthorized_slash_alert", {"profile": "p"},
         {"adapter.send": {"success": False, "error": "nope"},
          "get_home_channel": {"chat_id": "c1"}}),
]

# _desired_command_sync_fingerprint (6)
CASES += [
    case("_desired_command_sync_fingerprint", {"client": False}),
    case("_desired_command_sync_fingerprint", {"client": True}, {"client_tree": None}),
    case("_desired_command_sync_fingerprint", {"client": True},
         {"get_commands": [["b", "a"]], "to_dict": [{"type": 1, "name": "x"}, {"type": 1, "name": "y"}],
          "canonicalize_command": [{"type": 1, "name": "x"}, {"type": 1, "name": "y"}]}),
    case("_desired_command_sync_fingerprint", {"client": True},
         {"get_commands": [["c1"]], "to_dict": sr("ValueError", "bad")}),
    case("_desired_command_sync_fingerprint", {"client": True},
         {"get_language": sr("RuntimeError", "no lang")}),
    case("_desired_command_sync_fingerprint", {"client": True},
         {"sha256_hex": sr("RuntimeError", "hash fail")}),
]

# _discord_message_admission (8)
CASES += [
    case("_discord_message_admission", {"message_id": "m1", "claim": True, "is_dm": False,
         "author_id": "a1", "author_self": False, "type": 0, "guild": {"id": "g"},
         "channel_id": "ch1", "has_allowed_roles": False, "mentions": [], "name": "bot",
         "is_automated": False}),
    case("_discord_message_admission", {"message_id": "m1", "claim": True, "is_dm": False,
         "author_self": True, "type": 0, "guild": {"id": "g"}, "channel_id": "ch1"},
         {}),
    case("_discord_message_admission", {"message_id": "m1", "claim": True, "type": 5,
         "guild": {"id": "g"}, "channel_id": "ch1"}, {}),
    case("_discord_message_admission", {"message_id": "m1", "claim": True, "is_dm": False,
         "author_id": "a1", "type": 0, "guild": {"id": "g"}, "channel_id": "ch1"},
         {"dedup.is_duplicate": True}),
    case("_discord_message_admission", {"message_id": "m1", "claim": True, "is_dm": False,
         "author_id": "a1", "type": 0, "guild": {"id": "g"}, "channel_id": "ch1"},
         {"is_allowed_user": False}),
    case("_discord_message_admission", {"message_id": "m1", "claim": True, "is_dm": False,
         "author_id": "a1", "type": 0, "guild": {"id": "g"}, "channel_id": "ch1",
         "is_automated": True}, {"get_allow_bots": "mentions"}),
    case("_discord_message_admission", {"message_id": "m1", "claim": True, "is_dm": False,
         "author_id": "a1", "type": 0, "guild": {"id": "g"}, "channel_id": "ch1",
         "mentions": [{"bot": True, "is_self": False}], "name": "bot"},
         {"self_is_explicitly_mentioned": False, "scoped_gate_env": "true"}),
    case("_discord_message_admission", {"message_id": "m1", "claim": False, "is_dm": True,
         "author_id": "a1", "type": 19, "guild": None, "channel_id": "ch1"}, {}),
]

# DB helpers (6 each)
CASES += [
    case("_discord_message_is_persistently_complete", {"message_id": "m1"}),
    case("_discord_message_is_persistently_complete", {"message_id": ""}),
    case("_discord_message_is_persistently_complete", {"message_id": "m1"}, {"recovery_call": True}),
    case("_discord_message_is_persistently_complete", {"message_id": "m1"},
         {"recovery_call": sr("RuntimeError", "db down")}),
    case("_discord_message_is_persistently_complete", {"message_id": "m2"}),
    case("_discord_message_is_persistently_complete", {"message_id": "m3"}, {"recovery_call": [False, True]}),
]
CASES += [
    case("_discord_message_has_active_claim", {"message_id": "m1"}),
    case("_discord_message_has_active_claim", {"message_id": ""}),
    case("_discord_message_has_active_claim", {"message_id": "m1"}, {"recovery_call": True}),
    case("_discord_message_has_active_claim", {"message_id": "m1"},
         {"datetime_cutoff": sr("RuntimeError", "clock")}),
    case("_discord_message_has_active_claim", {"message_id": "m2"}),
    case("_discord_message_has_active_claim", {"message_id": "m3"}, {"recovery_call": [True, False]}),
]
CASES += [
    case("_discord_message_attempts_exhausted", {"message_id": "m1", "name": "bot"}),
    case("_discord_message_attempts_exhausted", {"message_id": ""}),
    case("_discord_message_attempts_exhausted", {"message_id": "m1", "name": "bot"}, {"recovery_call": True}),
    case("_discord_message_attempts_exhausted", {"message_id": "m1", "name": "bot"},
         {"max_attempts": sr("RuntimeError", "cfg")}),
    case("_discord_message_attempts_exhausted", {"message_id": "m1", "name": "bot"},
         {"recovery_call": sr("RuntimeError", "db")}),
    case("_discord_message_attempts_exhausted", {"message_id": "m2", "name": "bot"},
         {"recovery_call": [True, False]}),
]
CASES += [
    case("_discord_recovery_cursor", {"channel_id": "ch1"}),
    case("_discord_recovery_cursor", {"channel_id": ""}),
    case("_discord_recovery_cursor", {"channel_id": "ch1"}, {"recovery_call": "42"}),
    case("_discord_recovery_cursor", {"channel_id": "ch1"}, {"recovery_call": None}),
    case("_discord_recovery_cursor", {"channel_id": "ch1"},
         {"recovery_call": sr("RuntimeError", "db")}),
    case("_discord_recovery_cursor", {"channel_id": "ch2"}, {"recovery_call": [None, "9"]}),
]

# dispatches (6 each)
CASES += [
    case("_dispatch_discord_message", {"message_id": "m1", "ready": True}),
    case("_dispatch_discord_message", {"message_id": "m1", "ready": True}, {"admission": [False, False]}),
    case("_dispatch_discord_message", {"message_id": "m1", "ready": False}, {"wait_for_ready": sr("TimeoutError", "timeout")}),
    case("_dispatch_discord_message", {"message_id": "m1", "ready": False}, {"wait_for_ready": sr("ValueError", "boom")}),
    case("_dispatch_discord_message", {"message_id": "m1", "ready": True},
         {"admission": [True, True], "handle_message": "handled"}),
    case("_dispatch_discord_message", {"message_id": "m2", "ready": True},
         {"admission": sr("RuntimeError", "gate")}),
]
CASES += [
    case("_dispatch_recovered_message", {"message_id": "m1", "is_dm": True}),
    case("_dispatch_recovered_message", {"message_id": "m1", "is_dm": False, "channel_id": "ch1"}),
    case("_dispatch_recovered_message", {"message_id": "m1", "is_dm": False, "channel_id": "ch1"},
         {"require_mention": True, "free_channels": [["other"]], "channel_keys": [["ch1"]]}),
    case("_dispatch_recovered_message", {"message_id": "m1", "is_dm": False, "channel_id": "ch1"},
         {"require_mention": True, "free_channels": [["*"]]}),
    case("_dispatch_recovered_message", {"message_id": "m1", "is_dm": False, "channel_id": "ch1"},
         {"admission": [False, False]}),
    case("_dispatch_recovered_message", {"message_id": "m2", "is_dm": False, "channel_id": "ch2"},
         {"admission": [True, True], "handle_message": "ok"}),
]
CASES += [
    case("_dispatch_thread_session", {"thread_id": "t1", "user_id": "u1", "user_name": "bob", "text": "hello"}),
    case("_dispatch_thread_session", {"thread_id": "t1", "user_id": "u1", "user_name": "bob", "text": "hi"},
         {"get_parent_channel_id": None}),
    case("_dispatch_thread_session", {"thread_id": "t1", "user_id": "u1", "user_name": "bob", "text": "x"},
         {"slash_role_grant": False}),
    case("_dispatch_thread_session", {"thread_id": "t1", "user_id": "u1", "user_name": "bob", "text": "x"},
         {"get_parent_channel_id": sr("RuntimeError", "no parent")}),
    case("_dispatch_thread_session", {"thread_id": "t1", "user_id": "u1", "user_name": "bob", "text": "x"},
         {"thread_channel_prompt": None}),
    case("_dispatch_thread_session", {"thread_id": "t2", "user_id": "u2", "user_name": "carol", "text": "y"},
         {"handle_message": sr("RuntimeError", "handler")}),
]

# write / sleep / resolve (6 each)
CASES += [
    case("_write_command_sync_state", {"state": {"a": 1}}),
    case("_write_command_sync_state", {"state": {}}),
    case("_write_command_sync_state", {"state": {"b": 2}}, {"command_sync_state_path": "p2"}),
    case("_write_command_sync_state", {"state": {"a": 1}},
         {"command_sync_state_path": sr("RuntimeError", "path")}),
    case("_write_command_sync_state", {"state": None}),
    case("_write_command_sync_state", {"state": {"x": [1, 2]}}),
]
CASES += [
    case("_sleep_between_command_sync_mutations", {}),
    case("_sleep_between_command_sync_mutations", {}, {"command_sync_mutation_interval_seconds": 0}),
    case("_sleep_between_command_sync_mutations", {}, {"command_sync_mutation_interval_seconds": -1}),
    case("_sleep_between_command_sync_mutations", {}, {"command_sync_mutation_interval_seconds": 2.5}),
    case("_sleep_between_command_sync_mutations", {},
         {"command_sync_mutation_interval_seconds": sr("RuntimeError", "cfg")}),
    case("_sleep_between_command_sync_mutations", {}, {"asyncio.sleep": sr("RuntimeError", "sleep")}),
]
CASES += [
    case("_resolve_channel_prompt", {"channel_id": "ch1", "parent_id": "p1", "extra": {"k": 1}}),
    case("_resolve_channel_prompt", {"channel_id": "ch1", "parent_id": None}),
    case("_resolve_channel_prompt", {"channel_id": "ch1"},
         {"resolve_channel_prompt": None}),
    case("_resolve_channel_prompt", {"channel_id": "ch2", "parent_id": "p2"},
         {"resolve_channel_prompt": "custom"}),
    case("_resolve_channel_prompt", {"channel_id": "ch1", "parent_id": "p1"},
         {"resolve_channel_prompt": sr("RuntimeError", "resolve")}),
    case("_resolve_channel_prompt", {"channel_id": "ch3", "extra": {"z": 2}},
         {"resolve_channel_prompt": sc("v")}),
]


def main():
    out = REPO / "tests" / "fixtures" / "discord_adapter_flow10.jsonl"
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
