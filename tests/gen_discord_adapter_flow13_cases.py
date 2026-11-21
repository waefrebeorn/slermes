#!/usr/bin/env python3
"""Deterministic case generator for the Discord adapter FLOW part 13 oracle.

Writes tests/fixtures/discord_adapter_flow13.jsonl. Each case is
{"func", "in": {"args", "state"?, "io"}}. Re-running reproduces byte-identical
fixtures. Every ported function gets >= 6 cases; every seam name appears in at
least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow13.jsonl"

SEAM_NAMES = [
    "emit_platform_event", "message_event_parts", "thread_event_parts",
    "ws_is_ready", "ws_is_closed", "perf_counter",
    "command_sync_state_path", "path_exists", "path_read_text", "json_loads",
    "time_time", "update_command_sync_entry",
    "import_module", "mutagen_file", "subprocess_run",
    "named_tempfile", "to_thread", "is_whisper_hallucination", "logger_info",
    "voice_input_callback", "logger_warning", "os_unlink",
    "validate_media_size", "attachment_read",
    "is_safe_url", "session_get", "urljoin", "resp_read",
    "get_running_loop", "settle_reason", "view_retire", "loop_create_task",
    "bg_add", "add_done_callback", "call_soon_threadsafe", "observe_gateway_settle",
    "logger_debug", "nacl_decrypt", "refresh_credentials", "infer_user_for_ssrc",
    "dave_decrypt", "monotonic", "opus_decode",
]

CASES = []
ALL_IO = set()
ALL_FUNCS = set()


def case(func, args=None, state=None, io=None):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    c["in"]["io"] = io if io is not None else {}
    return c


def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


def add(c):
    ALL_IO.update((c["in"].get("io") or {}).keys())
    ALL_FUNCS.add(c["func"])
    CASES.append(c)


# ── event handlers ──────────────────────────────────────────────────────────
MSG = {"id": 10, "content": "hello world", "author": {"id": 77, "display_name": "u"},
       "edited_at": {"__stamp__": "2024-01-01T00:00:00"}}
BUILT = [{"chat_id": "10", "message_id": "10", "text": "hello world"},
         {"chat_id": "10", "user_id": "77"}]
EV_IO = {"message_event_parts": BUILT, "emit_platform_event": None}
add(case("_on_platform_message_edit", args={"before": MSG, "after": MSG}, io=dict(EV_IO)))
add(case("_on_platform_message_edit", args={"before": MSG, "after": None}, io=dict(EV_IO)))
add(case("_on_platform_message_edit", args={"before": MSG, "after": {"content": 5, "id": 2}}, io=dict(EV_IO)))
add(case("_on_platform_message_edit",
         args={"before": MSG, "after": {"content": "x", "edited_at": {"__stamp__": "S" * 80}}},
         io=dict(EV_IO)))
add(case("_on_platform_message_edit", args={"before": MSG, "after": MSG},
         io={"message_event_parts": sc(raise_type="RuntimeError", raise_msg="bad")}))
add(case("_on_platform_message_edit", args={"before": MSG, "after": MSG},
         io={"message_event_parts": BUILT,
             "emit_platform_event": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_on_platform_message_edit", args={"before": MSG, "after": MSG},
         io={"message_event_parts": None, "emit_platform_event": None}))

add(case("_on_platform_message_delete", args={"message": MSG}, io=dict(EV_IO)))
add(case("_on_platform_message_delete", args={"message": {"author": None}}, io=dict(EV_IO)))
add(case("_on_platform_message_delete", args={"message": {"author": {"id": ""}}}, io=dict(EV_IO)))
add(case("_on_platform_message_delete", args={"message": MSG},
         io={"message_event_parts": None, "emit_platform_event": None}))
add(case("_on_platform_message_delete", args={"message": MSG},
         io={"message_event_parts": sc(raise_type="OSError", raise_msg="x")}))
add(case("_on_platform_message_delete", args={"message": MSG},
         io={"message_event_parts": BUILT,
             "emit_platform_event": sc(raise_type="KeyboardInterrupt", raise_msg="k")}))

THREAD = {"id": 5, "name": "topic", "owner_id": 99, "parent_id": 1, "guild": {"id": 7}}
TBUILT = [{"thread_id": "5", "name": "topic", "owner_id": "99"},
          {"chat_id": "5", "user_id": "99"}]
TIO = {"thread_event_parts": TBUILT, "emit_platform_event": None}
add(case("_on_platform_thread_create", args={"thread": THREAD}, io=dict(TIO)))
add(case("_on_platform_thread_create", args={"thread": {"name": 5, "owner_id": None}}, io=dict(TIO)))
add(case("_on_platform_thread_create", args={"thread": {"name": "N" * 300, "owner_id": 1}}, io=dict(TIO)))
add(case("_on_platform_thread_create", args={"thread": THREAD},
         io={"thread_event_parts": None, "emit_platform_event": None}))
add(case("_on_platform_thread_create", args={"thread": THREAD},
         io={"thread_event_parts": sc(raise_type="ValueError", raise_msg="v")}))
add(case("_on_platform_thread_create", args={"thread": THREAD},
         io={"thread_event_parts": TBUILT,
             "emit_platform_event": sc(raise_type="SystemExit", raise_msg="bye")}))

REN = [{"thread_id": "5", "old_name": "a", "new_name": "b"}, {"chat_id": "5"}]
add(case("_on_platform_thread_update", args={"before": {"name": "a"}, "after": {"name": "b"}}, io={"thread_event_parts": REN, "emit_platform_event": None}))
add(case("_on_platform_thread_update", args={"before": {"name": "a"}, "after": {"name": "a"}}, io={"emit_platform_event": None}))
add(case("_on_platform_thread_update", args={"before": {"name": "a"}, "after": {"name": 5}}, io={"emit_platform_event": None}))
add(case("_on_platform_thread_update", args={"before": {"name": 1}, "after": {"name": "b"}}, io={"thread_event_parts": REN, "emit_platform_event": None}))
add(case("_on_platform_thread_update", args={"before": {"name": "a"}, "after": {"name": "B" * 300}}, io={"thread_event_parts": REN, "emit_platform_event": None}))
add(case("_on_platform_thread_update", args={"before": {"name": "a"}, "after": {"name": "b"}}, io={"thread_event_parts": sc(raise_type="RuntimeError", raise_msg="t"), "emit_platform_event": None}))
add(case("_on_platform_thread_update", args={"before": {"name": "a"}, "after": {"name": "b"}}, io={"thread_event_parts": REN, "emit_platform_event": sc(raise_type="SystemExit", raise_msg="bye")}))

# ── websocket health ────────────────────────────────────────────────────────
WS_STATE = {"_heartbeat_ack_max_age_seconds": 30.0, "_max_latency_seconds": 1.0,
            "_event_max_silence_seconds": 0, "_last_dispatched_event_monotonic": None}
WS_CLIENT = {"ws": {"open": True, "_keep_alive": {"_last_ack": 100.0}}, "latency": 0.1}
WS_IO = {"ws_is_ready": True, "ws_is_closed": False, "perf_counter": 101.0}
add(case("_read_websocket_health", args={"client": WS_CLIENT}, state=WS_STATE, io=WS_IO))
add(case("_read_websocket_health", args={"client": WS_CLIENT}, state=WS_STATE,
         io={"ws_is_ready": False}))
add(case("_read_websocket_health", args={"client": WS_CLIENT}, state=WS_STATE,
         io={"ws_is_ready": sc(raise_type="RuntimeError", raise_msg="r")}))
add(case("_read_websocket_health", args={"client": WS_CLIENT}, state=WS_STATE,
         io={"ws_is_ready": True, "ws_is_closed": True}))
add(case("_read_websocket_health", args={"client": WS_CLIENT}, state=WS_STATE,
         io={"ws_is_ready": True, "ws_is_closed": sc(raise_type="OSError", raise_msg="c")}))
add(case("_read_websocket_health", args={"client": {"latency": 0.1}}, state=WS_STATE,
         io={"ws_is_ready": True, "ws_is_closed": False}))
add(case("_read_websocket_health", args={"client": {"ws": {"open": False}, "latency": 0.1}},
         state=WS_STATE, io={"ws_is_ready": True, "ws_is_closed": False}))
add(case("_read_websocket_health", args={"client": {"ws": {"open": True}, "latency": 0.1}},
         state=WS_STATE, io={"ws_is_ready": True, "ws_is_closed": False, "perf_counter": 101.0}))
add(case("_read_websocket_health", args={"client": {"ws": {"open": True, "_keep_alive": {"_last_ack": 0.0}}, "latency": 0.1}},
         state=WS_STATE, io={"ws_is_ready": True, "ws_is_closed": False, "perf_counter": 400.0}))
add(case("_read_websocket_health", args={"client": {"ws": {"open": True, "_keep_alive": {"_last_ack": 100.0}}, "latency": "x"}},
         state=WS_STATE, io={"ws_is_ready": True, "ws_is_closed": False, "perf_counter": 101.0}))
add(case("_read_websocket_health", args={"client": {"ws": {"open": True, "_keep_alive": {"_last_ack": 100.0}}, "latency": 5.0}},
         state=WS_STATE, io={"ws_is_ready": True, "ws_is_closed": False, "perf_counter": 101.0}))
SIL_STATE = dict(WS_STATE, _event_max_silence_seconds=10, _last_dispatched_event_monotonic=0.0)
add(case("_read_websocket_health", args={"client": WS_CLIENT}, state=SIL_STATE,
         io={"ws_is_ready": True, "ws_is_closed": False, "perf_counter": [101.0, 200.0]}))

# ── command sync state ──────────────────────────────────────────────────────
CS_IO = {"command_sync_state_path": "P", "path_exists": True,
         "path_read_text": "{}", "json_loads": {}}
add(case("_read_command_sync_state", io=CS_IO))
add(case("_read_command_sync_state", io={"command_sync_state_path": "P", "path_exists": False}))
add(case("_read_command_sync_state", io={"command_sync_state_path": "P", "path_exists": True,
                                         "path_read_text": sc(raise_type="OSError", raise_msg="r")}))
add(case("_read_command_sync_state", io={"command_sync_state_path": "P", "path_exists": sc(raise_type="ValueError", raise_msg="e")}))
add(case("_read_command_sync_state", io={"command_sync_state_path": sc(raise_type="RuntimeError", raise_msg="p")}))
add(case("_read_command_sync_state", io={"command_sync_state_path": "P", "path_exists": True,
                                         "path_read_text": "[1,2]", "json_loads": [1, 2]}))
add(case("_read_command_sync_state", io={"command_sync_state_path": "P", "path_exists": True,
                                         "path_read_text": "bad", "json_loads": sc(raise_type="ValueError", raise_msg="j")}))
add(case("_read_command_sync_state", io={"command_sync_state_path": "P", "path_exists": True,
                                         "path_read_text": "{}", "json_loads": {"a": 1, "b": 2}}))

RATE_IO = {"time_time": 1000.0, "update_command_sync_entry": None}
add(case("_record_command_sync_rate_limit", args={"app_id": "A", "fingerprint": "F", "retry_after": 5.0}, io=dict(RATE_IO)))
add(case("_record_command_sync_rate_limit", args={"app_id": "A", "fingerprint": "F", "retry_after": 0.25}, io=dict(RATE_IO)))
add(case("_record_command_sync_rate_limit", args={"app_id": "B", "fingerprint": "G", "retry_after": "2.5"}, io=dict(RATE_IO)))
add(case("_record_command_sync_rate_limit", args={"app_id": "A", "fingerprint": "F", "retry_after": -3}, io=dict(RATE_IO)))
add(case("_record_command_sync_rate_limit", args={"app_id": "A", "fingerprint": "F", "retry_after": 5},
         io={"time_time": 1000.0, "update_command_sync_entry": sc(raise_type="OSError", raise_msg="w")}))
add(case("_record_command_sync_rate_limit", args={"app_id": "A", "fingerprint": "F", "retry_after": 5},
         io={"time_time": sc(raise_type="ValueError", raise_msg="t")}))

# ── audio probe ─────────────────────────────────────────────────────────────
PROC_OK = {"returncode": 0, "stdout": "3.5\n", "stderr": ""}
PROC_BAD = {"returncode": 1, "stdout": "", "stderr": ""}
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": "M", "mutagen_file": {"info": {"length": 12.5}}}))
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": "M", "mutagen_file": {"info": {"length": 0}}, "subprocess_run": PROC_OK}))
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": sc(raise_type="ImportError", raise_msg="no"), "subprocess_run": PROC_OK}))
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": "M", "mutagen_file": sc(raise_type="OSError", raise_msg="m"), "subprocess_run": PROC_OK}))
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": sc(raise_type="ImportError", raise_msg="no"), "subprocess_run": PROC_BAD}))
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": sc(raise_type="ImportError", raise_msg="no"), "subprocess_run": {"returncode": 0, "stdout": "", "stderr": ""}}))
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": sc(raise_type="ImportError", raise_msg="no"),
             "subprocess_run": sc(raise_type="OSError", raise_msg="s")}))
add(case("_probe_audio_duration_seconds", args={"audio_path": "/a.wav"},
         io={"import_module": sc(raise_type="SystemExit", raise_msg="bye")}))

# ── voice input ─────────────────────────────────────────────────────────────
VP_BASE = {"named_tempfile": {"name": "/tmp/vc.wav"},
           "to_thread": [None, {"success": True, "transcript": "  hello  "}],
           "is_whisper_hallucination": False, "logger_info": "ok", "os_unlink": None}
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9}, io=dict(VP_BASE)))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9, "voice_cb": True},
         io=dict(VP_BASE, voice_input_callback="ok")))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9},
         io={"named_tempfile": {"name": "/tmp/vc.wav"}, "to_thread": [None, {"success": False}], "os_unlink": None}))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9},
         io={"named_tempfile": {"name": "/tmp/vc.wav"}, "to_thread": [None, {"success": True, "transcript": "   "}], "os_unlink": None}))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9},
         io={"named_tempfile": {"name": "/tmp/vc.wav"},
             "to_thread": [None, {"success": True, "transcript": "hello"}],
             "is_whisper_hallucination": True, "os_unlink": None}))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9, "voice_text_channels": {1: 5}},
         io={"named_tempfile": {"name": "/tmp/vc.wav"},
             "to_thread": [None, {"success": True, "transcript": "hello"}],
             "is_whisper_hallucination": False, "logger_info": "ok", "os_unlink": None}))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9},
         io={"named_tempfile": {"name": "/tmp/vc.wav"},
             "to_thread": sc(raise_type="RuntimeError", raise_msg="boom"),
             "logger_warning": "ok", "os_unlink": None}))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9},
         io={"named_tempfile": {"name": "/tmp/vc.wav"},
             "to_thread": sc(raise_type="ValueError", raise_msg="ff"),
             "logger_warning": "ok", "os_unlink": None}))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9},
         io=dict(VP_BASE, os_unlink=sc(raise_type="OSError", raise_msg="u"))))
add(case("_process_voice_input", args={"guild_id": 1, "user_id": 2, "pcm_data": "AAEC", "captured_for": 9},
         io={"named_tempfile": {"name": "/tmp/vc.wav"},
             "to_thread": sc(raise_type="SystemExit", raise_msg="bye"), "os_unlink": None}))

# ── attachment read ─────────────────────────────────────────────────────────
add(case("_read_attachment_bytes", args={"att": {"filename": "a.png"}}))
add(case("_read_attachment_bytes", args={"att": {"size": 100, "read": "callable", "filename": "a.png"}},
         io={"validate_media_size": None, "attachment_read": "DEADBEEF"}))
add(case("_read_attachment_bytes", args={"att": {"size": "100", "read": "callable", "url": "http://x"}, "media_type": "image"},
         io={"validate_media_size": None, "attachment_read": "AA"}))
add(case("_read_attachment_bytes", args={"att": {"read": "callable"}},
         io={"validate_media_size": None, "attachment_read": "AA"}))
add(case("_read_attachment_bytes", args={"att": {"read": "callable", "filename": "a.png"}},
         io={"validate_media_size": None,
             "attachment_read": sc(raise_type="OSError", raise_msg="r"), "logger_warning": None}))
add(case("_read_attachment_bytes", args={"att": {"read": "callable"}},
         io={"validate_media_size": None,
             "attachment_read": sc(raise_type="SystemExit", raise_msg="bye")}))
add(case("_read_attachment_bytes", args={"att": {"size": 5, "read": "callable"}},
         io={"validate_media_size": sc(raise_type="ValueError", raise_msg="big"),
             "attachment_read": "AA"}))
add(case("_read_attachment_bytes", args={"att": {"size": 0, "read": "callable"}},
         io={"validate_media_size": None, "attachment_read": "AA"}))

# ── URL image redirect guard ───────────────────────────────────────────────
OK_RESP = {"status": 200, "headers": {"Content-Type": "image/png"}}
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x.png", "timeout": 5, "request_kwargs": {}},
         io={"is_safe_url": True, "session_get": OK_RESP, "resp_read": "AABB"}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": False}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": [True, True, True], "session_get": [{"status": 302, "headers": {"location": "/y"}}, OK_RESP],
             "urljoin": "http://a/y", "resp_read": "CC"}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": True, "session_get": {"status": 302, "headers": {}}}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": [True, False], "session_get": {"status": 307, "headers": {"location": "/y"}},
             "urljoin": "http://a/y"}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": sc(raise_type="OSError", raise_msg="s")}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": True, "session_get": sc(raise_type="RuntimeError", raise_msg="g")}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": True, "session_get": OK_RESP,
             "resp_read": sc(raise_type="OSError", raise_msg="r")}))
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": [True, True], "session_get": [{"status": 301, "headers": {"location": "/y"}}, OK_RESP],
             "urljoin": sc(raise_type="ValueError", raise_msg="u"), "resp_read": "CC"}))
# too many redirects (11 iterations)
n = 11
add(case("_read_url_image_with_redirect_guard", args={"url": "http://a/x", "timeout": 5},
         io={"is_safe_url": [True] * (2 * n),
             "session_get": [{"status": 302, "headers": {"location": "/y"}}] * n,
             "urljoin": ["http://a/y"] * n}))

# ── exec approval settle ────────────────────────────────────────────────────
OBS_IO = {"get_running_loop": None, "observe_gateway_settle": True, "settle_reason": "r1",
          "call_soon_threadsafe": None}
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": None, "request_id": "rq", "pending_retire": None}}, io=dict(OBS_IO)))
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": {"id": 1}, "request_id": "rq", "pending_retire": None}},
         io=dict(OBS_IO, view_retire=None, loop_create_task={"id": "t"}, bg_add=None, add_done_callback=None)))
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": {"id": 1}, "request_id": "rq"}},
         io=dict(OBS_IO, view_retire=None, loop_create_task={"id": "t"}, bg_add=None, add_done_callback=None,
                 observe_gateway_settle=False)))
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": None, "request_id": "rq"}},
         io={"get_running_loop": None, "observe_gateway_settle": sc(raise_type="RuntimeError", raise_msg="o")}))
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": None, "request_id": "rq"}},
         io={"get_running_loop": None, "observe_gateway_settle": True, "settle_reason": sc(raise_type="OSError", raise_msg="s")}))
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": {"id": 1}, "request_id": "rq"}},
         io={"get_running_loop": None, "observe_gateway_settle": True, "settle_reason": "r1", "call_soon_threadsafe": None,
             "view_retire": sc(raise_type="ValueError", raise_msg="v")}))
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": {"id": 1}, "request_id": "rq"}},
         io={"get_running_loop": None, "observe_gateway_settle": True, "settle_reason": "r1", "call_soon_threadsafe": None,
             "view_retire": None, "loop_create_task": sc(raise_type="OSError", raise_msg="c")}))
add(case("_observe_exec_approval_settle", args={"session_key": "sk", "view": {"_message": {"id": 1}, "request_id": "rq"}},
         io={"get_running_loop": sc(raise_type="SystemExit", raise_msg="bye")}))

# ── packet pipeline ─────────────────────────────────────────────────────────
def packet_hex(payload="aabbccdd", nonce="01020304", b0="80", b1="78", ssrc="0000002a"):
    return b0 + b1 + "0001" + "00000000" + ssrc + payload + nonce

PSTATE = {"_running": True, "_paused": False, "_creds": ["KEY", None, 0, False],
          "_packet_debug_count": 0, "_bot_ssrc": 99, "_nacl_fail_streak": 0,
          "_decode_failed": 0, "_decode_ok": 0, "_dave_unmapped_dropped": 0,
          "_dave_passthrough_until": 0, "_ssrc_to_user": {"42": "u1"},
          "_decoders": {}, "_buffers": {"42": []}, "_last_packet_time": {}}
PKT = packet_hex()
PDECRYPT = {"nacl_decrypt": "11223344", "logger_debug": "ok", "opus_decode": [[1, 2, 3]],
            "monotonic": 500.0}
add(case("_on_packet", args={"data": PKT}, state=dict(PSTATE, _running=False)))
add(case("_on_packet", args={"data": PKT}, state=dict(PSTATE, _paused=True)))
add(case("_on_packet", args={"data": "8078"}, state=dict(PSTATE), io={"logger_debug": "ok"}))
add(case("_on_packet", args={"data": packet_hex(b0="00")}, state=dict(PSTATE), io={"logger_debug": "ok"}))
add(case("_on_packet", args={"data": PKT}, state=dict(PSTATE, _bot_ssrc=42), io={"logger_debug": "ok"}))
add(case("_on_packet", args={"data": PKT}, state=dict(PSTATE), io=dict(PDECRYPT)))
add(case("_on_packet", args={"data": PKT}, state=dict(PSTATE),
         io={"nacl_decrypt": sc(raise_type="RuntimeError", raise_msg="bad"), "logger_debug": "ok", "logger_warning": "ok"}))
add(case("_on_packet", args={"data": PKT}, state=dict(PSTATE, _nacl_fail_streak=24),
         io={"nacl_decrypt": sc(raise_type="RuntimeError", raise_msg="bad"), "logger_debug": "ok",
             "logger_warning": "ok", "refresh_credentials": None}))
DAVE_STATE = dict(PSTATE, _creds=["KEY", {"__dave__": "D"}, 1, False], _ssrc_to_user={})
add(case("_on_packet", args={"data": PKT}, state=DAVE_STATE,
         io={"nacl_decrypt": "11223344", "logger_debug": "ok", "infer_user_for_ssrc": 7,
             "dave_decrypt": "AABB", "monotonic": 500.0, "opus_decode": [[9, 8]],
             "logger_warning": "ok"}))
add(case("_on_packet", args={"data": PKT}, state=dict(PSTATE),
         io={"nacl_decrypt": "11223344", "logger_debug": "ok",
             "opus_decode": sc(raise_type="OSError", raise_msg="opus")}))
# padding bit set with invalid pad length
PAD = packet_hex(payload="00", nonce="01020304", b0="a0")
add(case("_on_packet", args={"data": PAD}, state=dict(PSTATE, _packet_debug_count=20),
         io={"nacl_decrypt": "11", "logger_warning": "ok"}))


# The "binding moved" case below deliberately overrides this with a different id.
for _c in CASES:
    if _c["func"] == "_process_voice_input" and "voice_text_channels" not in (_c["in"].get("args") or {}):
        _c["in"]["args"]["voice_text_channels"] = {1: 9}


# assertions
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
assert all(v >= 6 for v in counts.values()), {k: v for k, v in counts.items() if v < 6}
assert ALL_IO == set(SEAM_NAMES), ("seam coverage drift", set(SEAM_NAMES) - ALL_IO, ALL_IO - set(SEAM_NAMES))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT} ({len(ALL_FUNCS)} funcs, {len(ALL_IO)} seams)")
for f in sorted(counts):
    print(f"  {f}: {counts[f]}")
