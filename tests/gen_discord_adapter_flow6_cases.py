#!/usr/bin/env python3
"""Generate tests/fixtures/discord_adapter_flow6.jsonl.

Each case: {"func": <name>, "in": {"args": {...}, "state": {...}, "io": {...}}}.
io keys are a subset of the C/oracle seam registry. At least 6 cases per
ported function. `fc: true` marks a deliberate fail-closed case.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "discord_adapter_flow6.jsonl"
OUT.parent.mkdir(parents=True, exist_ok=True)

cases = []


def add(func, args=None, io=None, state=None, fc=False, tag=""):
    rec = {"func": func, "in": {"args": args or {}, "io": io or {}}}
    if state is not None:
        rec["in"]["state"] = state
    if fc:
        rec["fc"] = True
    if tag:
        rec["tag"] = tag
    cases.append(rec)


rv = lambda t, m: {"__script__": True, "raise": {"type": t, "message": m}}

# ---- 1 _csv (no seams) ----
F = "_csv"
add(F, {"value": ["a", "b", "c"]})
add(F, {"value": [1, 2, 3]})
add(F, {"value": [True, False, None]})
add(F, {"value": "solo"})
add(F, {"value": None})
add(F, {"value": 42})
add(F, {"value": []})

# ---- 2 _t_discord (t -> truncate) ----
F = "_t_discord"
add(F, {"key": "x", "limit": 10}, {"t": "Hello", "_truncate_discord_component_text": "Hello"})
add(F, {"key": "y", "limit": 5}, {"t": "abcdefgh", "_truncate_discord_component_text": "abcde"})
add(F, {"key": "z", "limit": 8, "kwargs": {"n": 1}},
    {"t": "value", "_truncate_discord_component_text": "value"})
add(F, {"key": "q", "limit": 3}, {}, fc=True, tag="fail-closed: no t script")
add(F, {"key": "q", "limit": 3}, {"t": rv("ValueError", "bad")})
add(F, {"key": "q", "limit": 3},
    {"t": "abc", "_truncate_discord_component_text": rv("RuntimeError", "trunc")})
add(F, {"key": "unicode", "limit": 4, "kwargs": {"x": "é"}},
    {"t": "café", "_truncate_discord_component_text": "café"})

# ---- 3 _text (_t_discord seam) ----
F = "_text"
add(F, {"key": "a.b"}, {"_t_discord": "AB"})
add(F, {"key": ""}, {"_t_discord": "EMPTY"})
add(F, {"key": "platform.discord.command.btw"}, {"_t_discord": "BT"})
add(F, {"key": "k1"}, {}, fc=True, tag="fail-closed: no _t_discord script")
add(F, {"key": "k2"}, {"_t_discord": rv("TypeError", "boom")})
add(F, {"key": "k3"}, {"_t_discord": 123})
add(F, {"key": "k4"}, {"_t_discord": "é"})

# ---- 4 _choice_label (_text seam) ----
F = "_choice_label"
add(F, {"label": "NoDotHere"})
add(F, {"label": ""})
add(F, {"label": "a.b"}, {"_text": "T"})
add(F, {"label": ".leading"}, {"_text": "L"})
add(F, {"label": "a.b.c"}, {"_text": "M"})
add(F, {"label": "x.y"}, {}, fc=True, tag="fail-closed: no _text script")
add(F, {"label": "x.y"}, {"_text": rv("RuntimeError", "boom")})

# ---- 5 _default_voice_ack_phrases (t x5) ----
F = "_default_voice_ack_phrases"
add(F, {}, {"t": ["1", "2", "3", "4", "5"]})
add(F, {}, {"t": "same"})
add(F, {}, {"t": ["", "", "", "", ""]})
add(F, {}, {}, fc=True, tag="fail-closed: no t script")
add(F, {}, {"t": ["a", "b", rv("ValueError", "third"), "d", "e"]})
add(F, {}, {"t": ["é", "ü", "ö", "ñ", "ç"]})

# ---- 6 _unauthorized (unauthorized_action_notice) ----
F = "_unauthorized"
add(F, {}, {"unauthorized_action_notice": "denied"})
add(F, {}, {"unauthorized_action_notice": {"msg": "x"}})
add(F, {}, {"unauthorized_action_notice": None})
add(F, {}, {"unauthorized_action_notice": rv("RuntimeError", "boom")})
add(F, {}, {"unauthorized_action_notice": rv("SystemExit", "bye")})
add(F, {}, {}, fc=True, tag="fail-closed: no notice script")
add(F, {}, {"unauthorized_action_notice": "private"})

# ---- 7 _standalone_warn_missing_media (logger.warning) ----
F = "_standalone_warn_missing_media"
add(F, {"media_path": "/a/b.png"}, {"logger.warning": None})
add(F, {"media_path": ""}, {"logger.warning": None})
add(F, {"media_path": "café.png"}, {"logger.warning": None})
add(F, {"media_path": "/x"}, {"logger.warning": rv("RuntimeError", "logfail")})
add(F, {"media_path": "/x"}, {"logger.warning": rv("SystemExit", "bye")})
add(F, {"media_path": "/x"}, {}, fc=True, tag="fail-closed: no logger script")

# ---- 8 _env_bool (_scoped_gate_env) ----
F = "_env_bool"
add(F, {"name": "X", "default": False}, {"_scoped_gate_env": "true"})
add(F, {"name": "X", "default": False}, {"_scoped_gate_env": "YES"})
add(F, {"name": "X", "default": True}, {"_scoped_gate_env": "no"})
add(F, {"name": "X", "default": True}, {"_scoped_gate_env": ""})
add(F, {"name": "X", "default": False}, {"_scoped_gate_env": "On"})
add(F, {"name": "X", "default": False}, {"_scoped_gate_env": rv("RuntimeError", "boom")})
add(F, {"name": "X", "default": False}, {}, fc=True, tag="fail-closed: no gate script")

# ---- 9 _EA_REASON_LABEL (t) ----
F = "_EA_REASON_LABEL"
add(F, {}, {"t": "Reason"})
add(F, {}, {"t": "Why"})
add(F, {}, {"t": "Motivo"})
add(F, {}, {"t": "A: B"})
add(F, {}, {"t": rv("RuntimeError", "boom")})
add(F, {}, {}, fc=True, tag="fail-closed: no t script")

# ---- 10 _EA_HEADER (t x3) ----
F = "_EA_HEADER"
add(F, {}, {"t": ["Header", "Question", "Command"]})
add(F, {}, {"t": "X"})
add(F, {}, {"t": ["A", "B", "C"]})
add(F, {}, {"t": ["é", "ü", "ñ"]})
add(F, {}, {"t": ["a", rv("TypeError", "second"), "c"]})
add(F, {}, {}, fc=True, tag="fail-closed: no t script")

# ---- 11 _EA_SMART_DENY_LINE (t) ----
F = "_EA_SMART_DENY_LINE"
add(F, {}, {"t": "Do not: allow"})
add(F, {}, {"t": "no colon here"})
add(F, {}, {"t": "a:b:c"})
add(F, {}, {"t": ""})
add(F, {}, {"t": rv("RuntimeError", "boom")})
add(F, {}, {}, fc=True, tag="fail-closed: no t script")

# ---- 12 _discord_channel_ids_allowed (_get_allowed_channels) ----
F = "_discord_channel_ids_allowed"
sl = lambda v: {"__script__": True, "ret": v}
add(F, {"channel_ids": []})
add(F, {"channel_ids": ["1"]}, {"_get_allowed_channels": sl(["1"])})
add(F, {"channel_ids": ["1", "2"]}, {"_get_allowed_channels": sl(["9"])})
add(F, {"channel_ids": ["1"]}, {"_get_allowed_channels": sl(["*"])})
add(F, {"channel_ids": ["1"]}, {"_get_allowed_channels": sl([])})
add(F, {"channel_ids": ["5"]}, {}, fc=True, tag="fail-closed: no channels script")
add(F, {"channel_ids": ["5"]}, {"_get_allowed_channels": rv("ValueError", "boom")})

# ---- 13 _is_pairing_approved_user (PairingStore.is_approved) ----
F = "_is_pairing_approved_user"
add(F, {"user_id": ""})
add(F, {"user_id": None})
add(F, {"user_id": " 42 "}, {"PairingStore.is_approved": True})
add(F, {"user_id": "abc"}, {"PairingStore.is_approved": False})
add(F, {"user_id": "5"}, {"PairingStore.is_approved": rv("ValueError", "bad")})
add(F, {"user_id": "5"}, {"PairingStore.is_approved": rv("SystemExit", "bye")})
add(F, {"user_id": "5"}, {}, tag="missing script caught by except Exception -> False")

# ---- 14 _missed_message_backfill_enabled (_scoped_gate_env) ----
F = "_missed_message_backfill_enabled"
add(F, {}, {"_scoped_gate_env": "true"}, state={"config_extra": {"missed_message_backfill": {"enabled": True}}})
add(F, {}, {}, state={"config_extra": {"missed_message_backfill": {"enabled": "YES"}}})
add(F, {}, {}, state={"config_extra": {"missed_message_backfill": {"enabled": "no"}}})
add(F, {}, {}, state={"config_extra": {"missed_message_backfill": {"enabled": 0}}})
add(F, {}, {"_scoped_gate_env": "true"}, state={"config_extra": {}})
add(F, {}, {"_scoped_gate_env": "false"}, state={"config_extra": {}})
add(F, {}, {}, state={"config_extra": {}}, fc=True, tag="fail-closed: no gate script")
add(F, {}, {"_scoped_gate_env": rv("RuntimeError", "boom")}, state={"config_extra": {}})

# ---- 15 _get_effective_topic (_is_forum_parent) ----
F = "_get_effective_topic"
add(F, {"channel": {"topic": "t"}, "is_thread": False})
add(F, {"channel": {"topic": ""}, "is_thread": False})
add(F, {"channel": {"topic": None, "parent": {"topic": "p"}}, "is_thread": True},
    {"_is_forum_parent": True})
add(F, {"channel": {"topic": None, "parent": {"topic": "p"}}, "is_thread": True},
    {"_is_forum_parent": False})
add(F, {"channel": {"topic": None, "parent": {}}, "is_thread": True},
    {"_is_forum_parent": True})
add(F, {"channel": {"topic": None}, "is_thread": True})
add(F, {"channel": {"topic": None, "parent": {"topic": "p"}}, "is_thread": True},
    {}, fc=True, tag="fail-closed: no forum-parent script")
add(F, {"channel": {"topic": None, "parent": {"topic": "p"}}, "is_thread": True},
    {"_is_forum_parent": rv("RuntimeError", "boom")})

# ---- 16 _approval_mention_content (_extra_or_env_flag) ----
F = "_approval_mention_content"
add(F, {}, {"_extra_or_env_flag": False}, state={"_allowed_user_ids": ["1", "2"]})
add(F, {}, {"_extra_or_env_flag": True}, state={"_allowed_user_ids": ["1", "2"]})
add(F, {}, {"_extra_or_env_flag": True}, state={"_allowed_user_ids": ["05", "3", "10"]})
add(F, {}, {"_extra_or_env_flag": True}, state={"_allowed_user_ids": ["abc", "1"]})
add(F, {}, {"_extra_or_env_flag": True}, state={"_allowed_user_ids": []})
add(F, {}, {"_extra_or_env_flag": rv("RuntimeError", "boom")})
add(F, {}, {}, state={"_allowed_user_ids": ["1"]}, fc=True, tag="fail-closed: no flag script")

# ---- 17 _missed_message_backfill_number (_scoped_gate_env) ----
F = "_missed_message_backfill_number"
BASE = {"key": "k", "env_key": "ENV_K", "cast": "int", "default": 5, "lo": 1, "hi": 10}
add(F, dict(BASE), {}, state={"config_extra": {"missed_message_backfill": {"k": "3"}}})
add(F, dict(BASE), {}, state={"config_extra": {"missed_message_backfill": {"k": "bad"}}})
add(F, dict(BASE), {"_scoped_gate_env": "20"}, state={"config_extra": {}})
add(F, dict(BASE), {"_scoped_gate_env": "0"}, state={"config_extra": {}})
add(F, dict(BASE), {"_scoped_gate_env": "bad"}, state={"config_extra": {}})
add(F, dict(BASE, cast="float", lo=0, hi=None), {"_scoped_gate_env": "3.7"}, state={"config_extra": {}})
add(F, dict(BASE), {}, state={"config_extra": {}}, fc=True, tag="fail-closed: no gate script")
add(F, dict(BASE), {"_scoped_gate_env": rv("RuntimeError", "boom")}, state={"config_extra": {}})

# ---- 18-21 backfill wrappers (_missed_message_backfill_number seam) ----
for F in ("_missed_message_backfill_limit", "_missed_message_backfill_window_seconds",
          "_missed_message_backfill_max_dispatches", "_missed_message_backfill_max_attempts"):
    add(F, {}, {"_missed_message_backfill_number": {"r": 1}})
    add(F, {}, {"_missed_message_backfill_number": 50})
    add(F, {}, {"_missed_message_backfill_number": None})
    add(F, {}, {"_missed_message_backfill_number": "x"})
    add(F, {}, {"_missed_message_backfill_number": rv("RuntimeError", "boom")})
    add(F, {}, {}, fc=True, tag="fail-closed: no number script")

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

counts = {}
for c in cases:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"WROTE {OUT}: {len(cases)} cases, {len(counts)} funcs")
for k in sorted(counts):
    print(f"  {k}: {counts[k]}")
