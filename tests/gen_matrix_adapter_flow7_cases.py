#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow7.jsonl -- input cases for the
Matrix-adapter remainder flow oracle (src/gateway/platforms/matrix_adapter_flow7.c).
The oracle computes want from LIVE adapter.py at gate time; this script only
emits {func, in} inputs. >=6 handcrafted branch-coverage cases per function.
Every flow carries normal + branch variants; seam-driven funcs also carry a
scripted Exception-swallow, a BaseException-passthrough and a fail-closed
(fc=True) variant. Attribute carriers are {"__attr__": {...}}.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow7.jsonl"
CASES = []


def add(func, args, io=None, state=None, fc=False):
    inp = {"args": args}
    if io:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    case = {"func": func, "in": inp}
    if fc:
        case["fc"] = True
    CASES.append(case)


def rs(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def sc(v):
    return {"__script__": True, "ret": v}


def ad(attrs):
    return {"__attr__": attrs}


# ── _resolve_store_dir (seam: _get_hermes_dir) ──
add("_resolve_store_dir", {}, {"_get_hermes_dir": "/new/path"}, state={"_store_dir": "/old"})
add("_resolve_store_dir", {}, {"_get_hermes_dir": "matrix/store"}, state={})
add("_resolve_store_dir", {}, {"_get_hermes_dir": ""}, state={"_store_dir": None})
add("_resolve_store_dir", {}, {"_get_hermes_dir": "/"}, state={"_store_dir": "x"})
add("_resolve_store_dir", {}, {"_get_hermes_dir": rs("ValueError", "bad")}, state={})
add("_resolve_store_dir", {}, {"_get_hermes_dir": rs("SystemExit", "bye")}, state={})
add("_resolve_store_dir", {}, fc=True, state={})

# ── _get_matrix_recovery_key_output_target ──
add("_get_matrix_recovery_key_output_target", {}, {"_recovery_key_output_path": None})
add("_get_matrix_recovery_key_output_target", {},
    {"_recovery_key_output_path": "/k", "path.exists": True})
add("_get_matrix_recovery_key_output_target", {},
    {"_recovery_key_output_path": "/k", "path.exists": False, "path.parent.mkdir": None})
add("_get_matrix_recovery_key_output_target", {},
    {"_recovery_key_output_path": "/k2", "path.exists": False, "path.parent.mkdir": None})
add("_get_matrix_recovery_key_output_target", {},
    {"_recovery_key_output_path": "/k", "path.exists": False,
     "path.parent.mkdir": rs("ValueError", "denied")})
add("_get_matrix_recovery_key_output_target", {},
    {"_recovery_key_output_path": "/k", "path.exists": False,
     "path.parent.mkdir": rs("SystemExit", "bye")})
add("_get_matrix_recovery_key_output_target", {}, fc=True)

# ── _room_needs_encrypted_upload ──
CL_Y = ad({"crypto": "olm", "state_store": ad({"is_encrypted": True})})
CL_N = ad({"crypto": "olm", "state_store": ad({"is_encrypted": False})})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"}, {}, state={"_encryption": False, "_client": CL_Y})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"}, {}, state={"_encryption": True, "_client": None})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"}, {}, state={"_encryption": True, "_client": ad({"crypto": None})})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"},
    {"state_store.is_encrypted": True}, state={"_encryption": True, "_client": CL_Y})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"},
    {"state_store.is_encrypted": False}, state={"_encryption": True, "_client": CL_N})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"},
    {"state_store.is_encrypted": rs("ValueError", "bad")}, state={"_encryption": True, "_client": CL_Y})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"},
    {"state_store.is_encrypted": rs("SystemExit", "bye")}, state={"_encryption": True, "_client": CL_Y})
add("_room_needs_encrypted_upload", {"room_id": "!r:s"}, fc=True,
    state={"_encryption": True, "_client": CL_Y})

# ── _send_content_event ──
add("_send_content_event", {"room_id": "!r:s", "msg_content": {"body": "hi"}},
    {"client.send_message_event": "$abc"}, state={"_client": ad({})})
add("_send_content_event", {"room_id": "!r:s", "msg_content": {"body": "x"}},
    {"client.send_message_event": 123}, state={"_client": ad({})})
add("_send_content_event", {"room_id": "!r:s", "msg_content": {"body": "y"}},
    {"client.send_message_event": rs("ValueError", "boom")}, state={"_client": ad({})})
