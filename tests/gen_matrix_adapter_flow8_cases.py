#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow8.jsonl -- input cases for the
Matrix-adapter remainder flow oracle (src/gateway/platforms/matrix_adapter_flow8.c).

The oracle computes want from LIVE adapter.py at gate time; this script only
emits {func, in} inputs. >=6 handcrafted branch-coverage cases per function.
Every flow carries normal + branch variants; seam-driven funcs also carry a
scripted Exception-swallow, a BaseException-passthrough and a fail-closed
(fc=True) variant. Attribute carriers are {"__attr__": {...}}.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow8.jsonl"
CASES = []

# EXACTLY the C ma8_call/ma8_log0 seam names and the oracle SEAM_NAMES.
SEAM_NAMES = [
    "_abort_connect", "_build_inbound_event", "_classify_inbound_media",
    "_download_and_cache_media", "_media_too_large",
    "_mxc_to_http", "_recovery_key_output_path", "_resolve_message_context",
    "_send_reaction", "_upload_and_send", "_voice_may_park", "ClientSession",
    "CryptoStateStore", "Database.create", "OlmMachine", "PgCryptoStore",
    "check_e2ee_deps", "client.login", "client.query_keys",
    "client.send_message_event", "client.whoami", "content_bytes is covered",
    "create_task", "db.start", "emit_media_warning", "e2ee_setup_failed",
    "file.write", "get_secret", "guess_type", "handle_message",
    "http.get", "is_safe_url", "latex_to_tokens", "legacy_pickle.exists",
    "legacy_pickle.unlink", "logger.debug", "logger.error", "logger.info",
    "logger.warning", "make_prompt", "markdown", "mark_async",
    "migrate_legacy", "olm.load", "olm.share_keys", "os.close", "os.open",
    "parked_voices.begin", "parked_voices.release", "path.exists",
    "path.parent.mkdir", "path.read_bytes", "path.stat", "proxy_kwargs",
    "put", "raise_for_status", "read_body_capped", "reset_crypto_store",
    "resp.json", "resp.text", "sanitize_html", "send", "send_error",
    "send_reaction_prompt", "standalone_payloads", "store.open",
    "store.put_device_id", "store_dir.mkdir", "t", "time.monotonic",
    "truncate_message", "verify_cross_signing", "verify_device_keys",
    "voice_metadata", "content_bytes",
]
# Remove the documentation placeholder entry; keep the literal set clean.
SEAM_NAMES.remove("content_bytes is covered")
SEAM_NAMES = sorted(set(SEAM_NAMES))


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


def cov_io(**over):
    """An io map covering EVERY seam name with a benign value, plus overrides."""
    d = {k: None for k in SEAM_NAMES}
    d.update(over)
    return d


# ══ 1. _EA_HEADER (seam: t) ══
add("_EA_HEADER", {}, {"t": "Exec approval"})
add("_EA_HEADER", {}, {"t": ""})
add("_EA_HEADER", {}, {"t": "A/B & <x>"})
add("_EA_HEADER", {}, {"t": rs("ValueError", "bad")})
add("_EA_HEADER", {}, {"t": rs("SystemExit", "bye")})
add("_EA_HEADER", {}, fc=True)

# ══ 2. _send_room_message (seam: client.send_message_event) ══
add("_send_room_message", {"chat_id": "!r:s", "msg_content": {"body": "hi"}},
    {"client.send_message_event": "$abc"}, state={"_client": ad({})})
add("_send_room_message", {"chat_id": "!r:s", "msg_content": {"body": "x"}},
    {"client.send_message_event": 123}, state={"_client": ad({})})
add("_send_room_message", {"chat_id": "!r:s", "msg_content": {}},
    {"client.send_message_event": rs("ValueError", "boom")}, state={"_client": ad({})})
add("_send_room_message", {"chat_id": "!r:s", "msg_content": {}},
    {"client.send_message_event": rs("SystemExit", "bye")}, state={"_client": ad({})})
