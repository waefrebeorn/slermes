#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 10 oracle.

Writes tests/fixtures/telegram_adapter_flow10.jsonl.  Each case is
{"func", "in": {"args", "state"?, "io"?}}.  Re-running reproduces
byte-identical fixtures.  Every ported function gets >= 6 cases and every seam
name appears in at least one case's io (asserted at the bottom).
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow10.jsonl"

SEAM_NAMES = [
    "admission_get", "BotCommand", "run_bounded_async", "logger_debug",
    "sleep", "refresh_bot_identity", "quotable_text", "record_sent_text",
    "pad_inline_flags", "append_observed_note", "logger_info",
    "_build_message_event", "group_trigger_text", "_cache_replied_media",
    "_apply_telegram_group_observe_attribution", "add_item_note",
    "get_env_value", "truncate_message", "_toast", "answer_callback",
    "edit_message_text", "_format_choice_page", "model_button_labels",
    "InlineKeyboardButton", "_picker_back_cancel_row", "_paged_keyboard",
    "ensure_future", "wait", "task_cancel", "add_done_callback", "task_await",
    "logger_warning", "group_providers", "_provider_button", "t",
]

CASES = []


def case(func, args=None, state=None, io=None, **kw):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    c["in"]["io"] = io if io is not None else {}
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


ALL_IO = set()


def add(c):
    ALL_IO.update((c["in"].get("io") or {}).keys())
    CASES.append(c)


# ── 1: _accept_update ───────────────────────────────────────────────────────
for present, val in [
    (True, {"tag": "c1"}),
    (True, None),
    (False, "unused"),
    (True, sc(raise_type="RuntimeError", raise_msg="boom")),
    (True, sc(raise_type="KeyboardInterrupt", raise_msg="bye")),
    (True, {}),
    (True, {"accepted": False, "n": 2}),
]:
    st = {"_update_admission": {}} if present else {}
    add(case("_accept_update", state=st, io={"admission_get": val}))

# ── 2: _bot_commands ────────────────────────────────────────────────────────
for menu, ret in [
    ([["a", "desc"]], "CMD"),
    ([["x", None]], {"c": 1}),
    ([["y", ""]], None),
    ([["n", 123]], "N"),
    ([["long", "a" * 300]], "LONG"),
    ([["a", "1"], ["b", "2"]], ["A", "B"]),
    ([["z", "x"]], sc(raise_type="RuntimeError", raise_msg="x")),
    ([["z", "x"]], sc(raise_type="SystemExit", raise_msg="x")),
]:
    add(case("_bot_commands", args={"menu_commands": menu}, io={"BotCommand": ret}))

# ── 3: _await_with_thread_deadline ──────────────────────────────────────────
for aw, to, label, res in [
    ("A", 5, "telegram", {"timed_out": False, "value": "ok"}),
    ("B", 1.5, "send", {"timed_out": False, "value": 42}),
    ("C", 0, "telegram", {"timed_out": True}),
    ("D", 2, "init", {"timed_out": False, "value": None}),
    ("E", 3, "telegram", sc(raise_type="RuntimeError", raise_msg="x")),
    ("F", 4, "telegram", sc(raise_type="SystemExit", raise_msg="x")),
    ("G", 6, "custom", {"timed_out": False, "value": [1, 2]}),
]:
    add(case("_await_with_thread_deadline",
             args={"awaitable": aw, "timeout": to, "label": label,
                   "on_abandon": None, "dump_on_blocked_loop": True},
             io={"run_bounded_async": res}))

# ── 4: _bounded_request_step ────────────────────────────────────────────────
for aw, res, dbg in [
    ("A", {"timed_out": False, "value": 1}, "unused"),
    ("B", {"timed_out": True}, "logged"),
    ("C", sc(raise_type="RuntimeError", raise_msg="x"), "logged"),
    ("D", sc(raise_type="SystemExit", raise_msg="x"), "unused"),
    ("E", sc(raise_type="KeyboardInterrupt", raise_msg="x"), "unused"),
    ("F", {"timed_out": False, "value": None}, "unused"),
    ("G", {"timed_out": True}, sc(raise_type="SystemExit", raise_msg="log")),
]:
    add(case("_bounded_request_step", args={"awaitable": aw, "failure_msg": "shutdown %s"},
             state={"name": "bot"}, io={"run_bounded_async": res, "logger_debug": dbg}))