add("_send_content_event", {"room_id": "!r:s", "msg_content": {"body": "z"}},
    {"client.send_message_event": rs("SystemExit", "bye")}, state={"_client": ad({})})
add("_send_content_event", {"room_id": "!other:s", "msg_content": {"body": "q"}},
    {"client.send_message_event": "$xyz"}, state={"_client": ad({})})
add("_send_content_event", {"room_id": "!r:s", "msg_content": {}}, fc=True, state={"_client": ad({})})

# ── _handle_generated_matrix_recovery_key ──
add("_handle_generated_matrix_recovery_key", {"mxid": "@me:s", "recovery_key": "EsT ..."},
    {"_write_matrix_recovery_key_output_file": "/sec/key", "logger.warning": None})
add("_handle_generated_matrix_recovery_key", {"mxid": "@me:s", "recovery_key": "EsT ..."},
    {"_write_matrix_recovery_key_output_file": None, "logger.warning": None})
add("_handle_generated_matrix_recovery_key", {"mxid": "@me:s", "recovery_key": "EsT ..."},
    {"_write_matrix_recovery_key_output_file": rs("FileExistsError", "exists"), "logger.warning": None})
add("_handle_generated_matrix_recovery_key", {"mxid": "@me:s", "recovery_key": "EsT ..."},
    {"_write_matrix_recovery_key_output_file": rs("ValueError", "denied"), "logger.warning": None})
add("_handle_generated_matrix_recovery_key", {"mxid": "@me:s", "recovery_key": "EsT ..."},
    {"_write_matrix_recovery_key_output_file": rs("SystemExit", "bye")})
add("_handle_generated_matrix_recovery_key", {"mxid": "@other:s", "recovery_key": "k"},
    {"_write_matrix_recovery_key_output_file": "/sec/k2", "logger.warning": None})
add("_handle_generated_matrix_recovery_key", {"mxid": "@me:s", "recovery_key": "k"}, fc=True)

# ── _reverify_keys_after_upload ──
add("_reverify_keys_after_upload", {"client": ad({"device_id": None}), "local_ed25519": "KEY"},
    {"logger.warning": None}, state={"_device_id_unverified": False})
add("_reverify_keys_after_upload", {"client": ad({"device_id": "DEV"}), "local_ed25519": "KEY"},
    {"logger.warning": None}, state={"_device_id_unverified": True})
add("_reverify_keys_after_upload", {"client": ad({"device_id": "DEV"}), "local_ed25519": "KEY"},
    {"_query_own_device_keys": None}, state={"_device_id_unverified": False})
add("_reverify_keys_after_upload", {"client": ad({"device_id": "DEV"}), "local_ed25519": "KEY"},
    {"_query_own_device_keys": ad({}), "_extract_server_ed25519": "KEY"},
    state={"_device_id_unverified": False})
add("_reverify_keys_after_upload", {"client": ad({"device_id": "DEV"}), "local_ed25519": "KEY"},
    {"_query_own_device_keys": ad({}), "_extract_server_ed25519": "OTHER", "logger.error": None},
    state={"_device_id_unverified": False})
add("_reverify_keys_after_upload", {"client": ad({"device_id": "DEV"}), "local_ed25519": "KEY"},
    {"_query_own_device_keys": rs("ValueError", "bad"), "logger.error": None},
    state={"_device_id_unverified": False})
add("_reverify_keys_after_upload", {"client": ad({"device_id": "DEV"}), "local_ed25519": "KEY"},
    {"_query_own_device_keys": rs("SystemExit", "bye")}, state={"_device_id_unverified": False})
add("_reverify_keys_after_upload", {"client": ad({"device_id": "DEV"}), "local_ed25519": "KEY"},
    fc=True, state={"_device_id_unverified": False})

# ── _reset_crypto_store_if_device_changed ──
STORE = ad({"get_device_id": None, "delete": None})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": ""}, {})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": "DEV"},
    {"get_device_id": None})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": "DEV"},
    {"get_device_id": "DEV"})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": "DEV"},
    {"get_device_id": "OLD", "logger.warning": None, "delete": None})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": "DEV"},
    {"get_device_id": rs("ValueError", "bad"), "logger.warning": None})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": "DEV"},
    {"get_device_id": rs("SystemExit", "bye")})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": "DEV"},
    {"get_device_id": "OLD", "logger.warning": None, "delete": rs("ValueError", "bad")})
