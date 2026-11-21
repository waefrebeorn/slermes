#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow6.jsonl -- input cases for the
Matrix-adapter connect / sync / inbound / media / markdown / crypto oracle.
The oracle computes want from LIVE adapter.py at gate time; this script only
emits {func, in} inputs. >=6 handcrafted branch-coverage cases per function.
Every flow carries normal + branch variants; seam-driven funcs also carry a
scripted Exception-swallow, a BaseException-passthrough and a fail-closed
(fc=True) variant. Attribute carriers are {"__attr__": {...}}.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow6.jsonl"
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


# ── _crypto_db_path (seam: _get_hermes_dir) ──
add("_crypto_db_path", {}, state={"_store_dir": "/a/b"})
add("_crypto_db_path", {}, {"_get_hermes_dir": "/x/y"}, state={"_store_dir": ""})
add("_crypto_db_path", {}, {"_get_hermes_dir": "matrix/store"}, state={"_store_dir": None})
add("_crypto_db_path", {}, {"_get_hermes_dir": "/"}, state={"_store_dir": None})
add("_crypto_db_path", {}, {"_get_hermes_dir": rs("ValueError", "bad")}, state={"_store_dir": None})
add("_crypto_db_path", {}, {"_get_hermes_dir": rs("SystemExit", "bye")}, state={"_store_dir": None})
add("_crypto_db_path", {}, fc=True, state={"_store_dir": None})

# ── _abort_connect (seams: crypto_db.stop, session.close) ──
add("_abort_connect", {"crypto_db": ad({})}, {"crypto_db.stop": None, "session.close": None})
add("_abort_connect", {"crypto_db": None}, {"session.close": None})
add("_abort_connect", {}, {"session.close": None})
add("_abort_connect", {"crypto_db": ad({})}, {"crypto_db.stop": rs("ValueError", "bad"), "session.close": None})
add("_abort_connect", {"crypto_db": ad({})}, {"crypto_db.stop": None, "session.close": rs("SystemExit", "bye")})
add("_abort_connect", {"crypto_db": ad({})}, fc=True)

# ── _e2ee_setup_failed (seams: logger.warning, logger.error, _abort_connect) ──
add("_e2ee_setup_failed", {"what": "open", "exc": "boom", "api": ad({})},
    {"logger.warning": None}, state={"_e2ee_mode": "optional"})
add("_e2ee_setup_failed", {"what": "open", "exc": "boom", "api": ad({})},
    {"logger.error": None, "_abort_connect": False}, state={"_e2ee_mode": "required"})
add("_e2ee_setup_failed", {"what": "open", "exc": "boom", "api": ad({})},
    {"logger.warning": rs("ValueError", "bad")}, state={"_e2ee_mode": "optional"})
add("_e2ee_setup_failed", {"what": "open", "exc": "boom", "api": ad({})},
    {"logger.error": None, "_abort_connect": True}, state={"_e2ee_mode": None})
add("_e2ee_setup_failed", {"what": "open", "exc": "boom", "api": ad({})},
    {"logger.error": None, "_abort_connect": rs("SystemExit", "bye")}, state={"_e2ee_mode": "required"})
add("_e2ee_setup_failed", {"what": "open", "exc": "boom", "api": ad({})}, fc=True,
    state={"_e2ee_mode": "optional"})

# ── _connect_initial_sync (seams: sync, _absorb_sync, matrix_redaction_proxy,
#    describe_proxy_error, logger.warning) ──
CIS = {"_joined_rooms": ["!a:s"], "_proxy_url": "p", "_homeserver": "h"}
add("_connect_initial_sync", {"client": ad({})},
    {"sync": {"rooms": {"join": {}}}, "_absorb_sync": None}, state=CIS)
add("_connect_initial_sync", {"client": ad({})}, {"sync": None}, state=CIS)
add("_connect_initial_sync", {"client": ad({})}, {"sync": ["x"]}, state=CIS)
add("_connect_initial_sync", {"client": ad({})},
    {"sync": rs("ValueError", "bad"), "matrix_redaction_proxy": "redacted",
     "describe_proxy_error": "described"}, state=CIS)
