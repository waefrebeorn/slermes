#!/usr/bin/env python3
"""Deterministic case generator for the Discord adapter FLOW part 14 oracle."""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow14.jsonl"

SEAM_NAMES = [
    "backfill_enabled", "utc_now_iso", "message_channel_ids", "record_discord_message_seen",
    "with_recovery_db", "conn_execute", "conn_fetchone",
    "advance_recovery_cursor", "time_time", "os_getpid", "json_dumps",
    "backfill_window", "backfill_limit",
    "update_command_sync_entry",
    "native_slash_commands", "tree_get_commands", "tree_command",
    "tree_add_command", "app_command_ctor", "skill_commands_by_category",
    "slash_proxy", "resolve_config_gates", "command_registry",
    "is_gateway_available", "iter_plugin_command_entries",
    "scoped_gate_env", "apply_owner_only_visibility",
    "register_thread_slash", "register_skill_group",
    "refresh_skill_catalog_state",
    "logger_info", "logger_debug", "logger_warning",
    "t_discord", "truncate_text", "t", "unauthorized",
    "response_send_message", "notify_unauthorized_slash", "create_task",
    "to_thread", "record_discord_response",
]

# Benign defaults so every seam is present in every case's io (coverage) and
# no case fails closed on an unused seam. Per-case overrides replace values.
BASE_IO = {
    "backfill_enabled": True,
    "utc_now_iso": "2024-01-01T00:00:00+00:00",
    "message_channel_ids": {"__script__": True, "ret": ["10", "10", "10"]},
    "record_discord_message_seen": None,
    "with_recovery_db": None,
    "conn_execute": None,
    "conn_fetchone": None,
    "advance_recovery_cursor": None,
    "time_time": 1000.0,
    "os_getpid": 42,
    "json_dumps": "[]",
    "backfill_window": 21600.0,
    "backfill_limit": 100,
    "update_command_sync_entry": None,
    "native_slash_commands": {"__script__": True, "ret": []},
    "tree_get_commands": {"__script__": True, "ret": []},
    "tree_command": None,
    "tree_add_command": None,
    "app_command_ctor": None,
    "skill_commands_by_category": {"__script__": True, "ret": [[], [], 0]},
    "slash_proxy": "<proxy>",
    "resolve_config_gates": {},
    "command_registry": {"__script__": True, "ret": []},
    "is_gateway_available": True,
    "iter_plugin_command_entries": {"__script__": True, "ret": []},
    "scoped_gate_env": "false",
    "apply_owner_only_visibility": None,
    "register_thread_slash": None,
    "register_skill_group": None,
    "refresh_skill_catalog_state": {},
    "logger_info": None,
    "logger_debug": None,
    "logger_warning": None,
    "t_discord": "desc",
    "truncate_text": "desc",
    "t": "desc",
    "unauthorized": "unauthorized",
    "response_send_message": None,
    "notify_unauthorized_slash": None,
    "create_task": None,
    "to_thread": None,
    "record_discord_response": None,
}
assert set(BASE_IO) == set(SEAM_NAMES), (set(BASE_IO) ^ set(SEAM_NAMES))

CASES = []


def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


def add(func, args=None, state=None, io=None):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    merged = dict(BASE_IO)
    if io:
        merged.update(io)
    c["in"]["io"] = merged
    CASES.append(c)


MSG = {"id": 55, "author": {"id": "u1"}, "created_at": {"__stamp__": "2024-01-01T00:00:00"}}

# 1. _record_command_sync_attempt
add("_record_command_sync_attempt", args={"app_id": "A", "fingerprint": "F"})
add("_record_command_sync_attempt", args={"app_id": 5, "fingerprint": ""})
add("_record_command_sync_attempt", args={"app_id": None, "fingerprint": "X"})
add("_record_command_sync_attempt", args={"app_id": "A", "fingerprint": "F"},
    io={"update_command_sync_entry": sc(raise_type="RuntimeError", raise_msg="u")})
add("_record_command_sync_attempt", args={"app_id": "A", "fingerprint": "F"},
    io={"update_command_sync_entry": sc(raise_type="KeyboardInterrupt", raise_msg="k")})
