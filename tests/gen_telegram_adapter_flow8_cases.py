#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 8 oracle.

Writes tests/fixtures/telegram_adapter_flow8.jsonl.  Each case is
{"func", "in": {"args", "state"?, "io"?}}.  Re-running reproduces
byte-identical fixtures.  Every ported function gets >= 6 cases and every seam
name appears in at least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow8.jsonl"

SEAM_NAMES = [
    "_extra_bool", "_extra_str_set", "_extra_or_secret", "_scoped_gate_env",
    "tag_safe_display_name", "dataclasses.replace", "apply_group_observe_attribution",
    "_current_bot_username", "addressing_text", "pattern.search", "json.loads",
    "compile_mention_patterns", "_should_process_message",
    "_observe_unmentioned_group_message", "_is_own_message",
    "_telegram_observe_unmentioned_group_messages", "_is_group_chat",
    "_topic_gates_pass", "_chat_id_str", "_telegram_exclusive_bot_mentions",
    "_explicit_bot_mentions_exclude_self", "_telegram_observe_allowed_chats",
    "_bot_sender_suppressed", "_telegram_free_response_chats",
    "_telegram_is_free_response_topic", "_telegram_require_mention",
    "_is_reply_to_bot", "_message_mentions_bot", "_message_matches_mention_patterns",
    "_should_observe_unmentioned_group_message",
]

CASES = []


def case(func, args=None, state=None, io=None, **kw):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    if io is not None:
        c["in"]["io"] = io
    c["in"].update(kw)
    return c


def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


def wrap(v):
    """A bare JSON array in io is a per-call SEQUENCE; wrap single list VALUES."""
    return sc(ret=v) if isinstance(v, list) else v


# ── 1-2: config bool gates ────────────────────────────────────────────────
for fn in ("_telegram_observe_unmentioned_group_messages", "_telegram_guest_mode"):
    for v in (True, False, "true", "no", None, sc(raise_type="RuntimeError", raise_msg="boom")):
        CASES.append(case(fn, io={"_extra_bool": v}))

# ── 3-7: config str-set gates ─────────────────────────────────────────────
for fn in ("_telegram_free_response_chats", "_telegram_free_response_topics",
           "_telegram_allowed_chats", "_telegram_group_allowed_chats",
           "_telegram_allowed_topics"):
    for v in (["g1", "g2"], [], ["one"], ["-100:1", "7"], ["solo"],
              sc(raise_type="ValueError", raise_msg="bad")):
        CASES.append(case(fn, io={"_extra_str_set": wrap(v)}))

# ── 8: _reactions_enabled ─────────────────────────────────────────────────
for v in (True, False, "false", "0", "no", "yes", 0, None,
          sc(raise_type="RuntimeError", raise_msg="x"),
          sc(raise_type="SystemExit", raise_msg="bye")):
    CASES.append(case("_reactions_enabled",
                      state={"config": {"extra": {"reactions": "raw"}}},
                      io={"_extra_or_secret": v}))

# ── 9: _telegram_auth_env_configured ──────────────────────────────────────
CASES += [
    case("_telegram_auth_env_configured", io={"_scoped_gate_env": ["", "", "", "", "", ""]}),
    case("_telegram_auth_env_configured", io={"_scoped_gate_env": ["x", "", "", "", "", ""]}),
    case("_telegram_auth_env_configured", io={"_scoped_gate_env": ["", "", "  y  ", "", "", ""]}),
    case("_telegram_auth_env_configured", io={"_scoped_gate_env": [" ", "\t\n", "", "", "", ""]}),
    case("_telegram_auth_env_configured", io={"_scoped_gate_env": ["", "", "", "", "", "set"]}),
    case("_telegram_auth_env_configured",
         io={"_scoped_gate_env": [sc(raise_type="RuntimeError", raise_msg="x")]}),
    case("_telegram_auth_env_configured",
         io={"_scoped_gate_env": [sc(raise_type="KeyboardInterrupt", raise_msg="x")]}),
]

# ── 10: _topic_id_or_general ──────────────────────────────────────────────
for tid in (None, 5, "12", 0, -3, ""):
    CASES.append(case("_topic_id_or_general", args={"thread_id": tid}))

# ── 11: _telegram_group_observe_attributed_text ───────────────────────────
def ev(user_id, user_name, text):
    return {"source": {"user_id": user_id, "user_name": user_name}, "text": text}

for i, (e, tag) in enumerate([
    (ev("42", "alice", "hello"), "Alice"),
    (ev(42, None, "hi"), "42"),
    (ev(None, "bob", "body"), "Bob"),
    (ev("", "", ""), "unknown"),
    (ev("7", "z", ""), "Zed"),
]):
    CASES.append(case("_telegram_group_observe_attributed_text",
                      args={"event": e}, io={"tag_safe_display_name": tag}))