add("_connect_initial_sync", {"client": ad({})}, {"sync": rs("SystemExit", "bye")}, state=CIS)
add("_connect_initial_sync", {"client": ad({})}, fc=True, state=CIS)

# ── _extract_reply_context ──
RT = {"m.in_reply_to": {"event_id": "$r"}}
add("_extract_reply_context", {"room_id": "!r:s", "body": "> <@a:s> quoted\n\nreply", "relates_to": RT},
    {"_extract_reply_fallback": sc(["quoted", "@a:s"]), "_strip_reply_fallback": "reply",
     "_get_display_name": "Alice"})
add("_extract_reply_context", {"room_id": "!r:s", "body": "plain", "relates_to": {}}, {})
add("_extract_reply_context", {"room_id": "!r:s", "body": "> x", "relates_to": {}}, {})
add("_extract_reply_context", {"room_id": "!r:s", "body": "> x", "relates_to": RT},
    {"_extract_reply_fallback": sc(["quoted", None]), "_strip_reply_fallback": "reply"})
add("_extract_reply_context", {"room_id": "!r:s", "body": "> x", "relates_to": RT},
    {"_extract_reply_fallback": rs("SystemExit", "bye")})
add("_extract_reply_context", {"room_id": "!r:s", "body": "> x", "relates_to": RT}, fc=True)

# ── _check_e2ee_deps (import gate; no seams) ──
add("_check_e2ee_deps", {"blocked": []})
add("_check_e2ee_deps", {"blocked": ["asyncpg"]})
add("_check_e2ee_deps", {"blocked": ["mautrix.crypto"]})
add("_check_e2ee_deps", {"blocked": ["aiosqlite"]})
add("_check_e2ee_deps", {"blocked": ["asyncpg", "aiosqlite"]})
add("_check_e2ee_deps", {"blocked": ["mautrix.crypto", "asyncpg", "aiosqlite"]})

# ── _create_matrix_session ──
add("_create_matrix_session", {"proxy_url": None}, {"gateway_trust_env": True, "ClientSession": "sess"})
add("_create_matrix_session", {"proxy_url": ""}, {"gateway_trust_env": False, "ClientSession": "sess"})
add("_create_matrix_session", {"proxy_url": "http://p:8080"}, {"ClientSession": "sess"})
add("_create_matrix_session", {"proxy_url": "socks5://p", "socks_available": True},
    {"ProxyConnector.from_url": "conn", "ClientSession": "sess"})
add("_create_matrix_session", {"proxy_url": "SOCKS://p", "socks_available": True},
    {"ProxyConnector.from_url": "conn", "ClientSession": "sess"})
add("_create_matrix_session", {"proxy_url": "socks://p", "socks_available": False},
    {"safe_proxy_url_for_log": "***", "logger.warning": None, "gateway_trust_env": True, "ClientSession": "sess"})
add("_create_matrix_session", {"proxy_url": None},
    {"gateway_trust_env": True, "ClientSession": rs("SystemExit", "bye")})
add("_create_matrix_session", {"proxy_url": None}, fc=True)

# ── _dispatch_sync (seams: handle_sync, gather, logger.warning) ──
add("_dispatch_sync", {"sync_data": {}}, {"handle_sync": sc([])}, state={"_client": None})
add("_dispatch_sync", {"sync_data": {}}, {"handle_sync": sc([])}, state={"_client": ad({})})
add("_dispatch_sync", {"sync_data": {}}, {"handle_sync": sc(["t1"]), "gather": sc(["ok"])}, state={"_client": ad({})})
add("_dispatch_sync", {"sync_data": {}},
    {"handle_sync": sc(["t1"]), "gather": sc([{"__raise__": "ValueError", "message": "boom"}])}, state={"_client": ad({})})
add("_dispatch_sync", {"sync_data": {}}, {"handle_sync": rs("ValueError", "bad")}, state={"_client": ad({})})
add("_dispatch_sync", {"sync_data": {}},
    {"handle_sync": sc(["t1"]), "gather": rs("SystemExit", "bye")}, state={"_client": ad({})})
add("_dispatch_sync", {"sync_data": {}}, fc=True, state={"_client": ad({})})

