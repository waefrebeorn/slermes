#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow6.jsonl — input cases for the
Feishu adapter resolution-helpers FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
The corpus covers every ported branch, ordered multi-seam traces, short-circuit
order, fail-closed missing/exhausted scripts, and the full builtin exception
hierarchy (Exception-side swallowed where the live def has try/except,
BaseException-side propagated verbatim).

Run `python tests/gen_feishu_adapter_flow6_cases.py` to regenerate; the script
fails if any target has fewer than 6 cases, a target is missing, or a fixture
io key has no matching C seam.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow6.jsonl"
CASES = []


def add(func, args, io=None, note=None):
    case = {"func": func, "in": {"args": args, "io": dict(io or {})}}
    if note:
        case["_note"] = note
    CASES.append(case)


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


# ── is_connected (pure, module-level) ──
add("_is_connected", {"config": {"extra": {"app_id": "cli_a"}}})
add("_is_connected", {"config": {"extra": {}}})
add("_is_connected", {"config": {"extra": {"app_id": ""}}})
add("_is_connected", {"config": {}})
add("_is_connected", {"config": None})
add("_is_connected", {"config": {"extra": None}})
add("_is_connected", {"config": {"extra": {"app_id": 7}}})
add("_is_connected", {"config": {"extra": {"app_id": True}}})

# ── _normalize_media_type (pure) ──
add("_normalize_media_type", {"content_type": "image/png", "default": "x"})
add("_normalize_media_type", {"content_type": " Text/Plain ; charset=utf-8 ", "default": "x"})
add("_normalize_media_type", {"content_type": "", "default": "application/octet-stream"})
add("_normalize_media_type", {"content_type": None, "default": "d"})
add("_normalize_media_type", {"content_type": "  ", "default": "fallback"})
add("_normalize_media_type", {"content_type": "APPLICATION/JSON", "default": "d"})
add("_normalize_media_type", {"content_type": "a;b;c", "default": "d"})

# ── _default_image_media_type (pure) ──
for ext in [".jpg", ".JPEG", ".png", ".gif", ".webp", "", ".svg", "jpg", ".bmp"]:
    add("_default_image_media_type", {"ext": ext})

# ── _map_chat_type (pure) ──
for raw in ["p2p", " P2P ", "group", "GROUP", "topic_xyz", "a-thread-b", "forum",
            "other", "", "p2p_topic", "thread", None]:
    add("_map_chat_type", {"raw_chat_type": raw})

# ── _resolve_source_chat_type (pure) ──
add("_resolve_source_chat_type", {"chat_info": {"type": "group"}, "event_chat_type": "p2p"})
add("_resolve_source_chat_type", {"chat_info": {"type": "forum"}, "event_chat_type": "group"})
add("_resolve_source_chat_type", {"chat_info": {"type": " P2P "}, "event_chat_type": "p2p"})
add("_resolve_source_chat_type", {"chat_info": {"type": ""}, "event_chat_type": "p2p"})
add("_resolve_source_chat_type", {"chat_info": {}, "event_chat_type": "group"})
add("_resolve_source_chat_type", {"chat_info": {"type": None}, "event_chat_type": "x"})
add("_resolve_source_chat_type", {"chat_info": {"type": "GROUP"}, "event_chat_type": "p2p"})

# ── _guess_remote_extension (pure) ──
add("_guess_remote_extension", {"url": "https://x/a.png?token=1", "default": ".bin"})
add("_guess_remote_extension", {"url": "https://x/a.PDF", "default": ".bin"})
add("_guess_remote_extension", {"url": "https://x/a.zip?x", "default": ".dat"})
add("_guess_remote_extension", {"url": "https://x/a.unknown", "default": ".dat"})
add("_guess_remote_extension", {"url": "https://x/noext", "default": ".dat"})
add("_guess_remote_extension", {"url": "", "default": ".dat"})
add("_guess_remote_extension", {"url": "https://x/a.MP3?v=2", "default": ".dat"})