# ── 5: _bot_identity_refresh_loop ───────────────────────────────────────────
BN = "bot"
CANCEL = sc(raise_type="CancelledError", raise_msg="")
for ttl, td, fatal, sleeps, refresh, dbg in [
    (60, False, False, [None, CANCEL], "ok", "unused"),
    (60, True, False, [None], "unused", "unused"),
    (60, False, True, [None], "unused", "unused"),
    (30, False, False, [None, CANCEL], sc(raise_type="RuntimeError", raise_msg="x"), "logged"),
    (60, False, False, [sc(raise_type="RuntimeError", raise_msg="x"), CANCEL], "ok", "logged"),
    (60, False, False, [CANCEL], "unused", "unused"),
    (60, False, False, [sc(raise_type="SystemExit", raise_msg="x")], "unused", "unused"),
]:
    st = {"_BOT_IDENTITY_TTL_SECONDS": ttl, "name": BN,
          "_teardown_started": td, "has_fatal_error": fatal}
    add(case("_bot_identity_refresh_loop", state=st,
             io={"sleep": sleeps, "refresh_bot_identity": refresh, "logger_debug": dbg}))

# ── 6: _after_legacy_edit ───────────────────────────────────────────────────
for chat, mid, content, finalize, qt, rst in [
    ("c1", "m1", "hello", True, "qtext", "ok"),
    ("c1", "m1", "hello", False, "unused", "unused"),
    (5, 7, "x", True, "Q", "ok"),
    ("c", "m", "", True, "", "ok"),
    ("c", "m", "x", True, sc(raise_type="RuntimeError", raise_msg="x"), "unused"),
    ("c", "m", "x", True, "q", sc(raise_type="SystemExit", raise_msg="x")),
    ("c", "m", "x", False, "unused", sc(raise_type="KeyboardInterrupt", raise_msg="x")),
]:
    add(case("_after_legacy_edit",
             args={"chat_id": chat, "message_id": mid, "content": content,
                   "plain": content, "finalize": finalize, "saturated_preview": False},
             io={"quotable_text": qt, "record_sent_text": rst}))

# ── 7: _attach_cached ───────────────────────────────────────────────────────
MAP = {"image": "PHOTO", "video": "VIDEO", "audio": "AUDIO"}
for ev, cached, note, retype, pf, an, li in [
    ({"media_text_inlined": [], "media_urls": [], "media_types": [], "text": "t"},
     {"path": "p", "media_type": "image/png", "kind": "image"}, "note", True,
     wrap([False, True]), "t+note", "logged"),
    ({"media_text_inlined": [True], "media_urls": ["old"], "media_types": ["x"], "text": "a"},
     {"path": "p2", "media_type": "audio/mpeg", "kind": "audio"}, "n2", True,
     wrap([True, False]), "a+n2", "logged"),
    ({"media_text_inlined": [], "media_urls": [], "media_types": [], "text": "t"},
     {"path": "p", "media_type": "v", "kind": "video"}, "note", False,
     wrap([]), "t+note", "logged"),
    ({"media_text_inlined": [], "media_urls": [], "media_types": [], "text": "t"},
     {"path": "p", "media_type": "v", "kind": "unknown"}, "note", True,
     wrap([]), "t+note", "logged"),
    ({"media_text_inlined": [], "media_urls": [], "media_types": [], "text": "t"},
     {"path": "p", "media_type": "v", "kind": "image"}, "note", True,
     sc(raise_type="RuntimeError", raise_msg="x"), "unused", "unused"),
    ({"media_text_inlined": [], "media_urls": [], "media_types": [], "text": "t"},
     {"path": "p", "media_type": "v", "kind": "image"}, "note", True,
     wrap([]), sc(raise_type="SystemExit", raise_msg="x"), "unused"),
    ({"media_text_inlined": [], "media_urls": [], "media_types": [], "text": "t"},
     {"path": "p", "media_type": "v", "kind": "image"}, "note", True,
     wrap([]), "t+note", sc(raise_type="KeyboardInterrupt", raise_msg="x")),
]:
    add(case("_attach_cached",
             args={"event": ev, "cached": cached, "note": note,
                   "log_fmt": "cached %s %s", "retype": retype},
             state={"_CACHED_KIND_TO_MESSAGE_TYPE": MAP},
             io={"pad_inline_flags": pf, "append_observed_note": an, "logger_info": li}))

