#!/usr/bin/env python3
"""Generate tests/fixtures/buzz_adapter_flow2.jsonl for the Buzz adapter FLOW
remainder port. Deterministic; >=5 cases per ported function. Honours the
corpus restrictions documented in src/gateway/platforms/buzz_adapter_flow2.c
(ASCII scope, pre-normalized POSIX paths, canonical json.dumps JSON text,
scalar str()-positions, parsed doubles computed with the REAL urlsplit).

Run: python tests/gen_buzz_adapter_flow2_cases.py
"""
import json
import os
import urllib.parse as up

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "buzz_adapter_flow2.jsonl")

H = "a" * 64                       # 64-char lowercase hex
PK = "ab" * 32
OTHER = "cd" * 32
RELAY = "https://relay.example.com"

CASES = []

# Seams whose LIVE result is itself a list/tuple: a bare JSON-array script is a
# SEQUENCE (one result per call), so a single list result must be nested one
# level deeper ([result]). A sequence of list results already has list elems.
LIST_RESULT_SEAMS = {"_exec_buzz", "_parse_send_receipt", "_parse_json_list"}


def norm_io(io):
    if not io:
        return io
    out = {}
    for k, v in io.items():
        if k in LIST_RESULT_SEAMS and isinstance(v, list):
            if len(v) == 0 or not isinstance(v[0], list):
                v = [v]
        out[k] = v
    return out


def case(func, args, state=None, io=None):
    inp = {"args": args}
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = norm_io(io)
    CASES.append({"func": func, "in": inp})