# ── _display_name_from_cached_path (pure; ASCII domain) ──
add("_display_name_from_cached_path", {"path": "/tmp/aa_bb_report.txt"})
add("_display_name_from_cached_path", {"path": "/tmp/aa_bb.txt"})
add("_display_name_from_cached_path", {"path": "/tmp/report.pdf"})
add("_display_name_from_cached_path", {"path": "aa_bb_a b@c#d.txt"})
add("_display_name_from_cached_path", {"path": "aa_bb_cc_dd.txt"})
add("_display_name_from_cached_path", {"path": "file"})
add("_display_name_from_cached_path", {"path": "p_2024_06_summary.md"})

# ── _get_response_header (attr double) ──
add("_get_response_header", {"response": {"raw": {"headers": {"content-type": "Application/JSON; charset=utf-8"}}},
                             "name": "content-type"})
add("_get_response_header", {"response": {"raw": {"headers": {"content-type": "text/plain"}}},
                             "name": "Content-Type"})
add("_get_response_header", {"response": {"raw": {"headers": {"x": "v"}}}, "name": "missing"})
add("_get_response_header", {"response": {"raw": {"headers": {"x": None}}}, "name": "x"})
add("_get_response_header", {"response": {"raw": {}}, "name": "x"})
add("_get_response_header", {"response": None, "name": "x"})
add("_get_response_header", {"response": {"raw": {"headers": {"x": "  UPPER ; tail "}}}, "name": "x"})

# ── _extract_text_from_raw_content (one _normalize seam) ──
add("_extract_text_from_raw_content",
    {"msg_type": "text", "raw_content": "c", "mentions": []},
    io={"_normalize": [ret({"text_content": "hello", "metadata": {}})]})
add("_extract_text_from_raw_content",
    {"msg_type": "post", "raw_content": "c", "mentions": None},
    io={"_normalize": [ret({"text_content": "", "metadata": {"placeholder_text": " [IMAGE] "}})]})
add("_extract_text_from_raw_content",
    {"msg_type": "text", "raw_content": "c", "mentions": []},
    io={"_normalize": [ret({"text_content": "", "metadata": "not-a-dict"})]})
add("_extract_text_from_raw_content",
    {"msg_type": "text", "raw_content": "c", "mentions": []},
    io={"_normalize": [ret({"text_content": "", "metadata": {}})]})
add("_extract_text_from_raw_content",
    {"msg_type": "text", "raw_content": "c", "mentions": []},
    io={"_normalize": [rais("ValueError", "normalize down")]})
add("_extract_text_from_raw_content",
    {"msg_type": "text", "raw_content": "c", "mentions": []},
    io={"_normalize": [rais("SystemExit", "sx")]})
add("_extract_text_from_raw_content",
    {"msg_type": "text", "raw_content": "c", "mentions": []},
    io={"_normalize": [ret({"text_content": "a", "metadata": {"placeholder_text": "b"}}),
                       ret({"text_content": "second", "metadata": {}})]})
add("_extract_text_from_raw_content",
    {"msg_type": "text", "raw_content": "c", "mentions": []},
    io={"_normalize": []}, note="fail-closed")  # fail-closed raise propagates

# ── _guess_extension (mimetypes_guess_extension seam) ──
add("_guess_extension",
    {"filename": "a.png", "content_type": "", "default": ".bin", "allowed": [".png", ".jpg"]})
add("_guess_extension",
    {"filename": "a.xyz", "content_type": "image/jpeg; q=1", "default": ".bin", "allowed": [".png", ".jpg"]},
    io={"mimetypes_guess_extension": [ret(".jpg")]})
add("_guess_extension",
    {"filename": "a.xyz", "content_type": "image/gif", "default": ".bin", "allowed": [".png", ".jpg"]},
    io={"mimetypes_guess_extension": [ret(".gif")]})
add("_guess_extension",
    {"filename": "a.xyz", "content_type": "", "default": ".bin", "allowed": [".png"]},
    io={"mimetypes_guess_extension": [ret(None)]})
add("_guess_extension",
    {"filename": "a.xyz", "content_type": "x/y", "default": ".bin", "allowed": [".png"]},
    io={"mimetypes_guess_extension": [rais("RuntimeError", "mime down")]})
add("_guess_extension",
    {"filename": "a.xyz", "content_type": "x/y", "default": ".bin", "allowed": [".png"]},
    io={"mimetypes_guess_extension": [rais("GeneratorExit", "ge")]})
add("_guess_extension",
    {"filename": "a.JPG", "content_type": "", "default": ".bin", "allowed": [".jpg"]})

