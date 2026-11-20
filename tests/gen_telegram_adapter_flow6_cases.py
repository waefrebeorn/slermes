#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 6 oracle.

Writes tests/fixtures/telegram_adapter_flow6.jsonl.  Each case is
{"func", "in": {"args", "state"?, "io"?, "errors"?}}.  Re-running reproduces
byte-identical fixtures.  Every ported function gets >= 6 cases.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow6.jsonl"


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


def es(pairs):
    """_entity_sources is called twice (two loops); both calls see the same pairs."""
    return [pairs, pairs]


CASES = []


def add(func, *cases):
    for c in cases:
        assert c["func"] == func
        CASES.append(c)


# ── _normalize_platform_event ──
RENV = {"platform": "telegram", "event_type": "reaction", "payload": {}}
EENV = {"platform": "telegram", "event_type": "message_edited", "payload": {}}
add("_normalize_platform_event",
    case("_normalize_platform_event", {"update": {"message_reaction": {}, "edited_message": None}},
         io={"_normalize_reaction_event": RENV}),
    case("_normalize_platform_event", {"update": {"edited_message": {}, "message_reaction": None}},
         io={"_normalize_message_edited_event": EENV}),
    case("_normalize_platform_event", {"update": {"message_reaction": {}, "edited_message": {}}},
         io={"_normalize_reaction_event": RENV, "_normalize_message_edited_event": EENV}),
    case("_normalize_platform_event", {"update": {"message_reaction": None, "edited_message": None}}),
    case("_normalize_platform_event", {"update": {}}),
    case("_normalize_platform_event", {"update": None}),
    case("_normalize_platform_event", {"update": {"message_reaction": {}}},
         io={"_normalize_reaction_event": raise_sc("ValueError", "x")}),
)

# ── _normalize_reaction_event ──
add("_normalize_reaction_event",
    case("_normalize_reaction_event",
         {"update": {"message_reaction": {"chat": {"id": 100}, "message_id": 5,
                                          "new_reaction": [{"emoji": "ok", "custom_emoji_id": None}]}}},
         io={"_is_id_like": [True, True, False]}),
    case("_normalize_reaction_event",
         {"update": {"message_reaction": {"chat": {"id": 100}, "message_id": 5,
                                          "new_reaction": [{"emoji": "", "custom_emoji_id": 777}]}}},
         io={"_is_id_like": [True, True, True]}),
    case("_normalize_reaction_event", {"update": {"message_reaction": None}}),
    case("_normalize_reaction_event",
         {"update": {"message_reaction": {"chat": {"id": 1}, "message_id": 2, "new_reaction": "x"}}}),
    case("_normalize_reaction_event",
         {"update": {"message_reaction": {"chat": {"id": 1}, "message_id": 2, "new_reaction": []}}},
         io={"_is_id_like": [True, True]}),
    case("_normalize_reaction_event",
         {"update": {"message_reaction": {"chat": {"id": 1}, "message_id": 2}}},
         io={"_is_id_like": [True, True]}),
    case("_normalize_reaction_event",
         {"update": {"message_reaction": {"chat": {"id": "abc"}, "message_id": "m-1",
                                          "new_reaction": [{"emoji": "y" * 70}]}}},
         io={"_is_id_like": [True, True, False]}),
)

# ── _normalize_message_edited_event ──
def edup(**msg):
    d = {"chat": {"id": 7}, "message_id": 9}
    d.update(msg)
    return {"update": {"edited_message": d}}


add("_normalize_message_edited_event",
    case("_normalize_message_edited_event", edup(text="hello"),
         io={"_is_id_like": [True, True, False]}),
    case("_normalize_message_edited_event", edup(caption="cap"),
         io={"_is_id_like": [True, True, False]}),
    case("_normalize_message_edited_event", {"update": {"edited_message": None}}),
    case("_normalize_message_edited_event", edup(text="", caption="fallback"),
         io={"_is_id_like": [True, True, False]}),
    case("_normalize_message_edited_event",
         edup(text="t", message_thread_id=55, is_topic_message=True),
         io={"_is_id_like": [True, True, True, True, True]}),
    case("_normalize_message_edited_event",
         edup(text="t", edit_date={"isoformat": True}),
         io={"_is_id_like": [True, True, False], "isoformat": "2026-11-20T00:00:00+00:00"}),
    case("_normalize_message_edited_event",
         edup(text="t", edit_date={"isoformat": True}),
         io={"_is_id_like": [True, True, False], "isoformat": raise_sc("RuntimeError", "bad")}),
    case("_normalize_message_edited_event",
         edup(text="t", edit_date={"isoformat": True}),
         io={"_is_id_like": [True, True, False], "isoformat": raise_sc("SystemExit", "halt")},
         expect_fail_closed=True),
)