# ── 8: _build_triggered_event ───────────────────────────────────────────────
EV = {"text": "orig", "message_type": "TEXT"}
MSG = {"message_id": 7}
for upd, be, gt, cm, ap in [
    ({"update_id": 99}, dict(EV), "cleaned", "ok", "final"),
    ({"update_id": 0}, dict(EV), "", "ok", "final"),
    ({"update_id": 1}, sc(raise_type="RuntimeError", raise_msg="x"), "u", "u", "u"),
    ({"update_id": 1}, dict(EV), sc(raise_type="SystemExit", raise_msg="x"), "u", "u"),
    ({"update_id": 1}, dict(EV), "cleaned", sc(raise_type="RuntimeError", raise_msg="x"), "u"),
    ({"update_id": 1}, dict(EV), "cleaned", "ok", sc(raise_type="KeyboardInterrupt", raise_msg="x")),
    ({"update_id": 2}, {"text": "x"}, "y", "ok", {"final": True}),
]:
    add(case("_build_triggered_event",
             args={"msg": MSG, "update": upd, "msg_type": "TEXT"},
             io={"_build_message_event": be, "group_trigger_text": gt,
                 "_cache_replied_media": cm,
                 "_apply_telegram_group_observe_attribution": ap}))

# ── 9: _note_instead_of_media ───────────────────────────────────────────────
for tx, ret in [
    ("note", "ok"),
    ("", "ok"),
    (None, "ok"),
    ("x", sc(raise_type="RuntimeError", raise_msg="x")),
    ("x", sc(raise_type="SystemExit", raise_msg="x")),
    (5, "ok"),
    ("y", {"added": True}),
]:
    add(case("_note_instead_of_media", args={"event": {"id": 1}, "text": tx},
             io={"add_item_note": ret}))

# ── 10: _is_connected ───────────────────────────────────────────────────────
for token, env, in [
    ("tok", "unused"),
    ("  ", "unused"),
    (None, "abc"),
    (None, ""),
    (None, "  spaced  "),
    (None, sc(raise_type="RuntimeError", raise_msg="x")),
    ("", sc(raise_type="KeyboardInterrupt", raise_msg="x")),
    (5, "unused"),
]:
    add(case("_is_connected", args={"config": {"token": token}},
             io={"get_env_value": env}))

# ── 11: _truncate_stream_overflow_preview ───────────────────────────────────
for content, ret in [
    ("hello", wrap(["hel"])),
    ("", wrap([""])),
    ("x" * 100, wrap(["xxx", "rest"])),
    ("abc", wrap([])),
    ("abc", sc(raise_type="RuntimeError", raise_msg="x")),
    ("abc", sc(raise_type="SystemExit", raise_msg="x")),
    (None, wrap([None])),
]:
    add(case("_truncate_stream_overflow_preview", args={"content": content},
             state={"MAX_MESSAGE_LENGTH": 4096}, io={"truncate_message": ret}))

# ── 12: _parse_page ─────────────────────────────────────────────────────────
for raw, toast, ans in [
    ("5", "unused", "unused"),
    ("-2", "unused", "unused"),
    (" 7 ", "unused", "unused"),
    ("abc", "TOAST", "ok"),
    ("", "TOAST", "ok"),
    ("3.5", "TOAST", sc(raise_type="RuntimeError", raise_msg="x")),
    ("z", sc(raise_type="SystemExit", raise_msg="x"), "unused"),
]:
    add(case("_parse_page", args={"query": {"id": 1}, "raw": raw},
             io={"_toast": toast, "answer_callback": ans}))

# ── 13: _edit_html_quiet ────────────────────────────────────────────────────
for tx, ret in [
    ("a", "ok"),
    ("", {"id": 1}),
    ("x", sc(raise_type="RuntimeError", raise_msg="x")),
    ("x", sc(raise_type="ValueError", raise_msg="x")),
    ("x", sc(raise_type="SystemExit", raise_msg="x")),
    ("x", sc(raise_type="KeyboardInterrupt", raise_msg="x")),
    (None, "ok"),
]:
    add(case("_edit_html_quiet", args={"query": {"id": 2}, "text": tx},
             io={"edit_message_text": ret}))

# ── 14: _build_model_keyboard ───────────────────────────────────────────────
MF = wrap([["m1", "m2"], {"start": 0, "page": 0, "total_pages": 1, "page_info": "p"}])
for models, page, fp, labels, ikb, tail, paged in [
    (["a", "b"], 0, MF, wrap(["A", "B"]), ["B1", "B2"], wrap(["back", "cancel"]), "RESULT"),
    (["a"], 1, wrap([["a"], {"start": 0, "page": 1, "total_pages": 2, "page_info": "q"}]),
     wrap(["A"]), ["B1"], wrap(["back", "cancel"]), "R2"),
    ([], 0, wrap([[], {"start": 0, "page": 0, "total_pages": 1, "page_info": ""}]),
     wrap([]), "unused", wrap(["b", "c"]), "R3"),
    (["x"], 0, sc(raise_type="RuntimeError", raise_msg="x"), "u", "u", "u", "u"),
    (["x"], 0, MF, sc(raise_type="SystemExit", raise_msg="x"), "u", "u", "u"),
    (["x"], 0, MF, wrap(["X"]), sc(raise_type="RuntimeError", raise_msg="x"), "u", "u"),
    (["x"], 0, MF, wrap(["X"]), "B", sc(raise_type="SystemExit", raise_msg="x"), "u"),
]:
    add(case("_build_model_keyboard",
             args={"models": models, "page": page, "provider": None},
             state={"_MODEL_PAGE_SIZE": 8},
             io={"_format_choice_page": fp, "model_button_labels": labels,
                 "InlineKeyboardButton": ikb, "_picker_back_cancel_row": tail,
                 "_paged_keyboard": paged}))