add("_send_room_message", {"chat_id": "!other:s", "msg_content": {"body": "q"}},
    {"client.send_message_event": "$xyz"}, state={"_client": ad({})})
add("_send_room_message", {"chat_id": "!r:s", "msg_content": {}}, fc=True,
    state={"_client": ad({})})

# ══ 3. _connect_authenticate ══
AUTH_S = {"_access_token": "tok", "_user_id": "u1", "_device_id": "d1",
          "_password": "", "_proxy_url": "", "_homeserver": "https://s"}
add("_connect_authenticate", {"client": ad({}), "api": ad({})},
    cov_io(**{"client.whoami": ad({"user_id": "@new:s", "device_id": "DEV2"}),
              "logger.info": None}),
    state=dict(AUTH_S))
add("_connect_authenticate", {"client": ad({}), "api": ad({})},
    cov_io(**{"client.whoami": ad({"user_id": "", "device_id": ""}),
              "client.query_keys": ad({"device_keys": {"mxid": {"D1": {}}}}),
              "logger.info": None}),
    state=dict(AUTH_S, _device_id=""))
add("_connect_authenticate", {"client": ad({}), "api": ad({})},
    cov_io(**{"client.whoami": rs("ValueError", "bad"), "logger.error": None,
              "_abort_connect": ad({"__raise__": "Abort", "message": "a"})}),
    state=dict(AUTH_S))
add("_connect_authenticate", {"client": ad({}), "api": ad({})},
    cov_io(**{"client.whoami": rs("SystemExit", "bye")}), state=dict(AUTH_S))
add("_connect_authenticate", {"client": ad({}), "api": ad({})},
    cov_io(**{"client.login": ad({"device_id": "LD"}), "logger.info": None}),
    state=dict(AUTH_S, _access_token="", _password="pw", _user_id="u1"))
add("_connect_authenticate", {"client": ad({}), "api": ad({})},
    cov_io(**{"logger.error": None, "_abort_connect": ad({"ok": False})}),
    state=dict(AUTH_S, _access_token="", _password="", _user_id=""))
add("_connect_authenticate", {"client": ad({}), "api": ad({})}, fc=True,
    state=dict(AUTH_S))

# ══ 4. _connect_setup_e2ee ══
E2E_S = {"_e2ee_mode": "optional", "_encryption": True, "_user_id": "@me:s",
         "_device_id": "DEV", "_joined_rooms": [], "_crypto_db_path": "/db.sqlite"}
CL = ad({"device_id": "DEV"})
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    cov_io(**{"check_e2ee_deps": False, "logger.warning": None}), state=dict(E2E_S))
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    cov_io(**{"check_e2ee_deps": False, "logger.error": None,
              "_abort_connect": ad({"ok": False})}),
    state=dict(E2E_S, _e2ee_mode="required"))
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    cov_io(**{"check_e2ee_deps": True}), state=dict(E2E_S, _encryption=False))
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    cov_io(**{"check_e2ee_deps": True, "store_dir.mkdir": rs("ValueError", "bad"),
              "e2ee_setup_failed": ad({"ok": False})}), state=dict(E2E_S))
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    cov_io(**{"check_e2ee_deps": True, "store_dir.mkdir": rs("SystemExit", "bye")}),
    state=dict(E2E_S))
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    cov_io(**{
        "check_e2ee_deps": True, "store_dir.mkdir": None,
        "legacy_pickle.exists": False, "Database.create": ad({}), "db.start": None,
        "PgCryptoStore": ad({}), "store.open": None, "reset_crypto_store": False,
        "store.put_device_id": None, "migrate_legacy": True,
        "CryptoStateStore": ad({}), "OlmMachine": ad({}), "olm.load": None,
        "verify_device_keys": False, "_abort_connect": ad({"ok": False})}),
    state=dict(E2E_S))
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    cov_io(**{"check_e2ee_deps": True, "store_dir.mkdir": None,
              "legacy_pickle.exists": False, "Database.create": ad({}),
              "db.start": None, "PgCryptoStore": ad({}), "store.open": None,
              "reset_crypto_store": False, "store.put_device_id": None,
              "migrate_legacy": True, "CryptoStateStore": ad({}),
              "OlmMachine": ad({}), "olm.load": None, "verify_device_keys": True,
              "olm.share_keys": None, "verify_cross_signing": None,
              "logger.info": None}), state=dict(E2E_S))
