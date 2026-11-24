#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/qqbot/adapter.py (gate w19_adapter_5d2c00)."""
import ast
import contextlib
import datetime as _dt
import json
import os as _os
import re
import sys
import types
import __future__
from pathlib import Path
from urllib.parse import urlparse

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "plugins" / "platforms" / "qqbot" / "adapter.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_adapter_5d2c00"

_WANTED = {"_parse_json", "_parse_gateway_session_key", "_is_authorized_interaction_for_session",
           "_append_block", "_merge_quote_into", "_detect_message_type", "_opt_str",
           "_is_voice_content_type", "_guess_ext_from_data", "_looks_like_silk", "_rest_path",
           "_messages_path", "_build_text_body", "_is_url", "_strip_at_mention",
           "_entry_matches", "_guess_chat_type", "_parse_qq_timestamp", "_resolve_stt_config",
           "_write_update_response", "_next_msg_seq", "_create_task"}


def _config_number(cfg, key, default, cast=float):
    try:
        return cast(cfg.get(key, default))
    except (TypeError, ValueError):
        return default


def stub_modules():
    tc = types.ModuleType("tools.transcription_common")
    tc.DEFAULT_STT_TIMEOUT = 60.0
    tc._config_number = _config_number
    sys.modules["tools"] = types.ModuleType("tools")
    sys.modules["tools.transcription_common"] = tc


class _Env:
    values = {}


class _Logger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass
    def exception(self, *a, **k): pass


class _NowCall(Exception):
    """Raised by the stubbed clock: the live fallback read datetime.now()."""


class _DT(_dt.datetime):
    @classmethod
    def now(cls, tz=None):
        raise _NowCall()


class _Msg:
    def __init__(self, value):
        self.value = value


MSG = types.SimpleNamespace(TEXT=_Msg("text"), LOCATION=_Msg("location"), PHOTO=_Msg("photo"),
                            VIDEO=_Msg("video"), AUDIO=_Msg("audio"), VOICE=_Msg("voice"),
                            DOCUMENT=_Msg("document"), STICKER=_Msg("sticker"),
                            COMMAND=_Msg("command"))


def extract():
    stub_modules()
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("qqadapter_live")
    sys.modules["qqadapter_live"] = mod
    g = mod.__dict__

    class _Base:
        MAX_MESSAGE_LENGTH = 4000

        def __init__(self, *a, **k):
            self._chat_type_map = {}
            self._dedup = None
            self._last_msg_id = {}
            self.config = types.SimpleNamespace(extra={})

    class _Mixin:
        pass

    g.update(dict(
        json=json, re=re, os=types.SimpleNamespace(getenv=lambda k, d=None: _Env.values.get(k, d)),
        urlparse=urlparse, Path=Path, datetime=_DT, timezone=_dt.timezone,
        UTC=_dt.UTC, timedelta=_dt.timedelta,
        MessageType=MSG, BasePlatformAdapter=_Base, OwnAccessPolicyMixin=_Mixin,
        MessageEvent=object, MessageDeduplicator=object, ExecApprovalPrompt=object,
        SendResult=object, PlatformConfig=object, Platform=object,
        cache_document_from_bytes_async=None, cache_image_from_bytes_async=None,
        gateway_trust_env=lambda: True, strip_markdown=lambda s: s,
        cancel_task=None, platform_http_client=None, ext_for_mime=None,
        _coerce_list=lambda v: v, build_user_agent=lambda: "ua",
        describe_connect_proxy_error=lambda *a: "",
        _resolve_qq_secret=lambda key, default: _Env.values.get(key, default),
        DEFAULT_STT_TIMEOUT=60.0, _config_number=_config_number,
        asyncio=types.SimpleNamespace(get_running_loop=None, create_task=None, sleep=None),
        contextlib=contextlib, logging=types.SimpleNamespace(getLogger=lambda n: _Logger()),
        logger=_Logger(),
        httpx=None, aiohttp=None, AIOHTTP_AVAILABLE=True, HTTPX_AVAILABLE=True,
        MAX_MESSAGE_LENGTH=4000, MSG_TYPE_TEXT=0, MSG_TYPE_MARKDOWN=2, MSG_TYPE_MEDIA=7,
        MSG_TYPE_INPUT_NOTIFY=6, MEDIA_TYPE_IMAGE="image", MEDIA_TYPE_VIDEO="video",
        MEDIA_TYPE_VOICE="voice", MEDIA_TYPE_FILE="file",
        API_BASE="", TOKEN_URL="", GATEWAY_URL_PATH="", DEFAULT_API_TIMEOUT=30.0,
        FILE_UPLOAD_TIMEOUT=30.0, CONNECT_TIMEOUT_SECONDS=5.0, RECONNECT_BACKOFF=(1,),
        MAX_RECONNECT_ATTEMPTS=3, RATE_LIMIT_DELAY=1.0, QUICK_DISCONNECT_THRESHOLD=10,
        MAX_QUICK_DISCONNECT_COUNT=3, DEDUP_WINDOW_SECONDS=60, DEDUP_MAX_SIZE=100,
        ChunkedUploader=object, UploadDailyLimitExceededError=ValueError,
        UploadFileTooLargeError=ValueError, ApprovalRequest=object, InlineKeyboard=object,
        InteractionEvent=object, approval_button_request_id=lambda *a: None,
        build_approval_keyboard=lambda *a: None, build_update_prompt_keyboard=lambda *a: None,
        parse_approval_button_data=lambda *a: None, parse_interaction_event=lambda *a: None,
        parse_update_prompt_button_data=lambda *a: None,
        Any=object, Awaitable=object, Callable=object, Dict=dict, List=list,
        Optional=object, Tuple=tuple, install_hint=lambda *a: ""))
    for node in tree.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if names and names[0] in ("_VOICE_EXTENSIONS", "_STT_PROVIDER_BASE_URLS",
                                      "_AUDIO_URL_EXTENSIONS"):
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "QQAdapter":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
ADAPTER = G["QQAdapter"]