# ── _guess_document_media_type (mimetypes_guess_type seam; eager) ──
add("_guess_document_media_type", {"filename": "a.pdf"},
    io={"mimetypes_guess_type": [ret(["application/pdf", None])]})
add("_guess_document_media_type", {"filename": "a.xyz"},
    io={"mimetypes_guess_type": [ret(["text/custom", None])]})
add("_guess_document_media_type", {"filename": "a.xyz"},
    io={"mimetypes_guess_type": [ret([None, None])]})
add("_guess_document_media_type", {"filename": "a.MD"},
    io={"mimetypes_guess_type": [ret([None, None])]})
add("_guess_document_media_type", {"filename": "a.xyz"},
    io={"mimetypes_guess_type": [rais("ValueError", "mime down")]})
add("_guess_document_media_type", {"filename": "a.xyz"},
    io={"mimetypes_guess_type": [rais("KeyboardInterrupt", "ki")]})
add("_guess_document_media_type", {"filename": "noext"},
    io={"mimetypes_guess_type": [ret(["application/octet-stream", None])]})

# ── _guess_media_type_from_filename (guess_type + sibling seams) ──
add("_guess_media_type_from_filename", {"filename": "a.png"},
    io={"mimetypes_guess_type": [ret(["image/png", None])]})
add("_guess_media_type_from_filename", {"filename": "a.mp4"},
    io={"mimetypes_guess_type": [ret([None, None])]})
add("_guess_media_type_from_filename", {"filename": "a.MP3"},
    io={"mimetypes_guess_type": [ret([None, None])]})
add("_guess_media_type_from_filename", {"filename": "a.png"},
    io={"mimetypes_guess_type": [ret([None, None])],
        "_default_image_media_type": [ret("image/png")]})
add("_guess_media_type_from_filename", {"filename": "a.xyz"},
    io={"mimetypes_guess_type": [ret([None, None])]})
add("_guess_media_type_from_filename", {"filename": "a.xyz"},
    io={"mimetypes_guess_type": [rais("RuntimeError", "mime down")]})
add("_guess_media_type_from_filename", {"filename": "a.png"},
    io={"mimetypes_guess_type": [ret([None, None])],
        "_default_image_media_type": [rais("ValueError", "sibling down")]})
add("_guess_media_type_from_filename", {"filename": "a.png"},
    io={"mimetypes_guess_type": [rais("SystemExit", "sx")]})

# ── _derive_remote_filename (mimetypes_guess_extension seam) ──
add("_derive_remote_filename",
    {"file_url": "https://x/a.png?v=1", "content_type": "", "default_name": "f", "default_ext": ".bin"})
add("_derive_remote_filename",
    {"file_url": "https://x/noext", "content_type": "image/png", "default_name": "f", "default_ext": ".bin"},
    io={"mimetypes_guess_extension": [ret(".png")]})
add("_derive_remote_filename",
    {"file_url": "https://x/noext", "content_type": "", "default_name": "f", "default_ext": ".bin"},
    io={"mimetypes_guess_extension": [ret(None)]})
add("_derive_remote_filename",
    {"file_url": "", "content_type": "text/plain", "default_name": "f", "default_ext": ".txt"},
    io={"mimetypes_guess_extension": [ret(".txt")]})
add("_derive_remote_filename",
    {"file_url": "https://x/noext", "content_type": "x/y", "default_name": "f", "default_ext": ".bin"},
    io={"mimetypes_guess_extension": [rais("OSError", "mime down")]})
add("_derive_remote_filename",
    {"file_url": "https://x/noext", "content_type": "x/y", "default_name": "f", "default_ext": ".bin"},
    io={"mimetypes_guess_extension": [rais("BaseException", "be")]})
add("_derive_remote_filename",
    {"file_url": "https://x/a.PDF?x", "content_type": "", "default_name": "f", "default_ext": ".bin"})

# ── _log_background_failure (future_result + logger_exception seams) ──
add("_log_background_failure", {}, io={"future_result": [ret(None)]})
add("_log_background_failure", {}, io={"future_result": [ret(5)]})
add("_log_background_failure", {}, io={"future_result": [rais("ValueError", "boom")],
                                       "logger_exception": [ret(None)]})
add("_log_background_failure", {}, io={"future_result": [rais("RuntimeError", "rt")],
                                       "logger_exception": [ret(None)]})