add("_connect_setup_e2ee", {"client": CL, "api": ad({}), "state_store": ad({})},
    fc=True, state=dict(E2E_S))

# ══ 5. _download_external_media_with_cap ══
DL_S = {"_proxy_url": "", "_max_media_bytes": 1024}
add("_download_external_media_with_cap", {"url": "https://s/a/img.png?x=1"},
    cov_io(**{"is_safe_url": True, "proxy_kwargs": sc([{}, {}]),
              "ClientSession": ad({}), "http.get": ad({"status": 200, "location": "",
                                                        "content_type": "image/png"}),
              "raise_for_status": None, "read_body_capped": sc([1, 2, 3])}),
    state=dict(DL_S))
add("_download_external_media_with_cap", {"url": "https://s/a/img.png"},
    cov_io(**{"is_safe_url": False}), state=dict(DL_S))
add("_download_external_media_with_cap", {"url": "https://s/img.png"},
    cov_io(**{"is_safe_url": [True, True], "proxy_kwargs": sc([{}, {}]),
              "ClientSession": ad({}),
              "http.get": [ad({"status": 302, "location": "/next.png",
                               "content_type": ""}),
                           ad({"status": 200, "location": "", "content_type": "image/jpeg"})],
              "raise_for_status": None, "read_body_capped": sc([9])}), state=dict(DL_S))
add("_download_external_media_with_cap", {"url": "https://s/img.png"},
    cov_io(**{"is_safe_url": True, "proxy_kwargs": sc([{}, {}]),
              "ClientSession": ad({}),
              "http.get": ad({"status": 200, "location": "", "content_type": "text/html"}),
              "raise_for_status": None, "read_body_capped": sc([1])}), state=dict(DL_S))
add("_download_external_media_with_cap", {"url": "https://s/img.png"},
    cov_io(**{"is_safe_url": True, "proxy_kwargs": sc([{}, {}]),
              "ClientSession": ad({}),
              "http.get": ad({"status": 200, "location": "", "content_type": "image/png"}),
              "raise_for_status": rs("ValueError", "404")}), state=dict(DL_S))
add("_download_external_media_with_cap", {"url": "https://s/img.png"},
    cov_io(**{"is_safe_url": rs("SystemExit", "bye")}), state=dict(DL_S))
add("_download_external_media_with_cap", {"url": "https://s/img.png"}, fc=True,
    state=dict(DL_S))

# ══ 6. _handle_media_message ══
HM_S = {"_max_media_bytes": 1000, "_parked_voices": ad({}), "_threads": ad({})}
add("_handle_media_message",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1.0,
     "source_content": {"msgtype": "m.image", "body": "pic.jpg", "url": "mxc://s/abc",
                        "info": {"mimetype": "image/jpeg", "size": 10}},
     "relates_to": {}, "msgtype": "m.image", "mention_claimed": False},
    cov_io(**{"_classify_inbound_media": sc(["image", "image/jpeg", False]),
              "_voice_may_park": False,
              "_resolve_message_context": ad({"ctx": 1}),
              "_download_and_cache_media": "/cache/abc",
              "_build_inbound_event": ad({"msg": 1}), "handle_message": None}),
    state=dict(HM_S))
add("_handle_media_message",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1.0,
     "source_content": {"msgtype": "m.image", "body": "x", "url": "https://evil/x"},
     "relates_to": {}, "msgtype": "m.image"},
    cov_io(**{"logger.warning": None}), state=dict(HM_S))