def raise_spec(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def parsed(url):
    """Model urllib.parse.urlsplit as a parsed-double dict (port_error for the
    lazy .port ValueError; a split-time ValueError becomes a raise spec)."""
    try:
        p = up.urlsplit(url)
    except ValueError as e:
        return raise_spec("ValueError", str(e))
    d = {"scheme": p.scheme, "netloc": p.netloc, "path": p.path,
         "query": p.query, "fragment": p.fragment, "hostname": p.hostname,
         "username": p.username, "password": p.password}
    try:
        d["port"] = p.port
    except ValueError as e:
        d["port"] = None
        d["port_error"] = str(e)
    return d


def media_url(host="relay.example.com", sha=H, ext="", query="", scheme="https"):
    return f"{scheme}://{host}/media/{sha}{ext}{query}"


def cli_state(**over):
    base = {"_private_key": "pk", "_auth_tag": "", "cli_path": "/usr/bin/buzz",
            "relay_url": RELAY, "_extra": {}}
    base.update(over)
    return base


def imeta_tag(url=None, sha=H, size="100", filename="a.png", mime="image/png", extra=None):
    url = url if url is not None else media_url()
    fields = [f"url {url}", f"x {sha}", f"size {size}"]
    if filename is not None:
        fields.append(f"filename {filename}")
    if mime is not None:
        fields.append(f"m {mime}")
    if extra:
        fields.extend(extra)
    return ["imeta"] + fields


# ── _get_scoped_secret ──
case("_get_scoped_secret", {"name": "BUZZ_PRIVATE_KEY", "default": ""}, io={"_shared_scoped_secret": "nsec1x"})
case("_get_scoped_secret", {"name": "X"}, io={"_shared_scoped_secret": None})
case("_get_scoped_secret", {"name": "X", "default": "d"}, io={"_shared_scoped_secret": raise_spec("SystemExit", "bye")})
case("_get_scoped_secret", {"name": "X", "default": ""}, io={})
case("_get_scoped_secret", {"name": "X", "default": 5}, io={"_shared_scoped_secret": 7})

# ── _escape_unresolved_presentation_mention ──
ERR_FIZZ = "mention '@Fizz' does not match a current channel member"
case("_escape_unresolved_presentation_mention", {"content": "hi @Fizz!", "error": ERR_FIZZ})
case("_escape_unresolved_presentation_mention", {"content": "hi @Fizz!", "error": None})
case("_escape_unresolved_presentation_mention", {"content": "hi @Fizz!", "error": "no match here"})
case("_escape_unresolved_presentation_mention", {"content": "x@Fizz", "error": ERR_FIZZ})
case("_escape_unresolved_presentation_mention", {"content": "@Fizz @fizz", "error": ERR_FIZZ})
case("_escape_unresolved_presentation_mention", {"content": "@FizzBuzz", "error": ERR_FIZZ})
case("_escape_unresolved_presentation_mention", {"content": "@Foo Bar.", "error": "mention '@Foo Bar' does not match a current channel member"})
case("_escape_unresolved_presentation_mention", {"content": "@Fizz", "error": "mention '@' does not match a current channel member"})

# ── _safe_attachment_filename ──
for v in ["photo.png", "../../etc/passwd", "a\\b\\c.txt", "", "..", "  .  ",
          "x" * 130 + ".png", "file.tar.gz", "con.", ".hidden", "a\x01b\x7fc.txt",
          "x.abcdefghijklmnopqrstu"]:
    case("_safe_attachment_filename", {"value": v})

# ── _attachment_origin ──
case("_attachment_origin", {"value": "https://Media.Example.com/"}, io={"urlsplit": parsed("https://Media.Example.com/")})
case("_attachment_origin", {"value": "media.example.com"}, io={"urlsplit": parsed("//media.example.com")})
case("_attachment_origin", {"value": "https://h:8443/"}, io={"urlsplit": parsed("https://h:8443/")})
case("_attachment_origin", {"value": "http://h/"}, io={"urlsplit": parsed("http://h/")})
case("_attachment_origin", {"value": ""}, io={})
case("_attachment_origin", {"value": "https://h:abc/"}, io={"urlsplit": parsed("https://h:abc/")})
case("_attachment_origin", {"value": "wss://h."}, io={"urlsplit": parsed("wss://h.")})
case("_attachment_origin", {"value": "https://[::1"}, io={"urlsplit": parsed("https://[::1")})
case("_attachment_origin", {"value": "https://h/"}, io={})

# ── _effective_port ──
case("_effective_port", {"parsed": {"port": 8443, "scheme": "https"}})
case("_effective_port", {"parsed": {"port": None, "scheme": "https"}})
case("_effective_port", {"parsed": {"port": None, "scheme": "http"}})
case("_effective_port", {"parsed": {"port": None, "scheme": "ws"}})
case("_effective_port", {"parsed": {"port": None, "scheme": "ftp"}})
case("_effective_port", {"parsed": {"port": None, "port_error": "Port could not be cast to integer value as 'abc'", "scheme": "https"}})
case("_effective_port", {"parsed": {"port": 0, "scheme": "https"}})

# ── _is_relay_media_url ──
case("_is_relay_media_url", {"url": media_url(), "relay_url": RELAY}, io={"urlsplit": [parsed(media_url()), parsed(RELAY)]})
case("_is_relay_media_url", {"url": media_url(host="other.com"), "relay_url": RELAY}, io={"urlsplit": [parsed(media_url(host="other.com")), parsed(RELAY)]})
case("_is_relay_media_url", {"url": media_url() + ":443", "relay_url": RELAY + ":8443"}, io={"urlsplit": [parsed("https://relay.example.com:443/media/" + H), parsed(RELAY + ":8443")]})
case("_is_relay_media_url", {"url": "https://relay.example.com/other/" + H, "relay_url": RELAY}, io={"urlsplit": [parsed("https://relay.example.com/other/" + H), parsed(RELAY)]})
case("_is_relay_media_url", {"url": media_url(scheme="ftp"), "relay_url": RELAY}, io={"urlsplit": [parsed(media_url(scheme="ftp")), parsed(RELAY)]})
case("_is_relay_media_url", {"url": media_url(sha=H + "a"), "relay_url": RELAY}, io={"urlsplit": [parsed(media_url(sha=H + "a")), parsed(RELAY)]})
case("_is_relay_media_url", {"url": media_url(ext=".png"), "relay_url": "wss://relay.example.com"}, io={"urlsplit": [parsed(media_url(ext=".png")), parsed("wss://relay.example.com")]})
case("_is_relay_media_url", {"url": media_url(), "relay_url": RELAY}, io={})

# ── _find_relay_media_refs ──
MU = media_url()
case("_find_relay_media_refs", {"text": f"![alt]({MU})", "relay_url": RELAY}, io={"urlsplit": [parsed(MU), parsed(RELAY)]})
case("_find_relay_media_refs", {"text": MU, "relay_url": RELAY}, io={"urlsplit": [parsed(MU), parsed(RELAY)]})
case("_find_relay_media_refs", {"text": f"![a]({MU}) and {MU}", "relay_url": RELAY}, io={"urlsplit": [parsed(MU), parsed(RELAY), parsed(MU), parsed(RELAY)]})
case("_find_relay_media_refs", {"text": f"![x]({media_url(host='other.com')})", "relay_url": RELAY}, io={"urlsplit": [parsed(media_url(host="other.com")), parsed(RELAY), parsed(media_url(host="other.com")), parsed(RELAY)]})
case("_find_relay_media_refs", {"text": "no media here", "relay_url": RELAY}, io={})
case("_find_relay_media_refs", {"text": f"![  alt  ]({MU}) ![b]({MU})", "relay_url": RELAY}, io={"urlsplit": [parsed(MU), parsed(RELAY), parsed(MU), parsed(RELAY)]})
case("_find_relay_media_refs", {"text": f"![x]({MU}.png?q=1)", "relay_url": RELAY}, io={"urlsplit": [parsed(MU + ".png?q=1"), parsed(RELAY)]})

# ── _replace_media_refs ──
case("_replace_media_refs", {"text": "a X b", "replacements": [[2, 3, "Y"]]})
case("_replace_media_refs", {"text": "hello", "replacements": []})
case("_replace_media_refs", {"text": "a  \n  \nb\n\n\n\nc", "replacements": []})
case("_replace_media_refs", {"text": "abcde", "replacements": [[0, 2, "Z"], [3, 5, "W"]]})
case("_replace_media_refs", {"text": "  x  ", "replacements": []})
case("_replace_media_refs", {"text": "a X b Y c", "replacements": [[2, 3, ""], [6, 7, ""]]})
case("_replace_media_refs", {"text": "line\t\nnext", "replacements": []})

# ── hex_to_npub ──
for s in [PK, OTHER, "ab" * 31, "ab" * 33, "zz" * 32, "a", "ab cd" * 16, ""]:
    case("hex_to_npub", {"pubkey_hex": s})

# ── _resolve_cli_path ──
case("_resolve_cli_path", {"configured": "/usr/bin/buzz"}, io={"expanduser": "/usr/bin/buzz", "is_file": True})
case("_resolve_cli_path", {"configured": "~/bin/buzz"}, io={"expanduser": "/home/u/bin/buzz", "is_file": False})
case("_resolve_cli_path", {"configured": ""}, io={"shutil.which": "/usr/local/bin/buzz"})
case("_resolve_cli_path", {"configured": ""}, io={"shutil.which": None, "Path.home": "/home/u", "is_file": True})
case("_resolve_cli_path", {"configured": ""}, io={"shutil.which": None, "Path.home": "/home/u", "is_file": False})
case("_resolve_cli_path", {"configured": ""}, io={})
case("_resolve_cli_path", {"configured": "/x"}, io={"expanduser": raise_spec("SystemExit", "bye")})

# ── _configured_cli_path ──
case("_configured_cli_path", {"extra": {}}, io={"_scoped_platform_setting": "/usr/bin/buzz", "expanduser": "/usr/bin/buzz", "is_file": True})
case("_configured_cli_path", {"extra": {"cli_path": "/opt/buzz"}}, io={"_scoped_platform_setting": None, "expanduser": "/opt/buzz", "is_file": True})
case("_configured_cli_path", {"extra": {}}, io={"_scoped_platform_setting": None, "shutil.which": "/usr/bin/buzz"})
case("_configured_cli_path", {"extra": None}, io={"_scoped_platform_setting": None})
case("_configured_cli_path", {"extra": {"cli_path": ""}}, io={"_scoped_platform_setting": "  ", "shutil.which": None, "Path.home": "/h", "is_file": False})
case("_configured_cli_path", {"extra": {}}, io={})

# ── _resolve_auth_tag ──
case("_resolve_auth_tag", {"extra": {}}, io={"_shared_scoped_secret": "tagraw", "parse_auth_tag": {"a": 1}})
case("_resolve_auth_tag", {"extra": {}}, io={"_shared_scoped_secret": [None, "pk"], "_configured_credentials_file": "", "_resolve_credentials_data": {"auth_tag": "t"}, "parse_auth_tag": [[1, 2]]})
case("_resolve_auth_tag", {"extra": {}}, io={"_shared_scoped_secret": [None, None], "_resolve_credentials_data": {"auth_tag": {"k": "v"}}, "parse_auth_tag": {"ok": True}})
case("_resolve_auth_tag", {"extra": {}}, io={"_shared_scoped_secret": [None, None], "_resolve_credentials_data": {}})
case("_resolve_auth_tag", {"extra": {}}, io={"_shared_scoped_secret": ["  ", "pk"], "_configured_credentials_file": "/c.json", "_resolve_credentials_data": {}})
case("_resolve_auth_tag", {"extra": {}}, io={"_shared_scoped_secret": "t", "parse_auth_tag": raise_spec("ValueError", "bad")})
case("_resolve_auth_tag", {"extra": {}}, io={"_shared_scoped_secret": "t", "parse_auth_tag": raise_spec("SystemExit", "bye")})
case("_resolve_auth_tag", {"extra": {}}, io={})

# ── _consume_ws_read_task ──
case("_consume_ws_read_task", {}, io={"task.cancelled": True})
case("_consume_ws_read_task", {}, io={"task.cancelled": False, "task.exception": None})
case("_consume_ws_read_task", {}, io={"task.cancelled": False, "task.exception": raise_spec("ValueError", "x")})
case("_consume_ws_read_task", {}, io={"task.cancelled": False, "task.exception": raise_spec("SystemExit", "bye")})
case("_consume_ws_read_task", {}, io={"task.cancelled": False, "task.exception": raise_spec("CancelledError", "")})
case("_consume_ws_read_task", {}, io={"task.cancelled": raise_spec("RuntimeError", "r")})
case("_consume_ws_read_task", {}, io={})

# ── _parse_imeta_attachments ──
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag()]}}, io={"urlsplit": [parsed(media_url())]})
case("_parse_imeta_attachments", {"event": {"tags": "nope"}}, io={})
case("_parse_imeta_attachments", {"event": {"tags": [["p", PK]]}}, io={})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(size="abc")]}}, io={})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(url=media_url(scheme="http"))]}}, io={"urlsplit": [parsed(media_url(scheme="http"))]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(url="https://h:abc/media/" + H)]}}, io={"urlsplit": [parsed("https://h:abc/media/" + H)]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag() for _ in range(5)]}}, io={"urlsplit": [parsed(media_url()) for _ in range(4)]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(size="20971521")]}}, io={"urlsplit": [parsed(media_url())]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(sha="zz" * 32)]}}, io={"urlsplit": [parsed(media_url())]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(size="15000000"), imeta_tag(size="15000000")]}}, io={"urlsplit": [parsed(media_url()), parsed(media_url())]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(extra=["url https://other.com/media/" + H, "size 50"])]}}, io={"urlsplit": [parsed(media_url())]})
case("_parse_imeta_attachments", {"event": {"tags": [["imeta", 5, "url " + media_url(), "x " + H, "size 100"]]}}, io={"urlsplit": [parsed(media_url())]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag(url="https://[::1")]}}, io={"urlsplit": [parsed("https://[::1")]})
case("_parse_imeta_attachments", {"event": {"tags": [imeta_tag()]}}, io={"urlsplit": [raise_spec("SystemExit", "bye")]})