# ── _build_text_message_content ──
BT = {"_allow_room_mentions": True}
add("_build_text_message_content", {"text": "hello"},
    {"_extract_outbound_mentions": sc([]), "_has_outbound_room_mention": False,
     "_inject_outbound_mention_links": "hello", "_markdown_to_html": ""}, state=BT)
add("_build_text_message_content", {"text": "hi @a:s"},
    {"_extract_outbound_mentions": ["@a:s"], "_has_outbound_room_mention": False,
     "_inject_outbound_mention_links": "hi @a:s", "_markdown_to_html": "<b>hi</b>", "_content_bytes": 10}, state=BT)
add("_build_text_message_content", {"text": "@room hi"},
    {"_extract_outbound_mentions": sc([]), "_has_outbound_room_mention": True,
     "_inject_outbound_mention_links": "@room hi", "_markdown_to_html": "<b>x</b>", "_content_bytes": 10}, state=BT)
add("_build_text_message_content", {"text": "hi"},
    {"_extract_outbound_mentions": sc([]), "_has_outbound_room_mention": False,
     "_inject_outbound_mention_links": "hi", "_markdown_to_html": "**hi**",
     "_content_bytes": 50000}, state=BT)
add("_build_text_message_content", {"text": "hi"},
    {"_extract_outbound_mentions": sc([]), "_has_outbound_room_mention": False,
     "_inject_outbound_mention_links": "hi", "_markdown_to_html": rs("SystemExit", "bye")}, state=BT)
add("_build_text_message_content", {"text": "hi"}, fc=True, state=BT)

# ── _absorb_sync ──
AS = {"_joined_rooms": []}
add("_absorb_sync", {"client": ad({}), "sync_data": {"rooms": {"join": {"!a:s": {}}}, "next_batch": "s1"}, "initial": True},
    {"time.time": 1, "_invalidate_room_identities": None, "_warn_encrypted_drops": None,
     "put_next_batch": None, "logger.info": None, "_refresh_dm_cache": None,
     "_dispatch_sync": None, "_schedule_pending_invite_joins": None}, state=AS)
add("_absorb_sync", {"client": ad({}), "sync_data": {}, "initial": False},
    {"time.time": 1, "_warn_encrypted_drops": None, "_dispatch_sync": None,
     "_schedule_pending_invite_joins": None}, state=AS)
add("_absorb_sync", {"client": ad({}), "sync_data": {"next_batch": "s2"}},
    {"time.time": 1, "_warn_encrypted_drops": None, "put_next_batch": None,
     "_dispatch_sync": None, "_schedule_pending_invite_joins": None}, state=AS)
add("_absorb_sync", {"client": ad({}), "sync_data": {}, "initial": True},
    {"time.time": 1, "_warn_encrypted_drops": None, "logger.info": None,
     "_refresh_dm_cache": None, "_dispatch_sync": rs("ValueError", "bad"),
     "_schedule_pending_invite_joins": None}, state=AS)
add("_absorb_sync", {"client": ad({}), "sync_data": {}},
    {"time.time": 1, "_warn_encrypted_drops": None, "_dispatch_sync": rs("SystemExit", "bye")}, state=AS)
add("_absorb_sync", {"client": ad({}), "sync_data": {}}, fc=True, state=AS)

# ── _download_and_cache_media ──
DM = {"_max_media_bytes": 1000000, "_client": None}
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e", "encrypted_file": None,
    "msg_type": "photo", "media_type": "image/jpeg", "body": "img.jpg"},
    {"download_mxc_capped": "bytes", "cache_image": "/cache/img.jpg", "logger.info": None}, state=DM)
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e", "encrypted_file": None,
    "msg_type": "audio", "media_type": "audio/ogg", "body": "a.ogg"},
    {"download_mxc_capped": "bytes", "cache_audio": "/cache/a.ogg"}, state=DM)
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e", "encrypted_file": None,
    "msg_type": "voice", "media_type": "audio/ogg", "body": "", "is_voice_message": True},
    {"download_mxc_capped": "bytes", "cache_audio": "/cache/v.ogg"}, state=DM)
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e", "encrypted_file": None,
    "msg_type": "video", "media_type": "video/mp4", "body": ""},
    {"download_mxc_capped": "bytes", "cache_document": "/cache/video.mp4"}, state=DM)
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e",
    "encrypted_file": {"hashes": {"sha256": "h"}, "key": {"k": "k"}, "iv": "iv"},
    "msg_type": "document", "body": "doc"},
    {"download_mxc_capped": "bytes", "decrypt_attachment": "plain", "cache_document": "/cache/doc"}, state=DM)
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e",
    "encrypted_file": {"hashes": {}, "key": {}, "iv": None}, "msg_type": "document", "body": "doc"},
    {"download_mxc_capped": "bytes", "logger.warning": None}, state=DM)
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e", "encrypted_file": None,
    "msg_type": "document", "body": "doc"}, {"download_mxc_capped": rs("SystemExit", "bye")}, state=DM)