add("_record_command_sync_attempt", args={"app_id": "B", "fingerprint": "F\u00e9"})

# 2. _record_command_sync_success
add("_record_command_sync_success", args={"app_id": "A", "fingerprint": "F", "summary": {"ok": 1}},
    io={"time_time": 1000.0})
add("_record_command_sync_success", args={"app_id": "A", "fingerprint": "F", "summary": {}})
add("_record_command_sync_success", args={"app_id": "A", "fingerprint": "F", "summary": [1, 2]},
    io={"time_time": sc(raise_type="ValueError", raise_msg="t")})
add("_record_command_sync_success", args={"app_id": "A", "fingerprint": "F", "summary": {"x": 1}},
    io={"time_time": sc(raise_type="SystemExit", raise_msg="bye")})
add("_record_command_sync_success", args={"app_id": "A", "fingerprint": "F", "summary": {"y": 2}},
    io={"update_command_sync_entry": sc(raise_type="OSError", raise_msg="u")})
add("_record_command_sync_success", args={"app_id": None, "fingerprint": "", "summary": "S"})

# 3. _missed_message_backfill_enabled
add("_missed_message_backfill_enabled", args={"config_extra": {"enabled": True}})
add("_missed_message_backfill_enabled", args={"config_extra": {"enabled": "YES"}})
add("_missed_message_backfill_enabled", args={"config_extra": {"enabled": "no"}})
add("_missed_message_backfill_enabled", args={"config_extra": {"enabled": 0}})
add("_missed_message_backfill_enabled", args={"config_extra": None},
    io={"scoped_gate_env": "TRUE"})
add("_missed_message_backfill_enabled", args={"config_extra": {}},
    io={"scoped_gate_env": "false"})
add("_missed_message_backfill_enabled", args={"config_extra": None},
    io={"scoped_gate_env": sc(raise_type="RuntimeError", raise_msg="e")})

# 4. _record_discord_message_seen
add("_record_discord_message_seen", args={"message": MSG, "status": "discovered"})
add("_record_discord_message_seen", args={"message": MSG, "status": "processing"},
    io={"conn_fetchone": sc(ret=["processing", "OLD"])})
add("_record_discord_message_seen", args={"message": MSG, "status": "discovered"},
    io={"conn_fetchone": sc(ret=["processing", "OLD"])})
add("_record_discord_message_seen", args={"message": MSG, "status": "processing"},
    io={"conn_fetchone": sc(ret=["responded", "T"])})
add("_record_discord_message_seen", args={"message": MSG, "status": "discovered"},
    io={"conn_fetchone": sc(ret=[])})
add("_record_discord_message_seen", args={"message": MSG, "status": "discovered"},
    io={"backfill_enabled": False})
add("_record_discord_message_seen", args={"message": {"id": 1, "created_at": "x"}, "status": "discovered"})
add("_record_discord_message_seen", args={"message": MSG, "status": "discovered"},
    io={"message_channel_ids": sc(raise_type="SystemExit", raise_msg="bye")})
add("_record_discord_message_seen", args={"message": MSG, "status": "discovered"},
    io={"conn_execute": sc(raise_type="OSError", raise_msg="db")})

# 5. _record_recovery_attempt
add("_record_recovery_attempt", args={"message": MSG, "status": "queued"})
add("_record_recovery_attempt", args={"message": MSG, "status": "failed", "error": "boom"})
add("_record_recovery_attempt", args={"message": MSG, "status": "queued"},
    io={"backfill_enabled": False})
add("_record_recovery_attempt", args={"message": MSG, "status": "queued"},
    io={"record_discord_message_seen": sc(raise_type="SystemExit", raise_msg="bye")})
add("_record_recovery_attempt", args={"message": {"id": None}, "status": "queued"})
add("_record_recovery_attempt", args={"message": MSG, "status": "queued"},
    io={"conn_execute": sc(raise_type="RuntimeError", raise_msg="db")})