# ── _imeta_attachments ──
case("_imeta_attachments", {"event": {"tags": [imeta_tag()]}}, io={"urlsplit": [parsed(media_url())]})
case("_imeta_attachments", {"event": {"tags": None}}, io={})
case("_imeta_attachments", {"event": {"tags": [imeta_tag(size="abc")]}}, io={})
case("_imeta_attachments", {"event": {"tags": [imeta_tag(), imeta_tag(filename="b.jpg", mime="image/jpeg")]}}, io={"urlsplit": [parsed(media_url()), parsed(media_url())]})
case("_imeta_attachments", {"event": {"tags": [imeta_tag(url=media_url(scheme="http"))]}}, io={"urlsplit": [parsed(media_url(scheme="http"))]})

# ── _websocket_url ──
case("_websocket_url", {}, state={"relay_url": "https://h/path?q=1"}, io={"urlsplit": parsed("https://h/path?q=1"), "urlunsplit": "wss://h/path?q=1"})
case("_websocket_url", {}, state={"relay_url": "http://h"}, io={"urlsplit": parsed("http://h"), "urlunsplit": "ws://h"})
case("_websocket_url", {}, state={"relay_url": "ftp://h"}, io={"urlsplit": parsed("ftp://h")})
case("_websocket_url", {}, state={"relay_url": "wss://"}, io={"urlsplit": parsed("wss://")})
case("_websocket_url", {}, state={}, io={})
case("_websocket_url", {}, state={"relay_url": "  https://h  "}, io={"urlsplit": parsed("https://h"), "urlunsplit": "wss://h"})
case("_websocket_url", {}, state={"relay_url": "https://h"}, io={})

