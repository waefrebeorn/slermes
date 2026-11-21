#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 9 oracle.

Writes tests/fixtures/telegram_adapter_flow9.jsonl.  Each case is
{"func", "in": {"args", "state"?, "io"?}}.  Re-running reproduces
byte-identical fixtures.  Every ported function gets >= 6 cases and every seam
name appears in at least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow9.jsonl"

SEAM_NAMES = [
    "html_escape", "t", "InlineKeyboardButton", "TelegramAdapter",
    "resolve_notifications_mode", "LinkPreviewOptions", "parse_fallback_ip_env",
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


# ── 1: _bold_label_html ────────────────────────────────────────────────────
for line, esc, tag in [
    ("Hello", "E1", None),
    ("Label: rest", ["L", "R"], None),
    ("a:b:c", ["A", "B:C"], None),
    ("", "", None),
    (":x", ["", "X"], None),
    ("k:v", sc(raise_type="RuntimeError", raise_msg="boom"), None),
    ("k", sc(raise_type="SystemExit", raise_msg="bye"), None),
]:
    CASES.append(case("_bold_label_html", args={"line": line}, io={"html_escape": esc}))

# ── 2: _append_observed_note (pure; io empty) ───────────────────────────────
for existing, note in [
    ("a", "b"), (None, "b"), ("", "b"), ("a", ""), (None, None),
    (5, 0), (0, "n"), ("x", 7),
]:
    CASES.append(case("_append_observed_note",
                      args={"existing": existing, "note": note}, io={}))

# ── 3: _EA_HEADER ───────────────────────────────────────────────────────────
for t, e in [
    ("Header", "EscapedHeader"),
    ("", ""),
    ("A&B", "A&amp;B"),
    (sc(raise_type="RuntimeError", raise_msg="x"), "unused"),
    ("X", sc(raise_type="SystemExit", raise_msg="x")),
    (123, "123escaped"),
    (None, "None"),
]:
    CASES.append(case("_EA_HEADER", io={"t": t, "html_escape": e}))

# ── 4: _EA_REASON_LABEL ─────────────────────────────────────────────────────
for t, e in [
    ("Reason", "R"),
    ("", ""),
    ("x", "<b>x</b>"),
    (sc(raise_type="ValueError", raise_msg="x"), "unused"),
    ("x", sc(raise_type="KeyboardInterrupt", raise_msg="x")),
    (None, "N"),
    ("ü", "ü"),
]:
    CASES.append(case("_EA_REASON_LABEL", io={"t": t, "html_escape": e}))

# ── 5: _ea_escape ───────────────────────────────────────────────────────────
for text, e in [
    ("a&b", "a&amp;b"),
    ("", ""),
    ("<tag>", "&lt;tag&gt;"),
    (None, "None"),
    (5, "5"),
    ("x", "X"),
    ("x", sc(raise_type="SystemExit", raise_msg="x")),
]:
    CASES.append(case("_ea_escape", args={"text": text}, io={"html_escape": e}))
CASES.append(case("_ea_escape", args={"text": "x"},
                  io={"html_escape": sc(raise_type="RuntimeError", raise_msg="x")}))

# ── 6: _EA_ACTION_LABELS ────────────────────────────────────────────────────
for seq in [
    ["Once", "Session", "Always", "Deny"],
    ["", "", "", ""],
    ["A", "B", sc(raise_type="RuntimeError", raise_msg="x"), "D"],
    ["A"],                                  # fail-closed: script exhausted
    [sc(raise_type="SystemExit", raise_msg="x")],
    ["1", "2", "3", "4"],
    ["w", "x", "y", "z"],
]:
    c = case("_EA_ACTION_LABELS", io={"t": seq})
    if seq == ["A"]:
        c["in"]["expect_fail_closed"] = True
    CASES.append(c)

# ── 7: _EA_SMART_DENY_LINE ──────────────────────────────────────────────────
for t, e in [
    ("Label: text", ["L", "T"]),
    ("whole", "W"),
    ("", ""),
    (sc(raise_type="RuntimeError", raise_msg="x"), "unused"),
    ("x", sc(raise_type="SystemExit", raise_msg="x")),
    ("a:b", ["A", "B"]),
    ("no-colon", "NC"),
]:
    CASES.append(case("_EA_SMART_DENY_LINE", io={"t": t, "html_escape": e}))

# ── 8: _provider_list_text ──────────────────────────────────────────────────
for cm, pl, pi, seq, fc in [
    ("gpt", "OpenAI", " (1/3)", ["*Title*", "**current**", "**provider**", "**select**"], False),
    (None, "P", "", ["`?`", "T", "C", "P", "S"], False),
    ("", None, "more", ["U", "T", "C", "P", "S"], False),
    ("m", "P", "", ["T", sc(raise_type="RuntimeError", raise_msg="x")], False),
    ("m", "P", "", [sc(raise_type="SystemExit", raise_msg="x")], False),
    ("m", "P", "", ["T", "C"], True),
    (5, "P", "x", ["T", "C", "P", "S"], False),
]:
    c = case("_provider_list_text",
             args={"current_model": cm, "provider_label": pl, "page_info": pi},
             io={"t": seq})
    if fc:
        c["in"]["expect_fail_closed"] = True
    CASES.append(c)

# ── 9: _provider_button ─────────────────────────────────────────────────────
P9 = [
    {"name": "GPT", "slug": "gpt", "total_models": 3, "is_current": True},
    {"name": "X", "slug": "x", "models": ["a", "b"]},
    {"name": "Y", "slug": "y", "models": []},
    {"name": "Z", "slug": "z"},
    {"name": "Q", "slug": "q", "total_models": 0, "is_current": False},
]
for p in P9:
    CASES.append(case("_provider_button", args={"p": p},
                      io={"InlineKeyboardButton": "BTN"}))
CASES.append(case("_provider_button", args={"p": P9[0]},
                  io={"InlineKeyboardButton": sc(raise_type="RuntimeError", raise_msg="x")}))
CASES.append(case("_provider_button", args={"p": P9[0]},
                  io={"InlineKeyboardButton": sc(raise_type="SystemExit", raise_msg="x")}))

# ── 10: _picker_back_cancel_row ─────────────────────────────────────────────
CASES.append(case("_picker_back_cancel_row",
                  io={"t": ["Back", "Cancel"], "InlineKeyboardButton": ["B", "C"]}))
CASES.append(case("_picker_back_cancel_row",
                  io={"t": ["←", "×"], "InlineKeyboardButton": [{"id": 1}, {"id": 2}]}))
CASES.append(case("_picker_back_cancel_row",
                  io={"t": [sc(raise_type="RuntimeError", raise_msg="x")],
                      "InlineKeyboardButton": ["B", "C"]}))
CASES.append(case("_picker_back_cancel_row",
                  io={"t": ["B", "C"],
                      "InlineKeyboardButton": sc(raise_type="SystemExit", raise_msg="x")}))
c = case("_picker_back_cancel_row", io={"t": ["only"], "InlineKeyboardButton": ["B", "C"]})
c["in"]["expect_fail_closed"] = True
CASES.append(c)
CASES.append(case("_picker_back_cancel_row",
                  io={"t": ["B", "C"], "InlineKeyboardButton": ["x", sc(raise_type="ValueError", raise_msg="x")]}))
CASES.append(case("_picker_back_cancel_row",
                  io={"t": ["B", "C"], "InlineKeyboardButton": "only"}))

# ── 11: _picker_nav_row ─────────────────────────────────────────────────────
CASES.append(case("_picker_nav_row", args={"page": 1, "total_pages": 3, "prefix": "mpv"},
                  io={"t": ["Prev", "Next"], "InlineKeyboardButton": ["P", "M", "N"]}))
CASES.append(case("_picker_nav_row", args={"page": 0, "total_pages": 1, "prefix": "m"},
                  io={"t": [], "InlineKeyboardButton": ["M"]}))
CASES.append(case("_picker_nav_row", args={"page": 0, "total_pages": 3, "prefix": "p"},
                  io={"t": ["Next"], "InlineKeyboardButton": ["M", "N"]}))
CASES.append(case("_picker_nav_row", args={"page": 2, "total_pages": 3, "prefix": "p"},
                  io={"t": ["Prev"], "InlineKeyboardButton": ["P", "M"]}))
CASES.append(case("_picker_nav_row", args={"page": 1, "total_pages": 2, "prefix": "p"},
                  io={"t": [sc(raise_type="SystemExit", raise_msg="x")],
                      "InlineKeyboardButton": ["P", "M", "N"]}))
CASES.append(case("_picker_nav_row", args={"page": 1, "total_pages": 2, "prefix": "p"},
                  io={"t": ["Prev", "Next"],
                      "InlineKeyboardButton": sc(raise_type="RuntimeError", raise_msg="x")}))
c = case("_picker_nav_row", args={"page": 1, "total_pages": 2, "prefix": "p"},
         io={"t": ["Prev"], "InlineKeyboardButton": ["P", "M", "N"]})
c["in"]["expect_fail_closed"] = True
CASES.append(c)

# ── 12: _teardown_started ───────────────────────────────────────────────────
for v in [True, False, None, 1, 0, "yes", ""]:
    CASES.append(case("_teardown_started", state={"_polling_teardown_started": v}, io={}))
CASES.append(case("_teardown_started", state={}, io={}))

# ── 13: send_path_degraded ──────────────────────────────────────────────────
for v in [True, False, None, 1, 0, "x", "", 2.5]:
    CASES.append(case("send_path_degraded", state={"_send_path_degraded": v}, io={}))
CASES.append(case("send_path_degraded", state={}, io={}))

# ── 14: _callback_ctx ───────────────────────────────────────────────────────
Q14 = [
    {"message": {"chat_id": 1, "chat": {"type": "private"}, "message_thread_id": 7},
     "from_user": {"first_name": "Ann"}},
    {"message": {}, "from_user": {}},
    {"from_user": None},
    {"message": None, "from_user": None},
    {"message": {"chat_id": "c"}, "from_user": {"first_name": "B"}},
    {"from_user": {"first_name": "Z"}},
    {"message": {"chat": {"type": "group"}, "message_thread_id": 0},
     "from_user": None},
]
for q in Q14:
    CASES.append(case("_callback_ctx", args={"query": q}, io={}))

# ── 15: supports_draft_streaming ───────────────────────────────────────────
for bot, ct in [
    ({"send_message_draft": True}, "PRIVATE"),
    ({"send_message_draft": True}, "DM"),
    ({"send_message_draft": True}, "group"),
    ({"send_message_draft": True}, None),
    ({}, "dm"),
    (None, "dm"),
    ({"send_message_draft": True}, "private"),
    ({"send_message_draft": None}, "dm"),
]:
    st = {"_bot": bot}
    CASES.append(case("supports_draft_streaming", args={"chat_type": ct}, state=st, io={}))

# ── 16: _fallback_ips ───────────────────────────────────────────────────────
for cfg, val, fc in [
    ({"extra": {"fallback_ips": ["1.1.1.1", "8.8.8.8"]}}, wrap(["ip1", "ip2"]), False),
    ({"extra": {"fallback_ips": "1.1.1.1, 8.8.8.8"}}, wrap(["a"]), False),
    ({"extra": {"fallback_ips": []}}, wrap(["b"]), False),
    ({"extra": {}}, wrap(["c"]), False),
    (None, wrap(["d"]), False),
    ({"extra": {"fallback_ips": [1, 2]}}, wrap(["e"]), False),
    ({"extra": {"fallback_ips": ["x"]}}, sc(raise_type="RuntimeError", raise_msg="x"), False),
]:
    st = {"config": cfg}
    CASES.append(case("_fallback_ips", state=st, io={"parse_fallback_ip_env": val}))

# ── 17: _link_preview_kwargs ────────────────────────────────────────────────
for dis, present, lpo in [
    (False, True, "unused"),
    (True, True, "OBJ"),
    (True, False, "unused"),
    (True, True, sc(raise_type="RuntimeError", raise_msg="x")),
    (True, True, sc(raise_type="SystemExit", raise_msg="x")),
    (None, True, "unused"),
    ("yes", True, "MARK"),
]:
    CASES.append(case("_link_preview_kwargs",
                      state={"_disable_link_previews": dis,
                             "LinkPreviewOptions_present": present},
                      io={"LinkPreviewOptions": lpo}))

# ── 18: _build_adapter ──────────────────────────────────────────────────────
for ta, r, fc in [
    ({"tag": "a"}, "all", False),
    ({"tag": "b"}, "important", False),
    ({}, sc(raise_type="RuntimeError", raise_msg="x"), False),
    ({}, sc(raise_type="SystemExit", raise_msg="x"), False),
    (sc(raise_type="RuntimeError", raise_msg="x"), "all", False),
    ({}, 123, False),
    ({"x": 1}, None, False),
]:
    CASES.append(case("_build_adapter", args={"config": {"token": "t"}},
                      io={"TelegramAdapter": ta, "resolve_notifications_mode": r}))

# ── invariants ──────────────────────────────────────────────────────────────
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