# 6. _record_discord_processing_start
add("_record_discord_processing_start", args={"raw_message": MSG, "message_id": "", "emoji_ack": True})
add("_record_discord_processing_start", args={"raw_message": MSG, "message_id": "", "emoji_ack": False})
add("_record_discord_processing_start", args={"raw_message": MSG, "message_id": "", "emoji_ack": True},
    io={"backfill_enabled": False})
add("_record_discord_processing_start", args={"raw_message": {"id": None}, "message_id": "FB", "emoji_ack": True})
add("_record_discord_processing_start", args={"raw_message": {"id": None}, "message_id": "", "emoji_ack": True})
add("_record_discord_processing_start", args={"raw_message": MSG, "message_id": "", "emoji_ack": True},
    io={"record_discord_message_seen": sc(raise_type="KeyboardInterrupt", raise_msg="k")})

# 7. _record_discord_processing_complete
add("_record_discord_processing_complete", args={"raw_message": MSG, "outcome": "success"})
add("_record_discord_processing_complete", args={"raw_message": MSG, "outcome": "cancelled"})
add("_record_discord_processing_complete", args={"raw_message": MSG, "outcome": "failed"})
add("_record_discord_processing_complete", args={"raw_message": MSG, "outcome": "success"},
    io={"backfill_enabled": False})
add("_record_discord_processing_complete", args={"raw_message": {"id": None}, "message_id": "", "outcome": "success"})
add("_record_discord_processing_complete", args={"raw_message": MSG, "outcome": "success"},
    io={"conn_execute": sc(raise_type="OSError", raise_msg="db")})
add("_record_discord_processing_complete", args={"raw_message": MSG, "outcome": "other"})

# 8. _record_response_async
add("_record_response_async", args={"reply_to": "A", "success": True, "final": True})
add("_record_response_async", args={"reply_to": None, "success": True, "final": True,
                                    "metadata": {"reply_to_message_id": "M"}})
add("_record_response_async", args={"reply_to": "", "success": False, "final": False, "metadata": {}})
add("_record_response_async", args={"reply_to": "A", "success": True, "final": True},
    io={"to_thread": sc(raise_type="SystemExit", raise_msg="bye")})
add("_record_response_async", args={"reply_to": "A", "success": True, "final": True},
    io={"record_discord_response": sc(raise_type="RuntimeError", raise_msg="r")})
add("_record_response_async", args={"reply_to": "A", "success": False, "final": True,
                                    "result_message_id": "R"})

# 9. _record_discord_response
add("_record_discord_response", args={"reply_to": "A", "success": True, "final": True,
                                      "result_message_id": "R"},
    io={"conn_fetchone": sc(ret=["5"])})
add("_record_discord_response", args={"reply_to": "A", "success": False, "final": True})
add("_record_discord_response", args={"reply_to": "A", "success": True, "final": False})
add("_record_discord_response", args={"reply_to": None, "success": True, "final": True})
add("_record_discord_response", args={"reply_to": "A", "success": True, "final": True},
    io={"backfill_enabled": False})
add("_record_discord_response", args={"reply_to": "A", "success": True, "final": True},
    io={"conn_fetchone": None})
add("_record_discord_response", args={"reply_to": "A", "success": True, "final": True},
    io={"conn_execute": sc(raise_type="KeyboardInterrupt", raise_msg="k")})

# 10. _record_recovery_scan_start
add("_record_recovery_scan_start", args={"channels": ["a", "b"]},
    io={"time_time": 1000.0, "os_getpid": 42, "json_dumps": '["a", "b"]',
        "backfill_window": 21600.0, "backfill_limit": 100})
add("_record_recovery_scan_start", args={"channels": []}, io={"json_dumps": "[]"})
add("_record_recovery_scan_start", args={"channels": ["z", "a"]}, io={"json_dumps": '["a", "z"]'})
add("_record_recovery_scan_start", args={"channels": ["a"]},
    io={"time_time": sc(raise_type="RuntimeError", raise_msg="t")})
add("_record_recovery_scan_start", args={"channels": ["a"]},
    io={"conn_execute": sc(raise_type="SystemExit", raise_msg="bye")})