# ── 15: _await_disconnect_step ──────────────────────────────────────────────
for aw, to, step, ens, wait, awt, canc, cb, warn in [
    ("A", 2.0, "close", {"t": 1}, {"done": True}, "ok", "unused", "unused", "unused"),
    ("B", 2.0, "stop", {"t": 2}, {"done": False}, "unused", "ok", "ok", "logged"),
    ("C", 0, "x", {"t": 3}, {"done": False}, "unused", "ok", "ok", "logged"),
    ("D", 1.0, "y", {"t": 4}, sc(raise_type="CancelledError", raise_msg=""),
     "unused", "ok", "ok", "unused"),
    ("E", 1.0, "z", {"t": 5}, {"done": True},
     sc(raise_type="RuntimeError", raise_msg="x"), "unused", "unused", "unused"),
    ("F", 1.0, "w", sc(raise_type="RuntimeError", raise_msg="x"),
     "u", "u", "u", "u", "u"),
    ("G", 1.0, "v", {"t": 6}, {"done": True},
     sc(raise_type="CancelledError", raise_msg=""), "unused", "unused", "unused"),
]:
    add(case("_await_disconnect_step",
             args={"awaitable": aw, "timeout": to, "step": step},
             state={"name": "bot"},
             io={"ensure_future": ens, "wait": wait, "task_await": awt,
                 "task_cancel": canc, "add_done_callback": cb, "logger_warning": warn}))

# ── 16: _build_provider_keyboard ───────────────────────────────────────────
PROVIDERS = [
    {"slug": "openai", "name": "OpenAI", "total_models": 3, "is_current": True},
    {"slug": "kimi", "name": "Kimi", "models": ["a", "b"], "is_current": False},
]
GR = wrap([
    {"kind": "group", "label": "Kimi/Moonshot", "members": ["kimi"],
     "group_id": "kimi-g", "is_current": False},
    {"kind": "single", "slug": "openai"},
])
PFP = wrap([["b1", "b2"], {"start": 0, "page": 0, "total_pages": 1, "page_info": "p"}])
for present, providers, gp, ikb, pb, fp, tt, paged in [
    (True, PROVIDERS, GR, ["GROUPBTN", "SINGLEBTN"], "unused", PFP, "Cancel", "R"),
    (True, PROVIDERS, GR, ["G", "S"], sc(raise_type="RuntimeError", raise_msg="x"), "u", "u", "u"),
    (True, PROVIDERS, sc(raise_type="SystemExit", raise_msg="x"), "u", "u", "u", "u", "u"),
    (True, PROVIDERS, GR, sc(raise_type="RuntimeError", raise_msg="x"), "u", "u", "u", "u"),
    (False, PROVIDERS, "unused", "unused", ["P1", "P2"], PFP, "Cancel", "R2"),
    (True, PROVIDERS, GR, ["G", "S"], "unused", sc(raise_type="RuntimeError", raise_msg="x"), "u", "u"),
    (True, PROVIDERS, GR, ["G", "S"], "unused", PFP, sc(raise_type="SystemExit", raise_msg="x"), "u"),
]:
    st = {"group_providers_present": present, "_PROVIDER_PAGE_SIZE": 10}
    add(case("_build_provider_keyboard", args={"providers": providers, "page": 0},
             state=st,
             io={"group_providers": gp, "InlineKeyboardButton": ikb,
                 "_provider_button": pb, "_format_choice_page": fp,
                 "t": tt, "_paged_keyboard": paged}))

# ── invariants ──────────────────────────────────────────────────────────────
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
assert all(v >= 6 for v in counts.values()), counts
assert ALL_IO == set(SEAM_NAMES), (sorted(set(SEAM_NAMES) - ALL_IO), sorted(ALL_IO - set(SEAM_NAMES)))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
print(f"wrote {len(CASES)} cases over {len(counts)} funcs to {OUT}")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