add("_handle_media_message",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1.0,
     "source_content": {"msgtype": "m.file", "body": "x", "info": {"size": 5000}},
     "relates_to": {}, "msgtype": "m.file"},
    cov_io(**{"logger.warning": None}), state=dict(HM_S, _max_media_bytes=100))
add("_handle_media_message",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1.0,
     "source_content": {"msgtype": "m.audio", "body": "v", "url": "mxc://s/v",
                        "file": {"url": "mxc://s/v"}},
     "relates_to": {}, "msgtype": "m.audio"},
    cov_io(**{"_classify_inbound_media": sc(["audio", "audio/ogg", True]),
              "_voice_may_park": True, "parked_voices.begin": "G",
              "_resolve_message_context": None, "parked_voices.release": None}),
    state=dict(HM_S))
add("_handle_media_message",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1.0,
     "source_content": {"msgtype": "m.image", "body": "x", "url": "mxc://s/abc"},
     "relates_to": {}, "msgtype": "m.image"},
    cov_io(**{"_classify_inbound_media": sc(["image", "image/png", False]),
              "_voice_may_park": False, "_resolve_message_context": ad({"ctx": 1}),
              "_download_and_cache_media": rs("ValueError", "big"),
              "logger.warning": None, "_mxc_to_http": "http://s/abc",
              "_build_inbound_event": None}), state=dict(HM_S))
add("_handle_media_message",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1.0,
     "source_content": {"msgtype": "m.image", "body": "x", "url": "mxc://s/abc"},
     "relates_to": {}, "msgtype": "m.image"},
    cov_io(**{"_classify_inbound_media": rs("SystemExit", "bye")}), state=dict(HM_S))
add("_handle_media_message",
    {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1.0,
     "source_content": {"msgtype": "m.image", "body": "x", "url": "mxc://s/abc"},
     "relates_to": {}, "msgtype": "m.image"}, fc=True, state=dict(HM_S))

# ══ 7. _schedule_reaction_redaction ══
add("_schedule_reaction_redaction", {"room_id": "!r:s", "reaction_event_id": "$r", "reason": "x"},
    cov_io(**{"create_task": "TASK"}), state={"_reaction_redaction_delay_seconds": 0.01})
add("_schedule_reaction_redaction", {"room_id": "!r:s", "reaction_event_id": "$r"},
    cov_io(**{"create_task": "TASK"}), state={"_reaction_redaction_delay_seconds": 0})
add("_schedule_reaction_redaction", {"room_id": "!o:s", "reaction_event_id": "$z", "reason": ""},
    cov_io(**{"create_task": "T2"}), state={"_reaction_redaction_delay_seconds": 2.5})
add("_schedule_reaction_redaction", {"room_id": "!r:s", "reaction_event_id": "$r"},
    cov_io(**{"create_task": rs("ValueError", "bad")}), state={})
add("_schedule_reaction_redaction", {"room_id": "!r:s", "reaction_event_id": "$r"},
    cov_io(**{"create_task": rs("SystemExit", "bye")}), state={})
add("_schedule_reaction_redaction", {"room_id": "!r:s", "reaction_event_id": "$r"}, fc=True, state={})

# ══ 8. _send_exec_approval_prompt ══
EX_S = {"_client": ad({}), "_approval_prompt_by_session": {},
        "_EA_TYPED_HINT_KEYS": {"once": "hint.once", "always": "hint.always", "session": "hint.session"},
        "_EA_LEGEND_KEYS": {"once": "legend.once", "always": "legend.always", "session": "legend.session"},
        "_EA_REACTIONS": {"once": "OK", "always": "LOCK", "session": "CLOCK"}}
add("_send_exec_approval_prompt",
    {"prompt": ad({"choices": ["once"], "text": "Approve?", "smart_denied": False,
                   "metadata": {"requester_user_id": "@u:s"}, "session_key": "sk",
                   "chat_id": "!r:s", "request_id": "req"})},
    cov_io(**{"t": "TXT", "send_reaction_prompt": ad({"success": True, "message_id": "$m"})}),
    state=dict(EX_S))