add("_download_and_cache_media", {"url": "mxc://s/x", "event_id": "$e", "encrypted_file": None,
    "msg_type": "document", "body": "doc"}, fc=True, state=DM)

# ── _background_read_receipt (seam: ensure_future) ──
add("_background_read_receipt", {"room_id": "!r:s", "event_id": "$e"}, {"ensure_future": None})
add("_background_read_receipt", {"room_id": "!r2:s", "event_id": "$e2"}, {"ensure_future": None})
add("_background_read_receipt", {"room_id": "!r3:s", "event_id": "$e3"}, {"ensure_future": "task"})
add("_background_read_receipt", {"room_id": "!r:s", "event_id": "$e"}, {"ensure_future": rs("ValueError", "bad")})
add("_background_read_receipt", {"room_id": "!r:s", "event_id": "$e"}, {"ensure_future": rs("SystemExit", "bye")})
add("_background_read_receipt", {"room_id": "!r:s", "event_id": "$e"}, fc=True)

# ── _markdown_to_html ──
add("_markdown_to_html", {"text": "**hi**", "markdown_available": True, "html_block_present": True},
    {"_pre_sanitize_matrix_markdown": "**hi**", "_latex_to_tokens": sc(["**hi**", "tex"]),
     "Markdown": None, "md.preprocessors.deregister": None, "md.convert": "<p>hi</p>",
     "md.reset": None, "_sanitize_matrix_html": "clean"})
add("_markdown_to_html", {"text": "hi", "markdown_available": True, "html_block_present": False},
    {"_pre_sanitize_matrix_markdown": "hi", "_latex_to_tokens": sc(["hi", None]),
     "Markdown": None, "md.convert": "<p>hi</p>", "md.reset": None, "_sanitize_matrix_html": "clean"})
add("_markdown_to_html", {"text": "hi", "markdown_available": True},
    {"_pre_sanitize_matrix_markdown": "hi", "_latex_to_tokens": sc(["hi", "tex"]),
     "Markdown": None, "md.convert": "hi", "md.reset": None, "_sanitize_matrix_html": "clean"})
add("_markdown_to_html", {"text": "hi", "markdown_available": False},
    {"_pre_sanitize_matrix_markdown": "hi", "_latex_to_tokens": sc(["hi", "tex"]),
     "_markdown_to_html_fallback": "<p>hi</p>", "_sanitize_matrix_html": "clean"})
add("_markdown_to_html", {"text": "hi"}, {"_pre_sanitize_matrix_markdown": rs("SystemExit", "bye")})
add("_markdown_to_html", {"text": "hi"}, fc=True)

# ── _migrate_legacy_crypto_pickle ──
MG = {"acct_id": "@me:s", "pickle_key": "@me:s:DEV", "crypto_db": "db"}
add("_migrate_legacy_crypto_pickle", MG, {"get_account": {}})
add("_migrate_legacy_crypto_pickle", MG,
    {"get_account": rs("ValueError", "bad"), "PgCryptoStore.get_account": "acct",
     "_repickle_crypto_sessions": None, "put_account": None, "logger.info": None})
add("_migrate_legacy_crypto_pickle", MG,
    {"get_account": rs("ValueError", "bad"), "PgCryptoStore.get_account": "acct",
     "_repickle_crypto_sessions": rs("ValueError", "bad"), "logger.error": None})
add("_migrate_legacy_crypto_pickle", MG,
    {"get_account": rs("ValueError", "bad"), "PgCryptoStore.get_account": None, "logger.error": None})
