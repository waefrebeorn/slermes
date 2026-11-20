#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 3 oracle.

Writes tests/fixtures/telegram_adapter_flow3.jsonl.  Each case is
{"func", "in": {"args", "state"?, "io"?, "errors"?}}.  Re-running reproduces
byte-identical fixtures.  Every ported function gets >= 5 cases.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow3.jsonl"


def case(func, args=None, state=None, io=None, errors=None, **kw):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    if io is not None:
        c["in"]["io"] = io
    if errors is not None:
        c["in"]["errors"] = errors
    c["in"].update(kw)
    return c


def sc(**kw):
    d = {"__script__": True}
    d.update(kw)
    return d


def raise_sc(kind, msg):
    return sc(**{"raise": {"type": kind, "message": msg}})


CASES = []


def add(func, *cases):
    for c in cases:
        assert c["func"] == func
        CASES.append(c)


# ── _is_id_like ──
add("_is_id_like",
    case("_is_id_like", {"value": "abc"}),
    case("_is_id_like", {"value": ""}),
    case("_is_id_like", {"value": 5}),
    case("_is_id_like", {"value": 2.5}),
    case("_is_id_like", {"value": True}),
    case("_is_id_like", {"value": None}),
    case("_is_id_like", {"value": [1]}),
)

# ── _caption_1024 ──
add("_caption_1024",
    case("_caption_1024", {"caption": None}),
    case("_caption_1024", {"caption": ""}),
    case("_caption_1024", {"caption": "hello world"}),
    case("_caption_1024", {"caption": "a" * 1030}),
    case("_caption_1024", {"caption": "\u00e9" * 1100}),
    case("_caption_1024", {"caption": "x" * 1024}),
)

# ── _ext_from_path ──
add("_ext_from_path",
    case("_ext_from_path", {"file_path": "photo.JPG", "candidates": ["jpg", "png", "txt"], "default": "bin"}),
    case("_ext_from_path", {"file_path": "a.png", "candidates": ["jpg", "png", "txt"], "default": "bin"}),
    case("_ext_from_path", {"file_path": "FILE.TXT", "candidates": ["jpg", "png", "txt"], "default": "bin"}),
    case("_ext_from_path", {"file_path": "noext", "candidates": ["jpg", "png"], "default": "bin"}),
    case("_ext_from_path", {"file_path": None, "candidates": ["jpg"], "default": "bin"}),
    case("_ext_from_path", {"file_path": "a.gif", "candidates": [], "default": "bin"}),
    case("_ext_from_path", {"file_path": "a.txt", "candidates": ["TXT", "png"], "default": "bin"}),
)

# ── _rows_of_two ──
add("_rows_of_two",
    case("_rows_of_two", {"buttons": []}),
    case("_rows_of_two", {"buttons": [1]}),
    case("_rows_of_two", {"buttons": [1, 2]}),
    case("_rows_of_two", {"buttons": [1, 2, 3]}),
    case("_rows_of_two", {"buttons": ["a", "b", "c", "d", "e"]}),
    case("_rows_of_two", {"buttons": [None, True]}),
)

# ── _entity_type ──
add("_entity_type",
    case("_entity_type", {"entity": {"type": "MessageEntity.TEXT"}}),
    case("_entity_type", {"entity": {"type": "MENTION"}}),
    case("_entity_type", {"entity": {"type": "MessageEntity.BOT_COMMAND"}}),
    case("_entity_type", {"entity": {}}),
    case("_entity_type", {"entity": {"type": "URL"}}),
    case("_entity_type", {"entity": {"type": "A.B.C"}}),
)

# ── _flood_cap_result ──
for w in (12.5, 0.5, 30.25, 1.5, 2.75, 100.125):
    add("_flood_cap_result", case("_flood_cap_result", {"wait": w}))

# ── _is_permanent_fatal ──
add("_is_permanent_fatal",
    case("_is_permanent_fatal", {}, state={}),
    case("_is_permanent_fatal", {}, state={"_fatal_error_code": None}),
    case("_is_permanent_fatal", {}, state={"_fatal_error_code": ""}),
    case("_is_permanent_fatal", {}, state={"_fatal_error_code": "X", "_fatal_error_retryable": False}),
    case("_is_permanent_fatal", {}, state={"_fatal_error_code": "X", "_fatal_error_retryable": True}),
    case("_is_permanent_fatal", {}, state={"_fatal_error_code": 5, "_fatal_error_retryable": 0}),
    case("_is_permanent_fatal", {}, state={"_fatal_error_code": "X", "_fatal_error_retryable": None}),
)

# ── _should_drop_delayed_delivery ──
add("_should_drop_delayed_delivery",
    case("_should_drop_delayed_delivery", {}, state={}),
    case("_should_drop_delayed_delivery", {}, state={"_drop_delayed_deliveries": None}),
    case("_should_drop_delayed_delivery", {}, state={"_drop_delayed_deliveries": False}),
    case("_should_drop_delayed_delivery", {}, state={"_drop_delayed_deliveries": True}),
    case("_should_drop_delayed_delivery", {}, state={"_drop_delayed_deliveries": 0}),
    case("_should_drop_delayed_delivery", {}, state={"_drop_delayed_deliveries": 1}),
)