# ── _telegram_entity_text ──
add("_telegram_entity_text",
    case("_telegram_entity_text", {"source_text": "hello", "offset": 1, "length": 3}),
    case("_telegram_entity_text", {"source_text": "hello", "offset": 0, "length": 0}),
    case("_telegram_entity_text", {"source_text": "hello", "offset": -1, "length": 3}),
    case("_telegram_entity_text", {"source_text": "hello", "offset": 4, "length": 100}),
    case("_telegram_entity_text", {"source_text": "\U0001f600a", "offset": 0, "length": 2}),
    case("_telegram_entity_text", {"source_text": "\U0001f600a", "offset": 1, "length": 2}),
    case("_telegram_entity_text", {"source_text": "\U0001f600a", "offset": 2, "length": 1}),
)

# ── _entity_span ──
add("_entity_span",
    case("_entity_span", {"source_text": "hello", "entity": {"offset": 1, "length": 3}},
         io={"_telegram_entity_text": "ell"}),
    case("_entity_span", {"source_text": "hello", "entity": {"offset": -1, "length": 3}}),
    case("_entity_span", {"source_text": "hello", "entity": {"offset": 0, "length": 0}}),
    case("_entity_span", {"source_text": "hello", "entity": {}}),
    case("_entity_span", {"source_text": "hello", "entity": {"offset": "2", "length": "2"}},
         io={"_telegram_entity_text": "ll"}),
    case("_entity_span", {"source_text": "hello", "entity": {"offset": 1, "length": 3}},
         io={"_telegram_entity_text": None}),
    case("_entity_span", {"source_text": "hello", "entity": {"offset": 1, "length": 3}},
         io={"_telegram_entity_text": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
)

# ── _chat_id_str ──
add("_chat_id_str",
    case("_chat_id_str", {"message": {"chat": {"id": 123}}}),
    case("_chat_id_str", {"message": {"chat": {"id": "abc"}}}),
    case("_chat_id_str", {"message": {"chat": None}}),
    case("_chat_id_str", {"message": {}}),
    case("_chat_id_str", {"message": {"chat": {}}}),
    case("_chat_id_str", {"message": {"chat": {"id": None}}}),
    case("_chat_id_str", {"message": None}),
)

# ── _is_group_chat ──
add("_is_group_chat",
    case("_is_group_chat", {"message": {"chat": {"id": 1}}}, io={"_chat_type_str": "group"}),
    case("_is_group_chat", {"message": {"chat": {"id": 1}}}, io={"_chat_type_str": "supergroup"}),
    case("_is_group_chat", {"message": {"chat": {"id": 1}}}, io={"_chat_type_str": "channel"}),
    case("_is_group_chat", {"message": {"chat": {"id": 1}}}, io={"_chat_type_str": "private"}),
    case("_is_group_chat", {"message": {"chat": None}}),
    case("_is_group_chat", {"message": {"chat": {"id": 1}}},
         io={"_chat_type_str": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
)

# ── _extract_bot_mention_usernames ──
E1 = {"type": "mention", "offset": 0, "length": 7}
E2 = {"type": "bot_command", "offset": 0, "length": 15}
add("_extract_bot_mention_usernames",
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": "@MyBot"},
         io={"_entity_sources": es([["hi @mybot", [E1]]]),
             "_entity_type": ["mention"], "_entity_span": ["@mybot"]}),
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": ""},
         io={"_entity_sources": es([["hi @helper_bot", [E1]]]),
             "_entity_type": ["mention"], "_entity_span": ["@helper_bot"]}),
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": ""},
         io={"_entity_sources": es([["hi @alice", [E1]]]),
             "_entity_type": ["mention"], "_entity_span": ["@alice"]}),
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": ""},
         io={"_entity_sources": es([["/new@helper_bot", [E2]]]),
             "_entity_type": ["bot_command"], "_entity_span": ["/new@helper_bot"]}),
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": ""},
         io={"_entity_sources": es([["#topic", [E1]]]), "_entity_type": ["hashtag"]}),
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": ""},
         io={"_entity_sources": es([["hi @helper_bot and @alice", []]])}),
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": ""},
         io={"_entity_sources": es([["@helper_bot", [E1]]]), "_entity_type": ["hashtag"]}),
    case("_extract_bot_mention_usernames", {"message": {}, "self_username": ""},
         io={"_entity_sources": es([["hi", [E1]]]),
             "_entity_type": ["mention"], "_entity_span": [None]}),
)