CASES.append(case("_telegram_group_observe_attributed_text",
                  args={"event": ev("1", "u", "t")},
                  io={"tag_safe_display_name": sc(raise_type="RuntimeError", raise_msg="x")}))

# ── 12: _telegram_group_observe_shared_source ─────────────────────────────
for i, src in enumerate([{"user_id": "1", "user_name": "a", "shared_session": False},
                         {"user_id": None}, {}, {"user_id": 9, "x": 1}, {"a": [1, 2]}]):
    CASES.append(case("_telegram_group_observe_shared_source",
                      args={"source": src},
                      io={"dataclasses.replace": {"user_id": None, "shared_session": True, "src": i}}))
CASES.append(case("_telegram_group_observe_shared_source", args={"source": {"user_id": "1"}},
                  io={"dataclasses.replace": sc(raise_type="RuntimeError", raise_msg="x")}))

# ── 13: _apply_telegram_group_observe_attribution ─────────────────────────
for i, e in enumerate([{"text": "a"}, {"text": "", "source": {"user_id": "1"}},
                       {"text": "b", "auto_skill": "s"}, {}, {"text": "中文"}]):
    CASES.append(case("_apply_telegram_group_observe_attribution",
                      args={"event": e},
                      io={"apply_group_observe_attribution": e}))
CASES.append(case("_apply_telegram_group_observe_attribution", args={"event": {"text": "a"}},
                  io={"apply_group_observe_attribution": sc(raise_type="RuntimeError", raise_msg="x")}))

# ── 14: _telegram_group_observe_channel_prompt ────────────────────────────
for uname, bot in [("mybot", {"id": 123}), (None, {"id": 9}), ("", {"id": "7"}),
                   ("other", None), ("B", {"id": 0})]:
    CASES.append(case("_telegram_group_observe_channel_prompt",
                      state={"_bot": bot}, io={"_current_bot_username": uname}))
CASES.append(case("_telegram_group_observe_channel_prompt", state={"_bot": {"id": 1}},
                  io={"_current_bot_username": sc(raise_type="RuntimeError", raise_msg="x")}))

# ── 15: _message_matches_mention_patterns ─────────────────────────────────
CASES += [
    case("_message_matches_mention_patterns", state={"_mention_patterns": []},
         args={"message": {"text": "x"}}),
    case("_message_matches_mention_patterns", state={"_mention_patterns": ["foo"]},
         args={"message": {"text": "x"}},
         io={"addressing_text": "", "pattern.search": True}),
    case("_message_matches_mention_patterns", state={"_mention_patterns": ["foo", "bar"]},
         args={"message": {"text": "x"}},
         io={"addressing_text": "some foo text", "pattern.search": [False, True]}),
    case("_message_matches_mention_patterns", state={"_mention_patterns": ["foo", "bar"]},
         args={"message": {"text": "x"}},
         io={"addressing_text": "nothing", "pattern.search": [False, False]}),
    case("_message_matches_mention_patterns", state={"_mention_patterns": ["foo"]},
         args={"message": {"text": "x"}},
         io={"addressing_text": sc(raise_type="RuntimeError", raise_msg="x"), "pattern.search": True}),
    case("_message_matches_mention_patterns", state={"_mention_patterns": ["foo"]},
         args={"message": {"text": "x"}},
         io={"addressing_text": "a", "pattern.search": sc(raise_type="SystemExit", raise_msg="x")}),
]

# ── 16: _compile_mention_patterns ─────────────────────────────────────────
BASE16 = {"name": "adapter", "config": {"extra": {"mention_patterns": ["fromyaml"]}}}
CASES += [
    case("_compile_mention_patterns", state=BASE16,
         io={"_scoped_gate_env": "", "json.loads": [],
             "compile_mention_patterns": wrap(["fromyaml"])}),
    case("_compile_mention_patterns", state=BASE16,
         io={"_scoped_gate_env": "  ", "json.loads": [],
             "compile_mention_patterns": wrap(["fromyaml"])}),
    case("_compile_mention_patterns", state=BASE16,
         io={"_scoped_gate_env": "  [\"a\",\"b\"]  ", "json.loads": wrap(["a", "b"]),
             "compile_mention_patterns": wrap(["a", "b"])}),
    case("_compile_mention_patterns",
         state={"name": "adapter", "config": {"extra": {}}},
         io={"_scoped_gate_env": "envpat",
             "json.loads": sc(raise_type="ValueError", raise_msg="not json"),
             "compile_mention_patterns": wrap(["envpat"])}),
    case("_compile_mention_patterns",
         state={"name": "adapter", "config": {"extra": {}}},
         io={"_scoped_gate_env": "a,b,c",
             "json.loads": sc(raise_type="ValueError", raise_msg="not json"),
             "compile_mention_patterns": wrap(["a", "b", "c"])}),
    case("_compile_mention_patterns", state=BASE16,
         io={"_scoped_gate_env": "x", "json.loads": wrap(["x"]),
             "compile_mention_patterns": sc(raise_type="RuntimeError", raise_msg="x")}),
    case("_compile_mention_patterns",
         state={"name": "adapter", "config": {"extra": {"mention_patterns": None}}},
         io={"_scoped_gate_env": "", "json.loads": wrap([]), "compile_mention_patterns": wrap([])}),
    case("_compile_mention_patterns", state=BASE16,
         io={"_scoped_gate_env": sc(raise_type="SystemExit", raise_msg="x"),
             "json.loads": wrap([]), "compile_mention_patterns": wrap([])}),
]