# ── _is_own_message ──
add("_is_own_message",
    case("_is_own_message", {"message": {"from_user": {"id": 1}}}, state={}),
    case("_is_own_message", {"message": {"from_user": {"id": 1}}}, state={"_bot": None}),
    case("_is_own_message", {"message": {"from_user": {"id": 1}}}, state={"_bot": {"id": 1}}),
    case("_is_own_message", {"message": {"from_user": {"id": 2}}}, state={"_bot": {"id": 1}}),
    case("_is_own_message", {"message": {"from_user": None}}, state={"_bot": {"id": 1}}),
    case("_is_own_message", {"message": {}}, state={"_bot": {"id": 1}}),
    case("_is_own_message", {"message": {"from_user": {}}}, state={"_bot": {"id": 1}}),
)

# ── _sender_is_other_bot ──
add("_sender_is_other_bot",
    case("_sender_is_other_bot", {"message": {}}, state={"_bot": {"id": 1}}),
    case("_sender_is_other_bot", {"message": {"from_user": None}}, state={"_bot": {"id": 1}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": False, "id": 2}}}, state={"_bot": {"id": 1}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 2}}}, state={"_bot": None}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 2}}}, state={"_bot": {"id": 2}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 2}}}, state={"_bot": {"id": 1}}),
    case("_sender_is_other_bot", {"message": {"from_user": {"is_bot": True, "id": 2}}}, state={}),
)

# ── _media_message_type ──
add("_media_message_type",
    case("_media_message_type", {"msg": {"sticker": True}}),
    case("_media_message_type", {"msg": {"photo": True}}),
    case("_media_message_type", {"msg": {"video": True}}),
    case("_media_message_type", {"msg": {"video_note": True}}),
    case("_media_message_type", {"msg": {"audio": True}}),
    case("_media_message_type", {"msg": {"voice": True}}),
    case("_media_message_type", {"msg": {}}),
    case("_media_message_type", {"msg": {"sticker": True, "photo": True}}),
)

# ── _text_batch_delay_for ──
def tbstate(delay=0.5, split=0.9):
    return {"_text_batch_delay_seconds": delay, "_text_batch_split_delay_seconds": split}


add("_text_batch_delay_for",
    case("_text_batch_delay_for", {"pending": None}, state=tbstate()),
    case("_text_batch_delay_for", {"pending": {"text": "hi"}}, state=tbstate()),
    case("_text_batch_delay_for", {"pending": {"text": "x" * 400}}, state=tbstate()),
    case("_text_batch_delay_for", {"pending": {"text": "x" * 2000}}, state=tbstate()),
    case("_text_batch_delay_for", {"pending": {"_last_chunk_len": 4000}}, state=tbstate()),
    case("_text_batch_delay_for", {"pending": {"_last_chunk_len": 3999, "text": "short"}}, state=tbstate()),
    case("_text_batch_delay_for", {"pending": {"text": "short"}}, state=tbstate(0.1)),
)

# ── _env_float_clamped ──
add("_env_float_clamped",
    case("_env_float_clamped", {"name": "X", "default": 2.5}, io={"os.getenv": None}),
    case("_env_float_clamped", {"name": "X", "default": 2.5}, io={"os.getenv": "3.75"}),
    case("_env_float_clamped", {"name": "X", "default": 2.5}, io={"os.getenv": 4}),
    case("_env_float_clamped", {"name": "X", "default": 2.5}, io={"os.getenv": "abc"}),
    case("_env_float_clamped", {"name": "X", "default": 2.5}, io={"os.getenv": "inf"}),
    case("_env_float_clamped", {"name": "X", "default": 2.5, "min_value": 1, "max_value": 5}, io={"os.getenv": "10"}),
    case("_env_float_clamped", {"name": "X", "default": 2.5, "min_value": 1, "max_value": 5}, io={"os.getenv": "0.5"}),
    case("_env_float_clamped", {"name": "X", "default": 2.5}, io={"os.getenv": raise_sc("SystemExit", "halt")},
         expect_fail_closed=True),
)

# ── _redact_telegram_error_text ──
ER = {"e1": {"class": "RuntimeError", "message": "boom"},
      "e2": {"class": "ConnectTimeout", "message": ""},
      "e3": {"class": "ValueError", "message": "secret=abc"}}
add("_redact_telegram_error_text",
    case("_redact_telegram_error_text", {"error": "e1", "proxy_url": None},
         io={"redact_sensitive_text": "clean"}, errors=ER),
    case("_redact_telegram_error_text", {"error": "e1", "proxy_url": None},
         io={"redact_sensitive_text": raise_sc("RuntimeError", "redact down")}, errors=ER),
    case("_redact_telegram_error_text", {"error": "e1", "proxy_url": None},
         io={"redact_sensitive_text": raise_sc("SystemExit", "halt")}, errors=ER, expect_fail_closed=True),
    case("_redact_telegram_error_text", {"error": "e1", "proxy_url": "http://u:p@h"},
         io={"describe_proxy_error": "http://h", "redact_sensitive_text": "clean"}, errors=ER),
    case("_redact_telegram_error_text", {"error": "e1", "proxy_url": "http://u:p@h"},
         io={"describe_proxy_error": None, "redact_sensitive_text": "clean"}, errors=ER),
    case("_redact_telegram_error_text", {"error": None, "proxy_url": None}, errors=ER),
    case("_redact_telegram_error_text", {"error": "e2", "proxy_url": None}, errors=ER),
    case("_redact_telegram_error_text", {"error": "e3", "proxy_url": None},
         io={"redact_sensitive_text": "secret=***"}, errors=ER),
)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [json.dumps(c, ensure_ascii=False, separators=(",", ":")) for c in CASES]
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases to {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    assert all(v >= 5 for v in counts.values()), "every func needs >=5 cases"


if __name__ == "__main__":
    main()