add("_record_recovery_scan_start", args={"channels": ["a", "b", "c"]},
    io={"json_dumps": '["a", "b", "c"]', "backfill_window": 60.0, "backfill_limit": 5})

# 11. _record_recovery_scan_complete
add("_record_recovery_scan_complete", args={"scan_id": "s1", "status": "completed",
    "scanned": 5, "missed": 2, "dispatched": 1})
add("_record_recovery_scan_complete", args={"scan_id": "s2", "status": "failed",
    "scanned": 0, "missed": 0, "dispatched": 0, "error": "x"})
add("_record_recovery_scan_complete", args={"scan_id": "s3", "status": "partial",
    "scanned": 9, "missed": 1, "dispatched": 8})
add("_record_recovery_scan_complete", args={"scan_id": "s1"},
    io={"conn_execute": sc(raise_type="OSError", raise_msg="db")})
add("_record_recovery_scan_complete", args={"scan_id": "s1"},
    io={"utc_now_iso": sc(raise_type="SystemExit", raise_msg="bye")})
add("_record_recovery_scan_complete", args={"scan_id": "s4", "status": "completed",
    "scanned": 1, "missed": 1, "dispatched": 1})

# 12. _refresh_skill_catalog_state
add("_refresh_skill_catalog_state", args={"reserved_names": ["a"]},
    io={"skill_commands_by_category": sc(ret=[{}, [["a", "da", "ka"]], 0])})
add("_refresh_skill_catalog_state", args={"reserved_names": []},
    io={"skill_commands_by_category": sc(ret=[{"c": [["z", "dz", "kz"], ["b", "db", "kb"]]},
                                             [["a", "da", "ka"]], 2])})
add("_refresh_skill_catalog_state", args={"reserved_names": ["x", "a"]},
    io={"skill_commands_by_category": sc(ret=[{}, [], 0])})
add("_refresh_skill_catalog_state", args={"reserved_names": []},
    io={"skill_commands_by_category": sc(raise_type="RuntimeError", raise_msg="s")})
add("_refresh_skill_catalog_state", args={"reserved_names": []},
    io={"skill_commands_by_category": sc(ret=[{}, [["m", "", "k"]], 0])})
add("_refresh_skill_catalog_state", args={"reserved_names": ["b", "a"]},
    io={"skill_commands_by_category": sc(ret=[{}, [["a", "d", "k"], ["a", "d2", "k2"]], 1])})

# 13. _register_skill_group
ENTRIES = [["a", "desc-a", "ka"], ["b", "desc-b", "kb"]]
add("_register_skill_group", args={"name": "bot"},
    io={"tree_get_commands": sc(ret=["x"]), "refresh_skill_catalog_state": {"entries": ENTRIES}})
add("_register_skill_group", args={"name": "bot"}, io={"tree_get_commands": sc(ret=[])})
add("_register_skill_group", args={"name": "bot"},
    io={"tree_get_commands": sc(raise_type="RuntimeError", raise_msg="g"),
        "refresh_skill_catalog_state": {"entries": ENTRIES}})
add("_register_skill_group", args={"name": "bot"},
    io={"refresh_skill_catalog_state": {"entries": []}})
add("_register_skill_group", args={"name": "bot"},
    io={"refresh_skill_catalog_state": {"entries": ENTRIES},
        "app_command_ctor": sc(raise_type="SystemExit", raise_msg="bye")})
add("_register_skill_group", args={"name": "bot"},
    io={"refresh_skill_catalog_state": {"entries": ENTRIES},
        "logger_info": sc(raise_type="RuntimeError", raise_msg="log")})
add("_register_skill_group", args={"name": "bot"},
    io={"tree_get_commands": sc(ret=["x", "y"]),
        "refresh_skill_catalog_state": {"entries": [["only", "d", "k"]]}})