# ── 17: _gate_or_observe ──────────────────────────────────────────────────
G17 = [
    {"_should_process_message": True,
     "_should_observe_unmentioned_group_message": False,
     "_observe_unmentioned_group_message": None},
    {"_should_process_message": False,
     "_should_observe_unmentioned_group_message": False,
     "_observe_unmentioned_group_message": None},
    {"_should_process_message": False,
     "_should_observe_unmentioned_group_message": True,
     "_observe_unmentioned_group_message": "ok"},
    {"_should_process_message": sc(raise_type="RuntimeError", raise_msg="x"),
     "_should_observe_unmentioned_group_message": False,
     "_observe_unmentioned_group_message": None},
    {"_should_process_message": False,
     "_should_observe_unmentioned_group_message": sc(raise_type="ValueError", raise_msg="x"),
     "_observe_unmentioned_group_message": None},
    {"_should_process_message": False,
     "_should_observe_unmentioned_group_message": True,
     "_observe_unmentioned_group_message": sc(raise_type="RuntimeError", raise_msg="x")},
    {"_should_process_message": sc(raise_type="SystemExit", raise_msg="x"),
     "_should_observe_unmentioned_group_message": False,
     "_observe_unmentioned_group_message": None},
]
for io in G17:
    CASES.append(case("_gate_or_observe",
                      args={"msg": {"message_id": 3}, "msg_type": "text",
                            "update": {"update_id": 99}}, io=io))

# ── 18: _should_observe_unmentioned_group_message ─────────────────────────
def default18(**over):
    d = {
        "_is_own_message": False,
        "_telegram_observe_unmentioned_group_messages": True,
        "_is_group_chat": True,
        "_topic_gates_pass": None,          # `is False` -> not gated
        "_chat_id_str": "g1",
        "_telegram_exclusive_bot_mentions": False,
        "_explicit_bot_mentions_exclude_self": False,
        "_telegram_observe_allowed_chats": wrap(["g1", "g2"]),
        "_bot_sender_suppressed": False,
        "_telegram_free_response_chats": wrap([]),
        "_telegram_is_free_response_topic": False,
        "_telegram_require_mention": True,
        "_is_reply_to_bot": False,
        "_message_mentions_bot": False,
        "_message_matches_mention_patterns": False,
    }
    d.update(over)
    return d

MSG18 = {"message_id": 3, "message_thread_id": 7, "chat": {"id": -100, "type": "supergroup"}}
OVER18 = [
    {"_is_own_message": True},
    {"_telegram_observe_unmentioned_group_messages": False},
    {"_is_group_chat": False},
    {"_topic_gates_pass": False},
    {"_topic_gates_pass": 0},             # 0 is not False -> continue
    {"_explicit_bot_mentions_exclude_self": True,
     "_telegram_exclusive_bot_mentions": True},
    {"_telegram_observe_allowed_chats": wrap([])},
    {"_telegram_observe_allowed_chats": wrap(["other"])},
    {"_bot_sender_suppressed": True},
    {"_telegram_free_response_chats": wrap(["g1"])},
    {"_telegram_is_free_response_topic": True},
    {"_telegram_require_mention": False},
    {"_is_reply_to_bot": True},
    {"_message_mentions_bot": True},
    {"_message_matches_mention_patterns": True},
    {},
]
for over in OVER18:
    CASES.append(case("_should_observe_unmentioned_group_message",
                      args={"message": MSG18}, io=default18(**over)))

# fail-closed / BaseException passthrough coverage for func 18
CASES.append(case("_should_observe_unmentioned_group_message", args={"message": MSG18},
                  io=default18(_is_own_message=sc(raise_type="RuntimeError", raise_msg="x"))))
CASES.append(case("_should_observe_unmentioned_group_message", args={"message": MSG18},
                  io=default18(_telegram_observe_allowed_chats=sc(raise_type="SystemExit", raise_msg="x"))))

# ── invariants ────────────────────────────────────────────────────────────
counts = {}
keys = set()
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
    keys |= set((c["in"].get("io") or {}).keys())
assert all(v >= 6 for v in counts.values()), counts
assert keys == set(SEAM_NAMES), (sorted(set(SEAM_NAMES) - keys), sorted(keys - set(SEAM_NAMES)))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
print(f"wrote {len(CASES)} cases over {len(counts)} funcs to {OUT}")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