add("_send_exec_approval_prompt",
    {"prompt": ad({"choices": ["always", "session"], "text": "T", "smart_denied": False,
                   "metadata": None, "session_key": "sk2", "chat_id": "!r:s", "request_id": "r2"})},
    cov_io(**{"t": "TXT", "send_reaction_prompt": ad({"success": True, "message_id": "$m"})}),
    state=dict(EX_S))
add("_send_exec_approval_prompt",
    {"prompt": ad({"choices": ["once"], "text": "T", "smart_denied": True,
                   "metadata": {}, "session_key": "sk3", "chat_id": "!r:s", "request_id": "r3"})},
    cov_io(**{"t": "TXT", "send_reaction_prompt": ad({"success": True, "message_id": "$m"})}),
    state=dict(EX_S))
add("_send_exec_approval_prompt",
    {"prompt": ad({"choices": [], "text": "T", "smart_denied": False,
                   "metadata": {}, "session_key": "sk4", "chat_id": "!r:s", "request_id": "r4"})},
    cov_io(**{"t": "TXT", "send_reaction_prompt": ad({"success": False, "error": "no"})}),
    state=dict(EX_S))
add("_send_exec_approval_prompt",
    {"prompt": ad({"choices": ["once"], "text": "T", "smart_denied": False,
                   "metadata": {}, "session_key": "sk5", "chat_id": "!r:s", "request_id": "r5"})},
    cov_io(**{"t": rs("SystemExit", "bye")}), state=dict(EX_S))
add("_send_exec_approval_prompt", {"prompt": ad({"choices": ["once"], "text": "T", "smart_denied": False})},
    fc=True, state=dict(EX_S))

# ══ 9. _send_picker ══
add("_send_picker", {"chat_id": "!r:s", "lines": ["A", "B"], "choices": {"y": 1},
                     "session_key": "sk", "on_selected": None, "metadata": {"requester_user_id": "@u:s"},
                     "registry": {}, "label": "model"},
    cov_io(**{"send_reaction_prompt": ad({"success": True, "message_id": "$m"})}))
add("_send_picker", {"chat_id": "!r:s", "lines": [], "choices": {}, "session_key": "sk",
                     "on_selected": None, "metadata": None, "registry": {}, "label": "model"},
    cov_io(**{"send_reaction_prompt": ad({"success": True, "message_id": "$m"})}))
add("_send_picker", {"chat_id": "!r:s", "lines": ["one"], "choices": {"a": 1, "b": 2},
                     "session_key": "sk", "on_selected": None, "metadata": {},
                     "registry": {}, "label": "choice"},
    cov_io(**{"send_reaction_prompt": ad({"success": False, "error": "x"})}))
add("_send_picker", {"chat_id": "!r:s", "lines": ["A"], "choices": {}, "session_key": "sk",
                     "on_selected": None, "metadata": {}, "registry": {}, "label": "m"},
    cov_io(**{"send_reaction_prompt": rs("ValueError", "bad")}))
add("_send_picker", {"chat_id": "!r:s", "lines": ["A"], "choices": {}, "session_key": "sk",
                     "on_selected": None, "metadata": {}, "registry": {}, "label": "m"},
    cov_io(**{"send_reaction_prompt": rs("SystemExit", "bye")}))
add("_send_picker", {"chat_id": "!r:s", "lines": ["A"], "choices": {}, "session_key": "sk",
                     "on_selected": None, "metadata": {}, "registry": {}, "label": "m"}, fc=True)

# ══ 10. _send_local_file ══
LF_S = {"_max_media_bytes": 1000, "_client": ad({})}
add("_send_local_file", {"room_id": "!r:s", "file_path": "/media/a.jpg", "msgtype": "m.image"},
    cov_io(**{"path.exists": True, "path.stat": ad({"st_size": 100}),
              "path.read_bytes": [1, 2], "guess_type": sc(["image/jpeg", None]),
              "_upload_and_send": ad({"success": True, "message_id": "$m"})}),
    state=dict(LF_S))