add("_reset_crypto_store_if_device_changed", {"crypto_store": STORE, "device_id": "DEV"}, fc=True)

# ── _protect_outbound_mention_regions (pure; no seams; plain-text domain) ──
add("_protect_outbound_mention_regions", {"text": ""})
add("_protect_outbound_mention_regions", {"text": "hello"})
add("_protect_outbound_mention_regions", {"text": "hello @a:s"})
add("_protect_outbound_mention_regions", {"text": "**bold** and _em_"})
add("_protect_outbound_mention_regions", {"text": "a\nb"})
add("_protect_outbound_mention_regions", {"text": "> quoted line"})
add("_protect_outbound_mention_regions", {})

# ── _schedule_invite_join ──
add("_schedule_invite_join", {"room_id": "", "is_direct": False, "inviter": ""}, {},
    state={"_joined_rooms": [], "_invite_join_tasks": {}})
add("_schedule_invite_join", {"room_id": "!r:s"}, {},
    state={"_joined_rooms": ["!r:s"], "_invite_join_tasks": {}})
add("_schedule_invite_join", {"room_id": "!r:s"},
    {"create_task": "task-1"}, state={"_joined_rooms": [], "_invite_join_tasks": {"!r:s": ad({"done": False})}})
add("_schedule_invite_join", {"room_id": "!r:s"},
    {"create_task": "task-2"}, state={"_joined_rooms": [], "_invite_join_tasks": {"!r:s": ad({"done": True})}})
add("_schedule_invite_join", {"room_id": "!r:s", "is_direct": True, "inviter": "@a:s"},
    {"create_task": "task-3"}, state={"_joined_rooms": [], "_invite_join_tasks": {}})
add("_schedule_invite_join", {"room_id": "!other:s"},
    {"create_task": "task-4"}, state={"_joined_rooms": [], "_invite_join_tasks": {}})
add("_schedule_invite_join", {"room_id": "!r:s"}, fc=True,
    state={"_joined_rooms": [], "_invite_join_tasks": {}})

# ── _repickle_crypto_sessions ──
DB = ad({"fetch": None, "execute": None})
add("_repickle_crypto_sessions",
    {"crypto_db": DB, "acct_id": "@me:s", "legacy_key": "LEG", "pickle_key": "NEW"},
    {"fetch": [[], [], []]}) 
add("_repickle_crypto_sessions",
    {"crypto_db": DB, "acct_id": "@me:s", "legacy_key": "LEG", "pickle_key": "NEW"},
    {"fetch": [[{"session_id": "s1", "session": None}], [], []]})
add("_repickle_crypto_sessions",
    {"crypto_db": DB, "acct_id": "@me:s", "legacy_key": "LEG", "pickle_key": "NEW"},
    {"fetch": [[{"session_id": "s1", "session": [1, 2]}], [], []], "from_pickle": None})
add("_repickle_crypto_sessions",
    {"crypto_db": DB, "acct_id": "@me:s", "legacy_key": "LEG", "pickle_key": "NEW"},
    {"fetch": [[{"session_id": "s1", "session": [1, 2]}], [], []],
     "from_pickle": sc([rs("ValueError", "cur"), None]), "session.pickle": [3, 4], "execute": None})
add("_repickle_crypto_sessions",
    {"crypto_db": DB, "acct_id": "@me:s", "legacy_key": "LEG", "pickle_key": "NEW"},
    {"fetch": [[{"session_id": "s1", "session": [1, 2]}], [], []],
     "from_pickle": sc([rs("ValueError", "cur"), rs("ValueError", "leg")]), "logger.warning": None})
add("_repickle_crypto_sessions",
    {"crypto_db": DB, "acct_id": "@me:s", "legacy_key": "LEG", "pickle_key": "NEW"},
    {"fetch": rs("SystemExit", "bye")})
add("_repickle_crypto_sessions",
    {"crypto_db": DB, "acct_id": "@me:s", "legacy_key": "LEG", "pickle_key": "NEW"}, fc=True)

# ── _resolve_room_identity ──
BASE_RI = {"_room_identities": {}, "_room_identity_cached_at": {},
           "_room_identity_ttl_seconds": 0, "_room_identity_cache_max": 10, "_dm_rooms": {}}