def adapter(**attrs):
    obj = ADAPTER.__new__(ADAPTER)
    obj._next_msg_seq = lambda chat: 4242
    obj._markdown_support = False
    obj._chat_type_map = {}
    obj.config = types.SimpleNamespace(extra={})
    obj._dedup = None
    obj._last_msg_id = {}
    for k, v in attrs.items():
        setattr(obj, k, v)
    return obj


def run_case(r):
    f = r.get("func")
    if f == "ws_proxy":
        _Env.values = dict(r.get("env") or {})
        gs = [k for k in ("WSS_PROXY", "wss_proxy", "HTTPS_PROXY", "https_proxy", "ALL_PROXY",
                          "all_proxy") if _Env.values.get(k)]
        return _Env.values[gs[0]] if gs else None
    if f == "parse_json":
        return ADAPTER._parse_json(r.get("raw"))
    if f == "parse_gateway_session_key":
        return ADAPTER._parse_gateway_session_key(r.get("session_key"))
    if f == "is_authorized_interaction":
        ev = types.SimpleNamespace(**{k: (r.get("event") or {}).get(k) for k in
                                      ("operator_openid", "group_openid", "guild_id",
                                       "user_openid")})
        return adapter()._is_authorized_interaction_for_session(ev, r.get("session_key"))
    if f == "append_block":
        return ADAPTER._append_block(r.get("text") or "", r.get("block") or "")
    if f == "merge_quote_into":
        return ADAPTER._merge_quote_into(r.get("text") or "", r.get("quote_block") or "")
    if f == "detect_message_type":
        mt = ADAPTER._detect_message_type(r.get("media_urls") or [], r.get("media_types") or [])
        return getattr(mt, "value", mt)
    if f == "opt_str":
        return ADAPTER._opt_str(r.get("value"))
    if f == "is_voice_content_type":
        return ADAPTER._is_voice_content_type(r.get("content_type") or "",
                                              r.get("filename") or "")
    if f == "guess_ext_from_data":
        return ADAPTER._guess_ext_from_data(bytes.fromhex(r.get("hex") or ""))
    if f == "looks_like_silk":
        return ADAPTER._looks_like_silk(bytes.fromhex(r.get("hex") or ""))
    if f == "rest_path":
        return ADAPTER._rest_path(r.get("chat_type") or "", r.get("target_id") or "",
                                  r.get("endpoint") or "")
    if f == "messages_path":
        return ADAPTER._messages_path(r.get("chat_type") or "", r.get("target_id") or "")
    if f == "build_text_body":
        o = adapter(_markdown_support=bool(r.get("markdown")))
        o.MAX_MESSAGE_LENGTH = int(r.get("max_len") or 4000)
        return o._build_text_body(r.get("content") or "", r.get("reply_to"))
    if f == "is_url":
        return ADAPTER._is_url(r.get("source") or "")
    if f == "strip_at_mention":
        return ADAPTER._strip_at_mention(r.get("content") or "")
    if f == "entry_matches":
        return adapter()._entry_matches(r.get("entries") or [], r.get("target") or "")
    if f == "guess_chat_type":
        return adapter(_chat_type_map=dict(r.get("chat_type_map") or {}))._guess_chat_type(
            r.get("chat_id") or "")
    if f == "parse_qq_timestamp":
        try:
            value = adapter()._parse_qq_timestamp(r.get("raw") or "")
        except _NowCall:
            return {"iso": None, "is_now": True}
        return {"iso": value.isoformat(), "is_now": False}
    if f == "constants":
        return {"voice_extensions": list(G["_VOICE_EXTENSIONS"]),
                "stt_provider_base_urls": dict(G["_STT_PROVIDER_BASE_URLS"])}
    if f == "resolve_stt_config":
        _Env.values = dict(r.get("env") or {})
        o = adapter()
        o.config = types.SimpleNamespace(extra=r.get("extra") or {})
        return o._resolve_stt_config()
    return None


FUNCS = {"constants", "ws_proxy", "parse_json", "parse_gateway_session_key", "is_authorized_interaction",
         "append_block", "merge_quote_into", "detect_message_type", "opt_str",
         "is_voice_content_type", "guess_ext_from_data", "looks_like_silk", "rest_path",
         "messages_path", "build_text_body", "is_url", "strip_at_mention", "entry_matches",
         "guess_chat_type", "parse_qq_timestamp", "resolve_stt_config"}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