# ── _send_result ──
case("_send_result", {"chat_id": "c1", "code": 1, "out": "", "err": "e"}, io={"_cli_error_message": "label: e (exit 1)"})
case("_send_result", {"chat_id": "c1", "code": 2, "out": "", "err": "e"}, io={"_cli_error_message": "m"})
case("_send_result", {"chat_id": "c1", "code": 0, "out": "ok", "err": ""}, io={"_parse_send_receipt": ["ev1", None], "self._mark_seen": None})
case("_send_result", {"chat_id": "c1", "code": 0, "out": "ok", "err": ""}, io={"_parse_send_receipt": [None, "invalid CLI response"]})
case("_send_result", {"chat_id": "c1", "code": 0, "out": "ok", "err": ""}, io={"_parse_send_receipt": [None, None]})
case("_send_result", {"chat_id": "c1", "code": 0, "out": "ok", "err": ""}, io={"_parse_send_receipt": ["ev1", None], "self._mark_seen": raise_spec("SystemExit", "bye")})
case("_send_result", {"chat_id": "c1", "code": 1, "out": "", "err": "e", "redact_path": {"path": "/p/a", "name": "a"}}, io={"_cli_error_message": "m"})
case("_send_result", {"chat_id": "c1", "code": 0, "out": "ok", "err": ""}, io={})