add("_send_local_file", {"room_id": "!r:s", "file_path": "/media/a.jpg", "msgtype": "m.image",
                         "caption": "cap", "reply_to": "$r", "file_name": "x.png",
                         "metadata": {"k": 1}},
    cov_io(**{"path.exists": True, "path.stat": ad({"st_size": 100}),
              "path.read_bytes": [3], "guess_type": sc(["image/png", None]),
              "_upload_and_send": ad({"success": True, "message_id": "$m2"})}),
    state=dict(LF_S))
add("_send_local_file", {"room_id": "!r:s", "file_path": "/nope", "msgtype": "m.image"},
    cov_io(**{"path.exists": False, "t": "failed",
              "emit_media_warning": ad({"success": True, "message_id": "$w"})}),
    state=dict(LF_S))
add("_send_local_file", {"room_id": "!r:s", "file_path": "/big.bin", "msgtype": "m.file"},
    cov_io(**{"path.exists": True, "path.stat": ad({"st_size": 5000}),
              "_media_too_large": ad({"success": False, "error": "too big"})}),
    state=dict(LF_S))
add("_send_local_file", {"room_id": "!r:s", "file_path": "/v.ogg", "msgtype": "m.audio",
                         "is_voice": True},
    cov_io(**{"path.exists": True, "path.stat": ad({"st_size": 50}),
              "voice_metadata": {"duration": 1}, "path.read_bytes": [9],
              "guess_type": sc(["audio/ogg", None]),
              "_upload_and_send": ad({"success": True, "message_id": "$v"})}),
    state=dict(LF_S))
add("_send_local_file", {"room_id": "!r:s", "file_path": "/x", "msgtype": "m.file"},
    cov_io(**{"path.exists": rs("SystemExit", "bye")}), state=dict(LF_S))
add("_send_local_file", {"room_id": "!r:s", "file_path": "/x", "msgtype": "m.file"},
    fc=True, state=dict(LF_S))

# ══ 11. _send_reaction_prompt ══
add("_send_reaction_prompt", {"chat_id": "!r:s", "text": "hello",
                              "metadata": {"requester_user_id": "@u:s"},
                              "registry": {}, "emojis": ["✅"], "label": "approval"},
    cov_io(**{"send": ad({"success": True, "message_id": "$m"}),
              "time.monotonic": 100.0,
              "make_prompt": ad({"bot_reaction_events": {}}),
              "_send_reaction": "$r"}))
add("_send_reaction_prompt", {"chat_id": "!r:s", "text": "hello", "metadata": None,
                              "registry": {}, "emojis": ["✅", "❌"], "label": "approval"},
    cov_io(**{"send": ad({"success": True, "message_id": "$m"}),
              "time.monotonic": 1.0,
              "make_prompt": ad({"bot_reaction_events": {}}),
              "_send_reaction": ["$r1", "$r2"]}))
add("_send_reaction_prompt", {"chat_id": "!r:s", "text": "hello", "metadata": {},
                              "registry": {}, "emojis": [], "label": "approval"},
    cov_io(**{"send": ad({"success": False, "error": "no"})}))
add("_send_reaction_prompt", {"chat_id": "!r:s", "text": "hello", "metadata": {},
                              "registry": {}, "emojis": ["✅"], "label": "approval"},
    cov_io(**{"send": ad({"success": True, "message_id": "$m"}),
              "time.monotonic": 1.0, "make_prompt": ad({"bot_reaction_events": {}}),
              "_send_reaction": rs("ValueError", "bad"), "logger.debug": None}))
add("_send_reaction_prompt", {"chat_id": "!r:s", "text": "hello", "metadata": {},
                              "registry": {}, "emojis": [], "label": "approval"},
    cov_io(**{"send": rs("SystemExit", "bye")}))