# ── _is_guest_mention ──
GM = {"message": {"chat": {"id": 1}}}
add("_is_guest_mention",
    case("_is_guest_mention", GM, io={"_chat_type_str": "group", "_telegram_guest_mode": True,
                                      "_message_mentions_bot": True}),
    case("_is_guest_mention", GM, io={"_chat_type_str": "channel"}),
    case("_is_guest_mention", GM, io={"_chat_type_str": "group", "_telegram_guest_mode": False}),
    case("_is_guest_mention", GM, io={"_chat_type_str": "group", "_telegram_guest_mode": True,
                                      "_message_mentions_bot": False}),
    case("_is_guest_mention", GM, io={"_chat_type_str": "dm", "_telegram_guest_mode": True,
                                      "_message_mentions_bot": True}),
    case("_is_guest_mention", GM,
         io={"_chat_type_str": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
    case("_is_guest_mention", GM,
         io={"_chat_type_str": "group", "_telegram_guest_mode": raise_sc("ValueError", "x")}),
)

# ── _bot_sender_suppressed ──
BM = {"message": {"chat": {"id": 1}}}
add("_bot_sender_suppressed",
    case("_bot_sender_suppressed", BM,
         io={"_telegram_bots_require_mention": True, "_sender_is_other_bot": True,
             "_message_mentions_bot": False}),
    case("_bot_sender_suppressed", BM, io={"_telegram_bots_require_mention": False}),
    case("_bot_sender_suppressed", BM,
         io={"_telegram_bots_require_mention": True, "_sender_is_other_bot": False}),
    case("_bot_sender_suppressed", BM,
         io={"_telegram_bots_require_mention": True, "_sender_is_other_bot": True,
             "_message_mentions_bot": True}),
    case("_bot_sender_suppressed", BM,
         io={"_telegram_bots_require_mention": True,
             "_sender_is_other_bot": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
    case("_bot_sender_suppressed", BM, io={"_telegram_bots_require_mention": [False, True, True, True],
                                          "_sender_is_other_bot": [True], "_message_mentions_bot": [False]}),
)

# ── _clean_bot_trigger_text ──
add("_clean_bot_trigger_text",
    case("_clean_bot_trigger_text", {"text": "hi @otherbot"},
         io={"_current_bot_username": "mybot", "clean_bot_trigger_text": "hi"}),
    case("_clean_bot_trigger_text", {"text": None},
         io={"_current_bot_username": "", "clean_bot_trigger_text": None}),
    case("_clean_bot_trigger_text", {"text": ""},
         io={"_current_bot_username": "x", "clean_bot_trigger_text": ""}),
    case("_clean_bot_trigger_text", {"text": "hi"},
         io={"_current_bot_username": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
    case("_clean_bot_trigger_text", {"text": "hi"},
         io={"_current_bot_username": "my", "clean_bot_trigger_text": raise_sc("ValueError", "x")}),
    case("_clean_bot_trigger_text", {"text": "hi"},
         io={"_current_bot_username": ["a", "b"], "clean_bot_trigger_text": ["r1", "r2"]}),
)

# ── _current_bot_username ──
add("_current_bot_username",
    case("_current_bot_username", {}, state={"_bot_username_observed": "OBSERVED"}),
    case("_current_bot_username", {}, state={"_bot": {"username": "@MyBot"}}),
    case("_current_bot_username", {}, state={"_bot_username_observed": "", "_bot": {"username": "helper_bot"}}),
    case("_current_bot_username", {}, state={"_bot": {"username": None}}),
    case("_current_bot_username", {}, state={"_bot": None}),
    case("_current_bot_username", {}, state={}),
    case("_current_bot_username", {}, state={"_bot": {"username": "@@Double"}}),
)

# ── _is_transient_typing_error ──
ERRS = {
    "e1": {"class": "OSError", "message": "boom"},
    "e2": {"class": "RuntimeError", "message": "Too Many Requests"},
    "e3": {"class": "ValueError", "message": "nope"},
    "e4": {"class": "ValueError", "message": "x", "status_code": 429},
    "e5": {"class": "ValueError", "message": "x", "status_code": 500},
    "e6": {"class": "ValueError", "message": "x", "status_code": 404},
    "e7": {"class": "ValueError", "message": "x", "retry_after": 5},
    "e8": {"class": "RuntimeError", "message": "slow", "code": 502},
    "e9": {"class": "KeyboardInterrupt", "message": "Too Many Requests"},
}
add("_is_transient_typing_error",
    *[case("_is_transient_typing_error", {"exc": eid}, errors=ERRS) for eid in ERRS])

# ── _dm_topic_missing_anchor_error ──
add("_dm_topic_missing_anchor_error",
    *[case("_dm_topic_missing_anchor_error", {"i": i}) for i in range(6)])

# ── gate helpers (_extra_bool) ──
for fn in ("_telegram_require_mention", "_telegram_bots_require_mention",
           "_telegram_exclusive_bot_mentions"):
    add(fn,
        case(fn, {}, io={"_extra_bool": True}),
        case(fn, {}, io={"_extra_bool": False}),
        case(fn, {}, io={"_extra_bool": None}),
        case(fn, {}, io={"_extra_bool": raise_sc("SystemExit", "halt")}, expect_fail_closed=True),
        case(fn, {}, io={"_extra_bool": [True, False, True, False, True, False]}),
        case(fn, {}, io={"_extra_bool": "yes"}),
    )


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [json.dumps(c, ensure_ascii=False, separators=(",", ":")) for c in CASES]
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    from collections import Counter
    counts = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases to {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    assert all(v >= 6 for v in counts.values()), "every func needs >=6 cases"


if __name__ == "__main__":
    main()