# ── _connect_failed ──
case("_connect_failed", {"code": "config_missing", "detail": "d", "log": "Buzz: %s", "log_args": ["x"], "retryable": True}, io={"logger.error": None, "self._set_fatal_error": None})
case("_connect_failed", {"code": "c", "detail": "d", "log": "msg"}, io={"logger.error": None, "self._set_fatal_error": None})
case("_connect_failed", {"code": "c", "detail": "d", "log": "msg"}, io={"logger.error": raise_spec("SystemExit", "bye")})
case("_connect_failed", {"code": "c", "detail": "d", "log": "msg"}, io={"logger.error": None, "self._set_fatal_error": raise_spec("RuntimeError", "r")})
case("_connect_failed", {"code": "c", "detail": "d", "log": "msg"}, io={})

# ── _run_cli ──
case("_run_cli", {"args": ["users", "get"], "input_text": None}, state=cli_state(), io={"_exec_buzz": [0, "out", ""]})
case("_run_cli", {"args": ["users", "get"]}, state=cli_state(_private_key=""), io={"_resolve_private_key": "pk2", "_shared_scoped_secret": [None, None], "_resolve_credentials_data": {}, "_exec_buzz": [0, "o", ""]})
case("_run_cli", {"args": ["users", "get"]}, state={}, io={})
case("_run_cli", {"args": ["users", "get"]}, state={"_private_key": "pk"}, io={})
case("_run_cli", {"args": ["users", "get"]}, state=cli_state(), io={})
case("_run_cli", {"args": ["messages", "send"], "input_text": "body"}, state=cli_state(), io={"_exec_buzz": [0, "o", ""]})
case("_run_cli", {"args": ["users", "get"]}, state=cli_state(), io={"_exec_buzz": raise_spec("SystemExit", "bye")})