add("_migrate_legacy_crypto_pickle", MG, {"get_account": rs("SystemExit", "bye")})
add("_migrate_legacy_crypto_pickle", MG,
    {"get_account": rs("ValueError", "bad"), "PgCryptoStore.get_account": rs("SystemExit", "bye")})
add("_migrate_legacy_crypto_pickle", MG, fc=True)

# ── _build_inbound_event ──
CTX_ARG = ["body", True, "dm", "$t", "Alice", "local"]
CTX = sc(["body", True, "dm", "$t", "Alice", "local"])
add("_build_inbound_event", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
    "source_content": {"body": "hi"}, "relates_to": {}},
    {"_resolve_message_context": CTX, "_extract_reply_context": sc(["hi", None, None, None, None]),
     "_normalize_matrix_bang_command": "hi", "MessageEvent": "ev"})
add("_build_inbound_event", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
    "source_content": {"body": "hi"}, "relates_to": {}, "ctx": CTX_ARG},
    {"_extract_reply_context": sc(["hi", None, None, None, None]),
     "_normalize_matrix_bang_command": "hi", "MessageEvent": "ev"})
add("_build_inbound_event", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "file.jpg",
    "source_content": {"body": "file.jpg"}, "relates_to": {}, "ctx": CTX_ARG,
    "extra": {"media_msgtype": "m.image"}},
    {"_extract_reply_context": sc(["file.jpg", None, None, None, None]),
     "_is_bare_media_filename": True, "MessageEvent": "ev"})
add("_build_inbound_event", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "/cmd",
    "source_content": {"body": "/cmd"}, "relates_to": {}, "ctx": CTX_ARG},
    {"_extract_reply_context": sc(["/cmd", None, None, None, None]),
     "_normalize_matrix_bang_command": "/cmd", "MessageEvent": "ev"})
add("_build_inbound_event", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
    "source_content": {"body": "hi"}, "relates_to": {}},
    {"_resolve_message_context": None})
add("_build_inbound_event", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
    "source_content": {"body": "hi"}, "relates_to": {}, "ctx": CTX_ARG},
    {"_extract_reply_context": rs("SystemExit", "bye")})
add("_build_inbound_event", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "body": "hi",
    "source_content": {"body": "hi"}, "relates_to": {}}, fc=True)

# ── _handle_text_message ──
HT = {"_allow_room_mentions": True, "_text_batch_delay_seconds": 0, "_parked_voices": {}}
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "hi"}, "relates_to": {}},
    {"pending": False, "_normalize_matrix_bang_command": "hi",
     "_build_inbound_event": ad({"message_type": "text"}), "handle_message": None}, state=HT)
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "hi"}, "relates_to": {}},
    {"pending": True, "_strip_mention": "hi", "_normalize_matrix_bang_command": "hi",
     "_build_inbound_event": ad({"message_type": "text"}), "handle_message": None}, state=HT)
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "@bot"}, "relates_to": {}},
    {"pending": True, "_strip_mention": "  ", "_content_mentions_bot": True,
     "mark": 5, "settle": None, "claim": sc(["$v", {"body": "voice"}, {}]),
     "_handle_media_message": None, "_background_read_receipt": None}, state=HT)
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "@bot"}, "relates_to": {}},
    {"pending": True, "_strip_mention": "", "_content_mentions_bot": False,
     "_normalize_matrix_bang_command": "@bot", "_build_inbound_event": ad({"message_type": "text"}),
     "handle_message": None}, state=HT)
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "hi"}, "relates_to": {}},
    {"pending": False, "_normalize_matrix_bang_command": "hi", "_build_inbound_event": None}, state=HT)
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "hi"}, "relates_to": {}},
    {"pending": False, "_normalize_matrix_bang_command": "hi",
     "_build_inbound_event": ad({"message_type": "text"}), "_enqueue_text_event": None},
    state=dict(HT, _text_batch_delay_seconds=3))
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "hi"}, "relates_to": {}},
    {"pending": rs("SystemExit", "bye")}, state=HT)
add("_handle_text_message", {"room_id": "!r:s", "sender": "@u:s", "event_id": "$e", "event_ts": 1,
    "source_content": {"body": "hi"}, "relates_to": {}}, fc=True, state=HT)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")


if __name__ == "__main__":
    main()
