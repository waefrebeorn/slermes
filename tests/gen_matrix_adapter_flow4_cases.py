#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow4.jsonl -- input cases for the
Matrix-adapter config & classification helper flow oracle (the oracle computes
want from LIVE adapter.py at gate time; this script only emits {func, in} inputs).

Deterministic handcrafted branch-coverage cases (>=6 per function). Each flow
carries normal, branch, scripted-Exception-swallow, BaseException-passthrough
and fail-closed (fc=True) variants where seams are involved. Attribute carriers
are {"__attr__": {...}}; config.extra is a plain dict.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow4.jsonl"
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


def ad(attrs):
    return {"__attr__": attrs}


def cfg(extra=None):
    return ad({"extra": extra if extra is not None else {}})


def rs(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def sc(v):
    return {"__script__": True, "ret": v}


def mt(t, enc=None):
    """mimetypes.guess_type script result: a (type, encoding) pair.
    Wrapped in __script__ so the 2-elem LIST is a literal return, not a
    call-order script (a bare list in in.io is consumed as sequential calls)."""
    return {"__script__": True, "ret": [t, enc]}


# ── _utf8_len (no seams) ──
add("_utf8_len", {"text": "hello"})
add("_utf8_len", {"text": ""})
add("_utf8_len", {"text": "你好"})                       # 6 UTF-8 bytes
add("_utf8_len", {"text": "😀"})                        # 4 UTF-8 bytes
add("_utf8_len", {"text": "a你好b"})
add("_utf8_len", {"text": "line\nwith\ttabs"})
add("_utf8_len", {"text": "café"})
add("_utf8_len", {"text": "x" * 100})

# ── _resolve_max_message_length ──
add("_resolve_max_message_length", {"config": cfg({"max_message_length": 3000})},
    {"_extra_or_secret": 3000})
add("_resolve_max_message_length", {"config": cfg({"max_message_length": "abc"})},
    {"_extra_or_secret": "abc"})
add("_resolve_max_message_length", {"config": cfg({})},
    {"_extra_or_secret": None,
     "platform_registry.get": ad({"max_message_length": 9000})})
add("_resolve_max_message_length", {"config": cfg({})},
    {"_extra_or_secret": None,
     "platform_registry.get": ad({"max_message_length": 100})})
add("_resolve_max_message_length", {"config": cfg({})},
    {"_extra_or_secret": None,
     "platform_registry.get": ad({"max_message_length": 99999})})
add("_resolve_max_message_length", {"config": cfg({})},
    {"_extra_or_secret": None, "platform_registry.get": None})
add("_resolve_max_message_length", {"config": cfg({})},
    {"_extra_or_secret": 0})                              # "0" is not blank
add("_resolve_max_message_length", {"config": cfg({})},
    {"_extra_or_secret": None, "platform_registry.get": rs("RuntimeError", "no registry")})
add("_resolve_max_message_length", {"config": cfg({})},
    {"_extra_or_secret": rs("SystemExit", "bye")})
add("_resolve_max_message_length", {"config": cfg({})}, fc=True)

# ── _looks_like_transport_filename ──
TP = {"mime_prefixes": ["image/"], "exts": [".jpg", ".png"], "reject_spaces": False}
add("_looks_like_transport_filename", dict(TP, text="photo.jpg"),
    {"mimetypes.guess_type": mt("image/jpeg")})
add("_looks_like_transport_filename", dict(TP, text="photo.pdf"),
    {"mimetypes.guess_type": mt("application/pdf")})
add("_looks_like_transport_filename", dict(TP, text="photo.pdf"),
    {"mimetypes.guess_type": mt(None)})
add("_looks_like_transport_filename", dict(TP, text="a/b.jpg"),
    {})                                                   # path check short-circuits
add("_looks_like_transport_filename", dict(TP, text="note with spaces.jpg", reject_spaces=True),
    {})
add("_looks_like_transport_filename", dict(TP, text=""),
    {})
add("_looks_like_transport_filename", dict(TP, text="foo.jpg/"),
    {})
add("_looks_like_transport_filename", dict(TP, text="photo.jpg"),
    {"mimetypes.guess_type": rs("ValueError", "bad mime")})
add("_looks_like_transport_filename", dict(TP, text="photo.jpg"), fc=True)

# ── _looks_like_matrix_image_filename ──
add("_looks_like_matrix_image_filename", {"text": "cat.png"},
    {"mimetypes.guess_type": mt("image/png")})
add("_looks_like_matrix_image_filename", {"text": "cat.txt"},
    {"mimetypes.guess_type": mt(None)})
add("_looks_like_matrix_image_filename", {"text": "cat.png"},
    {"mimetypes.guess_type": mt(None)})                   # suffix wins
add("_looks_like_matrix_image_filename", {"text": "cat.PNG"},
    {"mimetypes.guess_type": mt(None)})
add("_looks_like_matrix_image_filename", {"text": ""}, {})
add("_looks_like_matrix_image_filename", {"text": "CAPTION with spaces.jpg"},
    {"mimetypes.guess_type": mt(None)})
add("_looks_like_matrix_image_filename", {"text": "cat.png"},
    {"mimetypes.guess_type": rs("SystemExit", "bye")})
add("_looks_like_matrix_image_filename", {"text": "cat.png"}, fc=True)

# ── _looks_like_matrix_media_filename ──
add("_looks_like_matrix_media_filename", {"text": "song.mp3"},
    {"mimetypes.guess_type": mt("audio/mpeg")})
add("_looks_like_matrix_media_filename", {"text": "song.mp3"},
    {"mimetypes.guess_type": mt(None)})
add("_looks_like_matrix_media_filename", {"text": "my song.mp3"},
    {})                                                   # reject_spaces
add("_looks_like_matrix_media_filename", {"text": "clip.mp4"},
    {"mimetypes.guess_type": mt("video/mp4")})
add("_looks_like_matrix_media_filename", {"text": "doc.txt"},
    {"mimetypes.guess_type": mt(None)})
add("_looks_like_matrix_media_filename", {"text": ""}, {})
add("_looks_like_matrix_media_filename", {"text": "song.mp3"}, fc=True)

# ── _is_bare_media_filename ──
add("_is_bare_media_filename", {"msgtype": "m.image", "body": "a.png"},
    {"mimetypes.guess_type": mt("image/png")})
add("_is_bare_media_filename", {"msgtype": "m.audio", "body": "a.mp3"},
    {"mimetypes.guess_type": mt("audio/mpeg")})
add("_is_bare_media_filename", {"msgtype": "m.file", "body": "a.pdf"},
    {"mimetypes.guess_type": mt(None)})
add("_is_bare_media_filename", {"msgtype": "m.video", "body": "a.mp4"},
    {"mimetypes.guess_type": mt("video/mp4")})
add("_is_bare_media_filename", {"msgtype": "m.text", "body": "a.png"}, {})
add("_is_bare_media_filename", {"msgtype": "m.image", "body": ""}, {})
add("_is_bare_media_filename", {"msgtype": "m.image", "body": "a.png"}, fc=True)

# ── _matrix_event_timestamp_seconds (no seams) ──
add("_matrix_event_timestamp_seconds", {"event": ad({"timestamp": 1700000000000})})
add("_matrix_event_timestamp_seconds", {"event": ad({"timestamp": 1700000000})})
add("_matrix_event_timestamp_seconds", {"event": ad({"timestamp": 0, "server_timestamp": 1700000000000})})
add("_matrix_event_timestamp_seconds", {"event": ad({"timestamp": "1700000000000"})})
add("_matrix_event_timestamp_seconds", {"event": ad({"timestamp": "bad"})})
add("_matrix_event_timestamp_seconds", {"event": ad({"timestamp": None, "server_timestamp": None})})
add("_matrix_event_timestamp_seconds", {"event": ad({})})
add("_matrix_event_timestamp_seconds", {"event": None})

# ── _normalize_e2ee_mode (no seams) ──
for v in ["required", "YES", " 1 ", "prefer", "optional", "off", "garbage", 1, True, None]:
    add("_normalize_e2ee_mode", {"value": v})

# ── _resolve_e2ee_mode ──
add("_resolve_e2ee_mode", {"extra": {"e2ee_mode": "require"}}, {})
add("_resolve_e2ee_mode", {"extra": {"e2ee_mode": "preferred"}}, {})
add("_resolve_e2ee_mode", {"extra": {}}, {"_get_scoped_secret": "optional"})
add("_resolve_e2ee_mode", {"extra": {}}, {"_get_scoped_secret": ["", "true"]})
add("_resolve_e2ee_mode", {"extra": {"encryption": True}},
    {"_get_scoped_secret": ["", "false"]})
add("_resolve_e2ee_mode", {"extra": {"encryption": False}},
    {"_get_scoped_secret": ["", "true"]})
add("_resolve_e2ee_mode", {"extra": {}}, {"_get_scoped_secret": rs("SystemExit", "bye")})
add("_resolve_e2ee_mode", {"extra": {}}, fc=True)

# ── _env_truthy ──
for v in ["true", "TRUE", "1", "yes", " true ", "false", None]:
    add("_env_truthy", {"name": "MATRIX_X", "default": ""}, {"_get_scoped_secret": v})
add("_env_truthy", {"name": "MATRIX_X"}, {"_get_scoped_secret": rs("ValueError", "bad")})
add("_env_truthy", {"name": "MATRIX_X"}, fc=True)

# ── _env_number ──
add("_env_number", {"name": "MATRIX_N", "default": 7, "cast": "int"},
    {"_get_scoped_secret": "42"})
add("_env_number", {"name": "MATRIX_N", "default": 7, "cast": "int"},
    {"_get_scoped_secret": "abc"})
add("_env_number", {"name": "MATRIX_N", "default": 7, "cast": "int"},
    {"_get_scoped_secret": ""})
add("_env_number", {"name": "MATRIX_N", "default": 2.5, "cast": "float"},
    {"_get_scoped_secret": "3.5"})
add("_env_number", {"name": "MATRIX_N", "default": 2.5, "cast": "float"},
    {"_get_scoped_secret": "3"})
add("_env_number", {"name": "MATRIX_N", "default": 7, "cast": "int"},
    {"_get_scoped_secret": " 12 "})
add("_env_number", {"name": "MATRIX_N", "default": 7, "cast": "int"},
    {"_get_scoped_secret": rs("SystemExit", "bye")})
add("_env_number", {"name": "MATRIX_N", "default": 7, "cast": "int"}, fc=True)

# ── _csv_set (no seams) ──
add("_csv_set", {"raw": "a, b ,c"})
add("_csv_set", {"raw": ["a", " b ", "", "c"]})
add("_csv_set", {"raw": ""})
add("_csv_set", {"raw": "a,a,b"})
add("_csv_set", {"raw": ["x", "x"]})
add("_csv_set", {"raw": 123})
add("_csv_set", {"raw": None})

# ── _thread_root (no seams) ──
add("_thread_root", {"relates_to": {"rel_type": "m.thread", "event_id": "$root"}})
add("_thread_root", {"relates_to": {"rel_type": "m.reference", "event_id": "$root"}})
add("_thread_root", {"relates_to": {"rel_type": "m.thread"}})
add("_thread_root", {"relates_to": {}})
add("_thread_root", {"relates_to": None})
add("_thread_root", {"relates_to": {"rel_type": "m.thread", "event_id": None}})

# ── _extra_csv_set ──
add("_extra_csv_set", {"config": cfg({}), "key": "allowed_rooms", "env_name": "MATRIX_ALLOWED_ROOMS"},
    {"_extra_or_secret": "!a:s, !b:s"})
add("_extra_csv_set", {"config": cfg({"allowed_rooms": "x"}), "key": "allowed_rooms",
                       "env_name": "MATRIX_ALLOWED_ROOMS"},
    {"_extra_or_secret": sc(["x", "y"])})
add("_extra_csv_set", {"config": cfg({}), "key": "k", "env_name": "E"},
    {"_extra_or_secret": ""})
add("_extra_csv_set", {"config": cfg({}), "key": "k", "env_name": "E"},
    {"_extra_or_secret": None})
add("_extra_csv_set", {"config": cfg({}), "key": "k", "env_name": "E"},
    {"_extra_or_secret": rs("ValueError", "bad")})
add("_extra_csv_set", {"config": cfg({}), "key": "k", "env_name": "E"}, fc=True)

# ── _scoped_recovery_key ──
add("_scoped_recovery_key", {}, {"_get_scoped_secret": " s3cr3t "})
add("_scoped_recovery_key", {}, {"_get_scoped_secret": "abc"})
add("_scoped_recovery_key", {}, {"_get_scoped_secret": ""})
add("_scoped_recovery_key", {}, {"_get_scoped_secret": None})
add("_scoped_recovery_key", {}, {"_get_scoped_secret": rs("SystemExit", "bye")})
add("_scoped_recovery_key", {}, fc=True)

# ── _redact_url_for_log ──
add("_redact_url_for_log", {"url": "https://example.org/path?token=abc#frag"},
    {"urlsplit": ad({"scheme": "https", "netloc": "example.org", "path": "/path",
                     "query": "token=abc", "fragment": "frag"}),
     "urlunsplit": "https://example.org/path"})
add("_redact_url_for_log", {"url": "relative?x=1#f"},
    {"urlsplit": ad({"scheme": "", "netloc": "", "path": "relative",
                     "query": "x=1", "fragment": "f"})})
add("_redact_url_for_log", {"url": "?x=1#f"},
    {"urlsplit": ad({"scheme": "", "netloc": "", "path": "", "query": "x=1", "fragment": "f"})})
add("_redact_url_for_log", {"url": "https://example.org/p"},
    {"urlsplit": rs("ValueError", "bad url")})
add("_redact_url_for_log", {"url": "https://example.org/p"},
    {"urlsplit": rs("SystemExit", "bye")})
add("_redact_url_for_log", {"url": "https://example.org/p"},
    {"urlsplit": ad({"scheme": "https", "netloc": "example.org", "path": "/p"}),
     "urlunsplit": rs("RuntimeError", "join failed")})
add("_redact_url_for_log", {"url": "https://example.org/p"}, fc=True)

# ── matrix_deps_present ──
add("matrix_deps_present", {}, {"is_available": True})
add("matrix_deps_present", {}, {"is_available": False})
add("matrix_deps_present", {}, {"is_available": "yes"})
add("matrix_deps_present", {}, {"is_available": rs("ValueError", "boom")})
add("matrix_deps_present", {}, {"is_available": rs("SystemExit", "bye")})
add("matrix_deps_present", {}, fc=True)

# ── check_matrix_requirements ──
add("check_matrix_requirements", {},
    {"_get_scoped_secret": ["", "", ""]})
add("check_matrix_requirements", {},
    {"_get_scoped_secret": ["tok", "", ""]})
add("check_matrix_requirements", {},
    {"_get_scoped_secret": ["tok", "", "hs"], "ensure_matrix_deps": True})
add("check_matrix_requirements", {},
    {"_get_scoped_secret": ["", "pw", "hs"], "ensure_matrix_deps": False})
add("check_matrix_requirements", {},
    {"_get_scoped_secret": rs("SystemExit", "bye")})
add("check_matrix_requirements", {},
    {"_get_scoped_secret": ["tok", "", "hs"], "ensure_matrix_deps": rs("ValueError", "bad")})
add("check_matrix_requirements", {}, fc=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