add("_send_reaction_prompt", {"chat_id": "!r:s", "text": "hello", "metadata": {},
                              "registry": {}, "emojis": [], "label": "approval"}, fc=True)

# ══ 12. _markdown_to_html_fallback (pure; no seams) ══
add("_markdown_to_html_fallback", {"text": ""})
add("_markdown_to_html_fallback", {"text": "hello"})
add("_markdown_to_html_fallback", {"text": "# Title"})
add("_markdown_to_html_fallback", {"text": "**bold** and _em_"})
add("_markdown_to_html_fallback", {"text": "- a\n- b"})
add("_markdown_to_html_fallback", {"text": "> quoted"})
add("_markdown_to_html_fallback", {"text": "a\nb"})
add("_markdown_to_html_fallback", {"text": "***"})

# ══ 13. _standalone_payloads ══
add("_standalone_payloads", {"message": "hello"},
    cov_io(**{"truncate_message": sc(["hello"]),
              "latex_to_tokens": sc(["hello", {}]), "markdown": "<p>hello</p>",
              "sanitize_html": "<p>hello</p>", "content_bytes": 20}))
add("_standalone_payloads", {"message": "a" * 50},
    cov_io(**{"truncate_message": sc(["a" * 25, "a" * 25]),
              "latex_to_tokens": sc(["x", {}]), "markdown": "<p>x</p>",
              "sanitize_html": "<p>x</p>", "content_bytes": 40}))
add("_standalone_payloads", {"message": "# H"},
    cov_io(**{"truncate_message": sc(["# H"]), "latex_to_tokens": sc(["# H", {}]),
              "markdown": "<h1>H</h1>", "sanitize_html": "<strong>H</strong>",
              "content_bytes": 30}))
add("_standalone_payloads", {"message": "big"},
    cov_io(**{"truncate_message": sc(["big"]), "latex_to_tokens": sc(["big", {}]),
              "markdown": "<p>big</p>", "sanitize_html": "<p>big</p>",
              "content_bytes": 100000}))
add("_standalone_payloads", {"message": "x"},
    cov_io(**{"truncate_message": rs("SystemExit", "bye")}))
add("_standalone_payloads", {"message": "x"}, fc=True)

# ══ 14. _standalone_send ══
SS = {}
add("_standalone_send", {"pconfig": ad({"extra": {"homeserver": "https://s/"}, "token": "tok"}),
                         "chat_id": "!r:s", "message": "hi"},
    cov_io(**{"get_secret": "", "ClientSession": ad({}),
              "standalone_payloads": sc([{"msgtype": "m.text", "body": "hi"}]),
              "put": ad({"status": 200}), "resp.json": {"event_id": "$e"}}))
add("_standalone_send", {"pconfig": ad({"extra": {"homeserver": "https://s"}, "token": "t"}),
                         "chat_id": "!r:s", "message": "a\nb"},
    cov_io(**{"get_secret": "", "ClientSession": ad({}),
              "standalone_payloads": sc([{"msgtype": "m.text", "body": "a"},
                                      {"msgtype": "m.text", "body": "b"}]),
              "put": ad({"status": 201}), "resp.json": {"event_id": "$e2"}}))
add("_standalone_send", {"pconfig": ad({"extra": {}, "token": ""}),
                         "chat_id": "!r:s", "message": "hi"},
    cov_io(**{"get_secret": "", "send_error": {"success": False, "error": "not configured"}}))
add("_standalone_send", {"pconfig": ad({"extra": {"homeserver": "https://s"}, "token": "t"}),
                         "chat_id": "!r:s", "message": "hi"},
    cov_io(**{"get_secret": "", "ClientSession": ad({}),
              "standalone_payloads": sc([{"msgtype": "m.text", "body": "hi"}]),
              "put": ad({"status": 429}), "resp.text": "rate limited",
              "send_error": {"success": False, "error": "429"}}))