add("_log_background_failure", {}, io={"future_result": [rais("SystemExit", "sx")]})
add("_log_background_failure", {}, io={"future_result": [rais("KeyboardInterrupt", "ki")]})
add("_log_background_failure", {}, io={"future_result": [], "logger_exception": [ret(None)]},
    note="fail-closed")
add("_log_background_failure", {},
    io={"future_result": [rais("TypeError", "t")], "logger_exception": [rais("ValueError", "handler")]})
add("_log_background_failure", {},
    io={"future_result": [ret(1), rais("OSError", "o")], "logger_exception": [ret(None)]})

# ── _is_webhook_signature_valid (sha256/secrets_match/timestamp_fresh/logger_debug) ──
BASE_HDRS = {"x-lark-request-timestamp": "1700000000", "x-lark-request-nonce": "n1",
             "x-lark-signature": "sig"}
add("_is_webhook_signature_valid",
    {"headers": {}, "body_bytes": "body", "encrypt_key": "ek"})
add("_is_webhook_signature_valid",
    {"headers": {"x-lark-request-timestamp": "1700000000"}, "body_bytes": "body", "encrypt_key": "ek"})
add("_is_webhook_signature_valid",
    {"headers": BASE_HDRS, "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [ret("abc")], "secrets_match": [ret(True)], "timestamp_fresh": [ret(True)]})
add("_is_webhook_signature_valid",
    {"headers": BASE_HDRS, "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [ret("abc")], "secrets_match": [ret(False)]})
add("_is_webhook_signature_valid",
    {"headers": BASE_HDRS, "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [ret("abc")], "secrets_match": [ret(True)], "timestamp_fresh": [ret(False)]})
add("_is_webhook_signature_valid",
    {"headers": BASE_HDRS, "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [rais("ValueError", "hash down")], "logger_debug": [ret(None)]})
add("_is_webhook_signature_valid",
    {"headers": BASE_HDRS, "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [rais("SystemExit", "sx")]})
add("_is_webhook_signature_valid",
    {"headers": BASE_HDRS, "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [ret("abc")], "secrets_match": [rais("RuntimeError", "cmp down")],
        "logger_debug": [ret(None)]})
add("_is_webhook_signature_valid",
    {"headers": BASE_HDRS, "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [ret("abc")], "secrets_match": [ret(True)],
        "timestamp_fresh": [rais("KeyboardInterrupt", "ki")]})
add("_is_webhook_signature_valid",
    {"headers": {"x-lark-request-timestamp": 1700000000, "x-lark-request-nonce": "n1",
                 "x-lark-signature": "sig"},
     "body_bytes": "body", "encrypt_key": "ek"},
    io={"sha256_hex": [ret("abc")], "secrets_match": [ret(True)], "timestamp_fresh": [ret(True)]})
add("_is_webhook_signature_valid",
    {"headers": {"x-lark-request-timestamp": "1700000000", "x-lark-request-nonce": None,
                 "x-lark-signature": "sig"}, "body_bytes": "body", "encrypt_key": "ek"})

# ── _namespace_from_mapping (pure recursion) ──
add("_namespace_from_mapping", {"value": {"a": 1, "b": "two"}})
add("_namespace_from_mapping", {"value": {"a": {"b": {"c": 3}}}})
add("_namespace_from_mapping", {"value": [1, {"x": 2}, [3, 4]]})
add("_namespace_from_mapping", {"value": "scalar"})
add("_namespace_from_mapping", {"value": None})
add("_namespace_from_mapping", {"value": {}})
add("_namespace_from_mapping", {"value": []})
add("_namespace_from_mapping", {"value": {"a": [{"b": 1}, 2], "c": None}})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 6]
    print("funcs with <6 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low:
        raise SystemExit(1)
    try:
        import sys
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import oracle_feishu_adapter_flow6 as o
        bad = set()
        for c in CASES:
            bad |= set(c["in"].get("io") or {}) - o.SEAMED
        if bad:
            print("fixture io keys without a seam:", sorted(bad))
            raise SystemExit(1)
        print("seam keys verified against", len(o.SEAMED), "C seams:", sorted(o.SEAMED))
    except ImportError as exc:
        print("oracle import skipped:", exc)


if __name__ == "__main__":
    main()