# ── _cli_json ──
case("_cli_json", {"args": ["channels", "list"], "default": []}, state=cli_state(), io={"_exec_buzz": [0, "[1]", ""], "_json_or": [[1]]})
case("_cli_json", {"args": ["channels", "list"], "default": []}, state=cli_state(), io={"_exec_buzz": [1, "", "e"]})
case("_cli_json", {"args": ["channels", "list"], "default": []}, state=cli_state(), io={"_exec_buzz": [0, "x", ""], "_json_or": raise_spec("SystemExit", "bye")})
case("_cli_json", {"args": ["channels", "list"], "default": {"a": 1}}, state=cli_state(), io={"_exec_buzz": [2, "", "e"]})
case("_cli_json", {"args": ["channels", "list"], "default": []}, state={}, io={})
case("_cli_json", {"args": ["channels", "list"], "default": []}, state=cli_state(), io={})

# ── send_reaction ──
case("send_reaction", {"chat_id": "c1", "message_id": "ev1", "emoji": "x"}, state=cli_state(), io={"_exec_buzz": [0, "", ""]})
case("send_reaction", {"chat_id": "c1", "message_id": "ev1", "emoji": "x"}, state=cli_state(cli_path=""), io={})
case("send_reaction", {"chat_id": "c1", "message_id": "ev1", "emoji": ""}, state=cli_state(), io={})
case("send_reaction", {"chat_id": "c1", "message_id": "", "emoji": "x"}, state=cli_state(), io={})
case("send_reaction", {"chat_id": "c1", "message_id": "ev1", "emoji": "x"}, state=cli_state(), io={"_exec_buzz": [1, "", "err"], "_cli_error_message": "msg", "logger.debug": None})
case("send_reaction", {"chat_id": "c1", "message_id": "ev1", "emoji": "x"}, state={}, io={})
case("send_reaction", {"chat_id": "c1", "message_id": "ev1", "emoji": "x"}, state=cli_state(), io={})

# ── edit_message ──
EDIT_OK = {"event_id": "e2", "accepted": True}
case("edit_message", {"chat_id": "c1", "message_id": "", "content": "x"}, state=cli_state(), io={})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": ""}, state=cli_state(), io={})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": "x"}, state=cli_state(), io={"_exec_buzz": [0, json.dumps(EDIT_OK), ""], "_json_or": EDIT_OK, "self._mark_seen": None})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": "x"}, state=cli_state(), io={"_exec_buzz": [1, "", "e"], "_cli_error_message": "m"})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": "x"}, state=cli_state(), io={"_exec_buzz": [2, "", "e"], "_cli_error_message": "m"})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": "x"}, state=cli_state(), io={"_exec_buzz": [0, "{}", ""], "_json_or": {}})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": "x"}, state=cli_state(), io={"_exec_buzz": [0, '{"accepted":false}', ""], "_json_or": {"accepted": False}})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": "x"}, state=cli_state(), io={"_exec_buzz": [0, "[1]", ""], "_json_or": [1]})
case("edit_message", {"chat_id": "c1", "message_id": "ev1", "content": "x"}, state=cli_state(), io={})

