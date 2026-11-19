#!/usr/bin/env python3
"""Generate tests/fixtures/matrix_adapter_flow.jsonl — input cases for the
Matrix-adapter FLOW oracle (the oracle computes want from LIVE adapter.py at
gate time; this script only emits {func, in} inputs).

Deterministic handcrafted branch-coverage cases. Seam scripts for
mimetypes.guess_type / urlsplit / urlunsplit are computed from the REAL stdlib
so both the C universal seam and the oracle recorders consume identical
values. Domains: ASCII strip/lower corpus; int/float parsers without
underscores; _content_bytes without floats; bare-filename Path corpus.
"""
import json
import mimetypes
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

OUT = Path(__file__).resolve().parent / "fixtures" / "matrix_adapter_flow.jsonl"
CASES = []


def add(func, args, io=None, state=None, fc=False):
    inp = {"args": args}
    if io:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    case = {"func": func, "in": inp}
    if fc:
        case["fc"] = True  # intentional fail-closed case (selftest skips gap check)
    CASES.append(case)


def fl(v):
    return {"__float__": True, "v": v}


def raise_spec(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def guess_script(candidate):
    t, enc = mimetypes.guess_type(candidate)
    # list-valued seam return -> wrap one level deeper (bare array = per-call queue)
    return {"mimetypes.guess_type": [[t, enc]]}


def url_scripts(url):
    p = urlsplit(url)
    io = {"urlsplit": {"scheme": p.scheme, "netloc": p.netloc, "path": p.path,
                       "query": p.query, "fragment": p.fragment}}
    if p.scheme or p.netloc:
        io["urlunsplit"] = urlunsplit((p.scheme, p.netloc, p.path, "", ""))
    return io


# ── _utf8_len ──
for t in ["", "hello world", "한국어 라벨", "a中é😀b", "abc", "\t\n"]:
    add("_utf8_len", {"text": t})

# ── _content_bytes (no floats) ──
for c in [{"msgtype": "m.text", "body": "hi"}, {"a": "é😀"},
          {"n": 123, "b": True, "z": None}, {"s": "a\"b\\c\nd"}, {},
          {"nested": {"x": [1, 2, "三"]}, "flag": False}]:
    add("_content_bytes", {"content": c})

# ── _csv_set ──
for raw in ["a, b ,c", ["a", " b ", "", None, 3], "", None, "  ", "a,,b", 5,
            ["x", "x", " y "]]:
    add("_csv_set", {"raw": raw})

# ── _thread_root ──
for rt in [{"rel_type": "m.thread", "event_id": "$root"},
           {"rel_type": "m.replace", "event_id": "$x"}, {},
           {"rel_type": "m.thread"}, {"rel_type": "m.thread", "event_id": None},
           {"event_id": "$x"}]:
    add("_thread_root", {"relates_to": rt})

# ── _normalize_e2ee_mode ──
for v in [None, "", "REQUIRED", " require ", "true", "1", "yes", "on", "prefer",
          "preferred", "optional", "off", "bogus", 0, False, True, 1]:
    add("_normalize_e2ee_mode", {"value": v})

# ── reply fallback trio ──
BODIES = ["", "> quoted\n\nreply", "> <@alice:ex.org> hi\n> more\n\nactual",
          "> a\n> b", "> only", "> x\nreply", "plain", "> <@a:b>",
          ">  <@a:b>  hi\n> t\n\nreply", "> q\n>\n\nreply"]
for b in BODIES:
    add("_extract_reply_fallback", {"body": b})
    add("_strip_reply_fallback", {"body": b})
    add("_split_reply_fallback", {"body": b})

# ── _is_permanent_matrix_auth_error ──
for exc in [{"errcode": "m_unknown_token"}, {"errcode": " M_Forbidden "},
            {"errcode": "m_limit_exceeded"}, {"http_status": 401},
            {"http_status": 403}, {"http_status": 500},
            {"http_status": fl(401)}, {"http_status": True}, {},
            {"errcode": "m_forbidden", "http_status": 500},
            {"errcode": None, "http_status": 403}]:
    add("_is_permanent_matrix_auth_error", {"exc": exc})

# ── _matrix_event_timestamp_seconds ──
for ev in [{"timestamp": None}, {"timestamp": 0}, {"timestamp": 1700000000},
           {"timestamp": 1700000000000}, {"timestamp": "1700000000000"},
           {"timestamp": "abc"}, {"server_timestamp": 1700000000},
           {"timestamp": 0, "server_timestamp": 5000}, {},
           {"timestamp": 10000000001}]:
    add("_matrix_event_timestamp_seconds", {"event": ev})

# ── _redact_url_for_log ──
for u in ["https://h/m?x=1#f", "plain?x#y", "https://h/p", "://bad", "mxc://h/m",
          "", "https://u:***@h:8448/p?q#r", "//h/p", "HTTPS://H/P"]:
    add("_redact_url_for_log", {"url": u}, url_scripts(u))
add("_redact_url_for_log", {"url": "https://h/p"})  # fail-closed: no urlsplit script
add("_redact_url_for_log", {"url": "https://h/p"},
    {"urlsplit": raise_spec("SystemExit", "bye")})  # BaseException passthrough
add("_redact_url_for_log", {"url": "https://h/p"},
    {"urlsplit": raise_spec("ValueError", "bad url")})  # swallowed -> <url>

# ── _looks_like_transport_filename ──
IMG = [".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".svg", ".heic", ".heif", ".avif"]
MED = [".ogg", ".oga", ".opus", ".m4a", ".mp3", ".wav", ".flac", ".aac", ".amr",
       ".mp4", ".webm", ".mov", ".mkv"]
for text, pref, exts, rej in [
        ("photo.jpg", ["image/"], IMG, False),
        ("a b.jpg", ["image/"], IMG, False),
        ("dir/photo.jpg", ["image/"], IMG, False),
        ("clip.mp3", ["audio/", "video/"], MED, True),
        ("a b.mp3", ["audio/", "video/"], MED, True),
        ("clip.txt", ["audio/", "video/"], MED, True),
        ("noext", ["image/"], IMG, False),
        ("photo/", ["image/"], IMG, False)]:
    add("_looks_like_transport_filename",
        {"text": text, "mime_prefixes": pref, "exts": exts, "reject_spaces": rej},
        guess_script(text.strip()))

# ── _looks_like_matrix_image_filename ──
for t in ["photo.jpg", "photo.PNG", "a b.jpg", "dir/photo.jpg", "photo.jpg\n",
          "photo/", "noext", "photo.svg", "clip.mp4", "a.txt", ""]:
    add("_looks_like_matrix_image_filename", {"text": t}, guess_script(t.strip()))

# ── _looks_like_matrix_media_filename ──
for t in ["clip.mp3", "clip.mp4", "a b.mp3", "clip.opus", "clip.wav", "clip.flac",
          "clip.txt"]:
    add("_looks_like_matrix_media_filename", {"text": t}, guess_script(t.strip()))

# ── _is_bare_media_filename ──
for mt, b in [("m.image", "p.jpg"), ("m.audio", "c.mp3"), ("m.file", "c.mp3"),
              ("m.video", "c.mp4"), ("m.text", "p.jpg"), ("m.image", "caption here"),
              ("m.audio", "a b.mp3")]:
    add("_is_bare_media_filename", {"msgtype": mt, "body": b}, guess_script(b.strip()))

# ── _env_truthy ──
for sc in ["true", "1", "YES", "false", "0", None]:
    add("_env_truthy", {"name": "X", "default": ""}, {"get_scoped_secret": sc})
add("_env_truthy", {"name": "X", "default": "yes"}, {"get_scoped_secret": None})
add("_env_truthy", {"name": "X", "default": ""}, fc=True)  # fail-closed (propagates)

# ── _env_number ──
for cast, sc, df in [("float", "3.5", 60), ("int", "42", 300),
                     ("float", "abc", 60), ("int", "abc", 300),
                     ("float", None, 60), ("int", "", 7),
                     ("float", "  2.5  ", 1), ("int", "  9  ", 0)]:
    add("_env_number", {"name": "N", "default": df, "cast": cast},
        {"get_scoped_secret": sc})
add("_env_number", {"name": "N", "default": 60, "cast": "float"},
    {"get_scoped_secret": raise_spec("SystemExit", "bye")})

# ── _scoped_recovery_key ──
for sc in ["  key  ", "", None, "key", "  "]:
    add("_scoped_recovery_key", {}, {"get_scoped_secret": sc})

# ── _extra_csv_set ──
for sc in ["a,b", ["x", " y "], "", None, "a,,b"]:
    io_val = [sc] if isinstance(sc, list) else sc  # list return -> wrap (queue semantics)
    add("_extra_csv_set", {"extra": {}, "key": "allowed_rooms",
                           "env_name": "MATRIX_ALLOWED_ROOMS"},
        {"extra_or_secret": io_val})

# ── _resolve_e2ee_mode ──
for extra, sc in [({"e2ee_mode": "required"}, None), ({"e2ee_mode": "prefer"}, None),
                  ({}, "true"), ({}, ""), ({"encryption": True}, ""),
                  ({"encryption": False}, ""), ({"encryption": "false"}, ""),
                  (None, ""), ({"e2ee_mode": ""}, "optional")]:
    io = {} if sc is None else {"get_scoped_secret": sc}
    add("_resolve_e2ee_mode", {"extra": extra}, io)

# ── _resolve_matrix_bang_command ──
for nm, known, skills in [("Help", True, {}), ("new", False, {"/new": 1}),
                          ("my_cmd", False, {"/my-cmd": 1}), ("x", False, {}),
                          ("", True, {}), ("a_b", False, {})]:
    add("_resolve_matrix_bang_command", {"name": nm},
        {"is_gateway_known_command": known, "get_skill_commands": skills})
add("_resolve_matrix_bang_command", {"name": "help"},
    {"is_gateway_known_command": raise_spec("RuntimeError", "boom"),
     "get_skill_commands": {"/help": 1}})  # first try swallowed -> skill hit
add("_resolve_matrix_bang_command", {"name": "help"},
    {"is_gateway_known_command": raise_spec("SystemExit", "bye")})  # base passthrough
add("_resolve_matrix_bang_command", {"name": "help"},
    {"is_gateway_known_command": False,
     "get_skill_commands": raise_spec("KeyboardInterrupt", "stop")})

# ── _normalize_matrix_bang_command ──
for txt, known, skills in [("!help", True, {}), ("!help arg", True, {}),
                           ("!new x", False, {"/new": 1}), ("hello", True, {}),
                           ("!bad", False, {}), ("!my_cmd z", False, {"/my-cmd": 1}),
                           ("!123", True, {}), ("!help!x", True, {}), ("!", True, {}),
                           ("!help\targ", True, {})]:
    add("_normalize_matrix_bang_command", {"text": txt},
        {"is_gateway_known_command": known, "get_skill_commands": skills})

# ── _resolve_max_message_length ──
for sc, reg in [(None, None), ("8000", None), ("abc", None),
                ("", {"max_message_length": 9000}), ("  ", None),
                (None, {"max_message_length": 20000}), ("100", None),
                ("20000", None)]:
    io = {"extra_or_secret": sc, "platform_registry.get": reg}
    add("_resolve_max_message_length", {"extra": {}}, io)
add("_resolve_max_message_length", {"extra": {}},
    {"extra_or_secret": None, "platform_registry.get": raise_spec("SystemExit", "bye")})
add("_resolve_max_message_length", {"extra": {}},
    {"extra_or_secret": None, "platform_registry.get": raise_spec("RuntimeError", "x")})

# ── _is_self_sender ──
for uid, sender in [("@Bot:Ex.org", "@bot:ex.org"), ("@Bot:Ex.org", "@other:ex.org"),
                    ("@Bot:Ex.org", "  @BOT:EX.ORG  "), ("", "@any:ex.org"),
                    ("@bot:ex.org", ""), (None, "@x:ex.org")]:
    add("_is_self_sender", {"self": {"_user_id": uid}, "sender": sender})

# ── _is_system_or_bridge_sender ──
for snd in ["@_telegram_123:srv", "@alice:srv", "@:srv", "", "  @_x:srv  ",
            "_plain", "@@_double:srv"]:
    add("_is_system_or_bridge_sender", {"sender": snd})

# ── _user_localpart ──
for uid in ["@bot:server", "bot:server", "@bot", "bot", "", None, "@a:b:c"]:
    add("_user_localpart", {"self": {"_user_id": uid}})

# ── _mxc_to_http ──
for u in ["mxc://h/media123", "https://h/x", "mxc://", "mxc://h/a/b", "notmxc"]:
    add("_mxc_to_http", {"self": {"_homeserver": "https://h"}, "mxc_url": u})

# ── _sanitize_link_url ──
for u in ["javascript:alert(1)", "  DATA:text/html,x  ", "https://ok", "vbscript:x",
          'https://a"b', "http://x", "  javascript :x", "ftp://ok"]:
    add("_sanitize_link_url", {"url": u})

# ── _state_event_value ──
for ev, key, attr in [({"name": "Room"}, "name", False),
                      ({"content": {"name": "Inner"}}, "name", False),
                      ({"name": "", "content": {"name": "X"}}, "name", False),
                      (None, "name", False), ({"name": 0}, "name", False),
                      ({"name": False}, "name", False), ({"name": 123}, "name", False),
                      ({"name": "AttrName"}, "name", True),
                      ({"content": {"name": "C"}}, "name", True)]:
    add("_state_event_value", {"event": ev, "key": key, "event_attr": attr})

# ── _extract_server_ed25519 ──
for dk in [{"keys": {"ed25519:ABC": "keyval", "curve25519:X": "other"}},
           {"keys": {}}, {"keys": None},
           {"keys": {"curve25519:X": "o", "ed25519:Z": "v"}},
           {"keys": {"ed25519:A": 123}}]:
    add("_extract_server_ed25519", {"device_keys": dk})

# ── _configured_bool ──
for v in [None, True, False, "true", "off", "FALSE", "0", "no", "yes", "", 1, 0,
          [], [1]]:
    add("_configured_bool", {"extra": {"k": v}, "key": "k"})
add("_configured_bool", {"extra": {}, "key": "k"})

# ── _media_too_large ──
for size, mmb in [(200000000, 104857600), (1, 100), (0, 0), (101, 100),
                  (999999999999, 104857600)]:
    add("_media_too_large", {"self": {"_max_media_bytes": mmb}, "size": size})

# ── _matrix_prompt_expired ──
for exp, mono in [(None, 100.0), (50.0, 100.0), (100.0, 100.0), (150.0, 100.0),
                  (99.5, 100.0)]:
    add("_matrix_prompt_expired", {"prompt": {"expires_at": exp}},
        {"time.monotonic": mono})
add("_matrix_prompt_expired", {"prompt": {"expires_at": 50.0}},
    {"time.monotonic": raise_spec("SystemExit", "bye")})

# ── _apply_relation_metadata ──
for reply, meta, base in [("$ev", None, {}), ("$ev", {"thread_id": "$th"}, {}),
                          (None, {"thread_id": "$th"}, {}), (None, None, {}),
                          ("$ev", {"thread_id": "$th"},
                           {"m.relates_to": {"m.in_reply_to": {"event_id": "$pre"}}}),
                          (None, {"thread_id": ""}, {}),
                          ("$ev", {"thread_id": "$th"}, {"body": "hi"})]:
    add("_apply_relation_metadata",
        {"msg_content": base, "reply_to": reply, "metadata": meta})

# ── _is_duplicate_event ──
add("_is_duplicate_event", {"event_id": "a", "maxlen": 3},
    state={"processed_events": []})
add("_is_duplicate_event", {"event_id": "a", "maxlen": 3},
    state={"processed_events": ["a", "b"]})
add("_is_duplicate_event", {"event_id": "d", "maxlen": 3},
    state={"processed_events": ["a", "b", "c"]})
add("_is_duplicate_event", {"event_id": "a", "maxlen": 3},
    state={"processed_events": ["b", "c", "d"]})
add("_is_duplicate_event", {"event_id": None, "maxlen": 3},
    state={"processed_events": ["a"]})
add("_is_duplicate_event", {"event_id": "", "maxlen": 3},
    state={"processed_events": ["a"]})
add("_is_duplicate_event", {"event_id": "x", "maxlen": 1},
    state={"processed_events": ["y"]})

# ── _invalidate_room_identities ──
add("_invalidate_room_identities", {"room_id": "r1"},
    state={"room_identities": {"r1": 1, "r2": 2},
           "room_identity_cached_at": {"r1": 10, "r2": 20}})
add("_invalidate_room_identities", {"room_id": None},
    state={"room_identities": {"r1": 1, "r2": 2},
           "room_identity_cached_at": {"r1": 10, "r2": 20}})
add("_invalidate_room_identities", {"room_id": "rX"},
    state={"room_identities": {"r1": 1}, "room_identity_cached_at": {"r1": 10}})
add("_invalidate_room_identities", {"room_id": None},
    state={"room_identities": {}, "room_identity_cached_at": {}})
add("_invalidate_room_identities", {"room_id": "r1"},
    state={"room_identities": {"r1": 1}, "room_identity_cached_at": {}})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {OUT} : {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 5]
    if low:
        print("WARNING funcs with <5 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()