add("_resolve_room_identity", {"room_id": "!r:s"},
    {"time.monotonic": [10.0, 11.0],
     "_get_room_state_value": ["Room", "Topic", "alias"],
     "_get_room_member_count": 2}, state=dict(BASE_RI))
add("_resolve_room_identity", {"room_id": "!r:s"},
    {"time.monotonic": [10.0, 11.0],
     "_get_room_state_value": [None, None, None],
     "_get_room_member_count": 5}, state=dict(BASE_RI))
add("_resolve_room_identity", {"room_id": "!r:s:other"},
    {"time.monotonic": [10.0, 11.0],
     "_get_room_state_value": ["", "", "alias"],
     "_get_room_member_count": None}, state=dict(BASE_RI))
add("_resolve_room_identity", {"room_id": "!r:s", "force_refresh": True},
    {"time.monotonic": [10.0, 11.0],
     "_get_room_state_value": ["Name", "", None],
     "_get_room_member_count": 1, "_invalidate_room_identities": None},
    state=dict(BASE_RI, _room_identities={"!old:s": "OLDID"},
               _room_identity_cached_at={"!old:s": 1.0}, _room_identity_cache_max=1))
add("_resolve_room_identity", {"room_id": "!r:s"}, {"time.monotonic": 101.0}, state=dict(BASE_RI,
    _room_identities={"!r:s": "CACHED"}, _room_identity_cached_at={"!r:s": 100.0},
    _room_identity_ttl_seconds=60))
add("_resolve_room_identity", {"room_id": "!r:s"},
    {"_get_room_state_value": rs("SystemExit", "bye")}, state=dict(BASE_RI))
add("_resolve_room_identity", {"room_id": "!r:s"}, fc=True, state=dict(BASE_RI))

# ── _matrix_voice_metadata_for_file ──
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"},
    {"shutil.which": [None, None]})
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"},
    {"shutil.which": ["/bin/ffprobe", None],
     "_run_media_tool": ad({"returncode": 1, "stdout": ""})})
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"},
    {"shutil.which": ["/bin/ffprobe", None],
     "_run_media_tool": ad({"returncode": 0, "stdout": "1.5"})})
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"},
    {"shutil.which": ["/bin/ffprobe", None],
     "_run_media_tool": ad({"returncode": 0, "stdout": ""})})
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"},
    {"shutil.which": ["/bin/ffprobe", None],
     "_run_media_tool": ad({"returncode": 0, "stdout": "abc"}), "logger.debug": None})
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"},
    {"shutil.which": ["/bin/ffprobe", None],
     "_run_media_tool": rs("ValueError", "bad"), "logger.debug": None})
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"},
    {"shutil.which": ["/bin/ffprobe", None], "_run_media_tool": rs("SystemExit", "bye")})
add("_matrix_voice_metadata_for_file", {"path": "/v/clip.ogg"}, fc=True)

# ── _on_room_message ──
EV = ad({"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "content": {"msgtype": "m.text", "body": "hi"}})
ON_STATE = {"_startup_ts": 1000, "_process_notices": False, "_ignored_user_patterns": []}
add("_on_room_message", {"event": EV},
    {"logger.debug": None, "_is_self_sender": True}, state=ON_STATE)
add("_on_room_message", {"event": EV},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": True}, state=ON_STATE)
add("_on_room_message", {"event": EV},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": False,
     "_is_allowed_matrix_room_event": False, "logger.info": None}, state=ON_STATE)
add("_on_room_message", {"event": EV},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": False,
     "_is_allowed_matrix_room_event": True, "_is_duplicate_event": True}, state=ON_STATE)
add("_on_room_message", {"event": EV},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": False,
     "_is_allowed_matrix_room_event": True, "_is_duplicate_event": False,
     "_matrix_event_timestamp_seconds": 10, "_note_late_grace_drop": None}, state=ON_STATE)
add("_on_room_message", {"event": ad({"room_id": "!r:s", "sender": "@u:s", "event_id": "$e"})},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": False,
     "_is_allowed_matrix_room_event": True, "_is_duplicate_event": False,
     "_matrix_event_timestamp_seconds": 999}, state=ON_STATE)