# ── delete_message ──
case("delete_message", {"chat_id": "c1", "message_id": ""}, state=cli_state(), io={})
case("delete_message", {"chat_id": "c1", "message_id": "ev1"}, state=cli_state(), io={"_exec_buzz": [1, "", ""]})
case("delete_message", {"chat_id": "c1", "message_id": "ev1"}, state=cli_state(), io={"_exec_buzz": [0, '{"event_id":"e2","accepted":true}', ""], "self._mark_seen": None})
case("delete_message", {"chat_id": "c1", "message_id": "ev1"}, state=cli_state(), io={"_exec_buzz": [0, "not json", ""]})
case("delete_message", {"chat_id": "c1", "message_id": "ev1"}, state=cli_state(), io={"_exec_buzz": [0, "", ""]})
case("delete_message", {"chat_id": "c1", "message_id": "ev1"}, state=cli_state(), io={"_exec_buzz": [0, "[1]", ""]})
case("delete_message", {"chat_id": "c1", "message_id": "ev1"}, state=cli_state(), io={"_exec_buzz": [0, '{"accepted":false}', ""]})
case("delete_message", {"chat_id": "c1", "message_id": "ev1"}, state=cli_state(), io={})

# ── get_chat_info ──
case("get_chat_info", {"chat_id": "c1"}, state=cli_state(_channel_state={"c1": {"chat_type": "dm"}}, _channel_names={"c1": "Name"}), io={})
case("get_chat_info", {"chat_id": "c1"}, state=cli_state(_channel_state={}, _channel_names={}), io={"_exec_buzz": [0, '{"name":"N"}', ""], "_json_or": {"name": "N"}})
case("get_chat_info", {"chat_id": "c1"}, state=cli_state(cli_path="", _channel_state={}, _channel_names={}), io={})
case("get_chat_info", {"chat_id": "c1"}, state=cli_state(_channel_state={"c1": {}}, _channel_names={"c1": "Name"}), io={})
case("get_chat_info", {"chat_id": "c1"}, state=cli_state(_channel_state={"c1": {"last_ts": 5}}, _channel_names={"c1": "Name"}), io={})
case("get_chat_info", {"chat_id": "c1"}, state={}, io={})
case("get_chat_info", {"chat_id": "c1"}, state=cli_state(_channel_state={}, _channel_names={}), io={"_exec_buzz": [0, "[1]", ""], "_json_or": [[1]]})

# ── _resolve_user_name ──
case("_resolve_user_name", {"pubkey": PK}, state=cli_state(_user_names={PK: "Cached"}), io={})
case("_resolve_user_name", {"pubkey": PK}, state=cli_state(_user_names={}), io={"_exec_buzz": [0, '[{"display_name":"Alice"}]', ""], "_parse_json_list": [{"display_name": "Alice"}]})
case("_resolve_user_name", {"pubkey": PK}, state=cli_state(_user_names={}), io={"_exec_buzz": [1, "", ""]})
case("_resolve_user_name", {"pubkey": "short"}, state=cli_state(_user_names={}), io={"_exec_buzz": [0, "[]", ""], "_parse_json_list": []})
case("_resolve_user_name", {"pubkey": PK}, state=cli_state(_user_names={}), io={"_exec_buzz": [0, '[{"display_name":""}]', ""], "_parse_json_list": [{"display_name": ""}]})
case("_resolve_user_name", {"pubkey": PK}, state={}, io={})
case("_resolve_user_name", {"pubkey": PK}, state=cli_state(_user_names={}), io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    low = [f for f, n in counts.items() if n < 5]
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low:
        print(f"WARNING: <5 cases: {low}")


if __name__ == "__main__":
    main()