# 14. _register_slash_commands
NATIVE = [["t1", "desc1", [], "/t1", False], ["thread", "d", [], None, False]]
add("_register_slash_commands", args={"no_client": True})
add("_register_slash_commands", args={}, io={"native_slash_commands": sc(ret=NATIVE)})
add("_register_slash_commands", args={}, io={"native_slash_commands": sc(ret=[["x", "d", [], "/x {args}", True]])})
add("_register_slash_commands", args={},
    io={"command_registry": sc(ret=[["cmd1", "desc1", "hint1"], ["cmd2", "desc2", ""]]),
        "is_gateway_available": [True, False]})
add("_register_slash_commands", args={},
    io={"command_registry": sc(ret=[[f"c{i}", "d", ""] for i in range(105)]),
        "is_gateway_available": True, "tree_get_commands": sc(ret=[])})
add("_register_slash_commands", args={},
    io={"iter_plugin_command_entries": sc(ret=[["p1", "pd", "ph"]])})
add("_register_slash_commands", args={},
    io={"scoped_gate_env": "true"})
add("_register_slash_commands", args={},
    io={"command_registry": sc(raise_type="RuntimeError", raise_msg="c")})

# 15. _register_thread_slash
add("_register_thread_slash", args={"name": "thr", "description": "desc"})
add("_register_thread_slash", args={"name": "", "description": ""})
add("_register_thread_slash", args={"name": "thr", "description": "desc"},
    io={"tree_command": sc(raise_type="SystemExit", raise_msg="bye")})
add("_register_thread_slash", args={"name": "thr", "description": "desc"},
    io={"t_discord": ["n", "m", "a"]})
add("_register_thread_slash", args={"name": "t" * 40, "description": "d"})
add("_register_thread_slash", args={"name": "thr", "description": "desc"},
    io={"t_discord": sc(raise_type="ValueError", raise_msg="t")})

# 16. _reject_slash
INT = {"user": {"id": 7, "name": "u"}, "channel_id": "10", "guild_id": "20"}
add("_reject_slash", args={"interaction": INT, "command_text": "/x", "reason": "no"})
add("_reject_slash", args={"interaction": {"channel_id": "10"}, "command_text": "/y", "reason": "r"})
add("_reject_slash", args={"interaction": {"channel": {"id": "99"}, "guild_id": None},
                          "command_text": "/z arg", "reason": "q"})
add("_reject_slash", args={"interaction": INT, "command_text": "/x", "reason": "no"},
    io={"response_send_message": sc(raise_type="OSError", raise_msg="send")})
add("_reject_slash", args={"interaction": INT, "command_text": "/x", "reason": "no"},
    io={"response_send_message": sc(raise_type="KeyboardInterrupt", raise_msg="k")})
add("_reject_slash", args={"interaction": INT, "command_text": "/x", "reason": "no"},
    io={"create_task": sc(raise_type="RuntimeError", raise_msg="ct")})
add("_reject_slash", args={"interaction": INT, "command_text": "/x", "reason": "no"},
    io={"create_task": sc(raise_type="SystemExit", raise_msg="bye")})


counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
assert all(v >= 6 for v in counts.values()), {k: v for k, v in counts.items() if v < 6}
all_io = set()
all_funcs = set()
for c in CASES:
    all_io |= set(c["in"]["io"])
    all_funcs.add(c["func"])
assert all_io == set(SEAM_NAMES), (set(SEAM_NAMES) - all_io, all_io - set(SEAM_NAMES))
EXPECTED_FUNCS = {
    "_record_command_sync_attempt", "_record_command_sync_success",
    "_missed_message_backfill_enabled", "_record_discord_message_seen",
    "_record_recovery_attempt", "_record_discord_processing_start",
    "_record_discord_processing_complete", "_record_response_async",
    "_record_discord_response", "_record_recovery_scan_start",
    "_record_recovery_scan_complete", "_refresh_skill_catalog_state",
    "_register_skill_group", "_register_slash_commands",
    "_register_thread_slash", "_reject_slash",
}
assert all_funcs == EXPECTED_FUNCS, (EXPECTED_FUNCS - all_funcs, all_funcs - EXPECTED_FUNCS)

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT} ({len(counts)} funcs, {len(all_io)} seams)")
for f in sorted(counts):
    print(f"  {f}: {counts[f]}")