add("_on_room_message", {"event": ad({"room_id": "!r:s", "sender": "@u:s", "event_id": "$e",
    "content": {"msgtype": "m.replace"}})},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": False,
     "_is_allowed_matrix_room_event": True, "_is_duplicate_event": False,
     "_matrix_event_timestamp_seconds": 999}, state=ON_STATE)
add("_on_room_message", {"event": ad({"room_id": "!r:s", "sender": "@u:s", "event_id": "$e",
    "content": {"msgtype": "m.image", "body": "pic.jpg"}})},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": False,
     "_is_allowed_matrix_room_event": True, "_is_duplicate_event": False,
     "_matrix_event_timestamp_seconds": 999, "_handle_media_message": None}, state=ON_STATE)
add("_on_room_message", {"event": EV},
    {"logger.debug": None, "_is_self_sender": False, "_is_system_or_bridge_sender": False,
     "_is_allowed_matrix_room_event": True, "_is_duplicate_event": False,
     "_matrix_event_timestamp_seconds": 999, "_handle_text_message": None}, state=ON_STATE)
add("_on_room_message", {"event": EV},
    {"logger.debug": rs("SystemExit", "bye")}, state=ON_STATE)
add("_on_room_message", {"event": EV}, fc=True, state=ON_STATE)

# ── _resolve_message_context ──
MC_STATE = {
    "_allowed_rooms": [], "_free_rooms": [], "_threads": {}, "_require_mention": False,
    "_thread_require_mention": False, "_dm_mention_threads": False, "_dm_auto_thread": False,
    "_matrix_session_scope": "room", "_auto_thread": False,
    "_parked_voices": {}, "_ignored": True,
}
ID = ad({"display_name": "Room", "room_topic": "Topic", "server_name": "s"})
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
     "source_content": {"msgtype": "m.text"}, "relates_to": {}},
    {"_resolve_room_identity": ID, "_is_dm_room": True, "_thread_root": None,
     "_content_mentions_bot": False, "_get_display_name": "User", "build_source": "SRC",
     "_background_read_receipt": None}, state=dict(MC_STATE))
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
     "source_content": {"msgtype": "m.text"}, "relates_to": {}},
    {"_resolve_room_identity": ID, "_is_dm_room": False, "_thread_root": None,
     "_content_mentions_bot": True, "logger.debug": None},
    state=dict(MC_STATE, _allowed_rooms=["!other:s"]))
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
     "source_content": {"msgtype": "m.text"}, "relates_to": {}},
    {"_resolve_room_identity": ID, "_is_dm_room": False, "_thread_root": None,
     "_content_mentions_bot": False, "logger.debug": None},
    state=dict(MC_STATE, _require_mention=True))
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "@bot hi",
     "source_content": {"msgtype": "m.text"}, "relates_to": {}},
    {"_resolve_room_identity": ID, "_is_dm_room": False, "_thread_root": None,
     "_content_mentions_bot": True, "_strip_mention": "hi", "_get_display_name": "User",
     "build_source": "SRC", "_background_read_receipt": None},
    state=dict(MC_STATE, _require_mention=True))
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "reply",
     "source_content": {"msgtype": "m.text"}, "relates_to": {"m.in_reply_to": {"event_id": "$p"}}},
    {"_resolve_room_identity": ID, "_is_dm_room": False, "_thread_root": None,
     "_content_mentions_bot": True, "_split_reply_fallback": sc(["> pill", " @bot reply"]),
     "_strip_mention": " reply", "_get_display_name": "User", "build_source": "SRC",
     "_background_read_receipt": None}, state=dict(MC_STATE, _require_mention=True))
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
     "source_content": {"msgtype": "m.notice"}, "relates_to": {}},
    {"_resolve_room_identity": ID, "_is_dm_room": True, "_thread_root": None,
     "_content_mentions_bot": False, "_get_display_name": "User", "build_source": "SRC",
     "_background_read_receipt": None, "mark_async": None}, state=dict(MC_STATE, _dm_auto_thread=True))
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
     "source_content": {"msgtype": "m.text"}, "relates_to": {}},
    {"_resolve_room_identity": rs("SystemExit", "bye")}, state=dict(MC_STATE))
add("_resolve_message_context",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
     "source_content": {"msgtype": "m.text"}, "relates_to": {}}, fc=True, state=dict(MC_STATE))


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")


if __name__ == "__main__":
    main()