add("_standalone_send", {"pconfig": ad({"extra": {"homeserver": "https://s"}, "token": "t"}),
                         "chat_id": "!r:s", "message": "hi"},
    cov_io(**{"get_secret": "", "ClientSession": ad({}),
              "standalone_payloads": sc([{"msgtype": "m.text", "body": "hi"}]),
              "put": rs("TimeoutError", "late"), "send_error": {"success": False}}))
add("_standalone_send", {"pconfig": ad({"extra": {}, "token": ""}),
                         "chat_id": "!r:s", "message": "hi"},
    cov_io(**{"get_secret": rs("SystemExit", "bye")}))
add("_standalone_send", {"pconfig": ad({}), "chat_id": "!r:s", "message": "hi"}, fc=True)

# ══ 15. _write_matrix_recovery_key_output_file (extra) ══
add("_write_matrix_recovery_key_output_file", {"recovery_key": "EsT key"},
    cov_io(**{"_recovery_key_output_path": "/k", "path.parent.mkdir": None,
              "os.open": 7, "file.write": None}),
    state=dict(SS))
add("_write_matrix_recovery_key_output_file", {"recovery_key": "k"},
    cov_io(**{"_recovery_key_output_path": None}))
add("_write_matrix_recovery_key_output_file", {"recovery_key": "k"},
    cov_io(**{"_recovery_key_output_path": "/k", "path.parent.mkdir": rs("ValueError", "denied")}))
add("_write_matrix_recovery_key_output_file", {"recovery_key": "k"},
    cov_io(**{"_recovery_key_output_path": "/k", "path.parent.mkdir": None,
              "os.open": 8, "file.write": rs("ValueError", "bad"), "os.close": None}))
add("_write_matrix_recovery_key_output_file", {"recovery_key": "k"},
    cov_io(**{"_recovery_key_output_path": "/k", "path.parent.mkdir": None,
              "os.open": rs("SystemExit", "bye")}))
add("_write_matrix_recovery_key_output_file", {"recovery_key": "k"}, fc=True)

# ══ 16. create_handoff_thread (extra) ══
add("create_handoff_thread", {"parent_chat_id": "!r:s", "name": "Thread"},
    cov_io(**{"t": "Handoff", "send": ad({"success": True, "message_id": "$root"}),
              "mark_async": None}), state={"_client": ad({})})
add("create_handoff_thread", {"parent_chat_id": "!r:s", "name": ""},
    cov_io(**{"t": "Default", "send": ad({"success": True, "message_id": "$r2"}),
              "mark_async": None}), state={"_client": ad({})})
add("create_handoff_thread", {"parent_chat_id": "!r:s", "name": "T"},
    cov_io(**{"t": "T", "send": ad({"success": False, "error": "no"})}),
    state={"_client": ad({})})
add("create_handoff_thread", {"parent_chat_id": "!r:s", "name": "T"},
    cov_io(**{"t": "T", "send": ad({"success": True, "message_id": None})}),
    state={"_client": ad({})})
add("create_handoff_thread", {"parent_chat_id": "!r:s", "name": ""},
    cov_io(**{"t": rs("SystemExit", "bye")}), state={"_client": ad({})})
add("create_handoff_thread", {"parent_chat_id": "!r:s", "name": "T"}, fc=True,
    state={"_client": None})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    # Set-equality guard: every seam must be exercised by at least one fixture,
    # and no unexpected io key may appear.
    keys = set()
    for c in CASES:
        keys |= set((c.get("in") or {}).get("io") or {})
    if keys != set(SEAM_NAMES):
        missing = sorted(set(SEAM_NAMES) - keys)
        extra = sorted(keys - set(SEAM_NAMES))
        raise SystemExit(f"io key drift: missing={missing} extra={extra}")
    with OUT.open("w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT} ({len(SEAM_NAMES)} seams)")


if __name__ == "__main__":
    main()
