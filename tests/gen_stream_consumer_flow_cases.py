#!/usr/bin/env python3
"""Generate tests/fixtures/stream_consumer_flow.jsonl — decision-sequence
cases for the gateway/stream_consumer.py FLOW ports.

Each case scripts the seams (send results, clock, text-leaf maps, ceiling
verdicts, queue contents) that drive one live method down a specific branch
path; the C harness and the oracle consume the SAME scripts. Whitespace in
fixtures is ASCII-only (the C str.strip port covers the ASCII set plus the
common Unicode spaces; cases stay inside that intersection).

Run: python tests/gen_stream_consumer_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "stream_consumer_flow.jsonl")

CURSOR = " \u2589"          # DEFAULT_STREAMING_CURSOR " ▉"
CHECK = "\u2705"            # ✅ native empty-finalize placeholder
HOURGLASS = " \u231b"       # " ⏳" multi-byte cursor case


def case(func, state=None, cfg=None, tick=None, queue=None, args=None,
         seams=None, hook_present=False, draft_id_counter=0):
    inn = {}
    if cfg is not None:
        inn["cfg"] = cfg
    if state is not None:
        inn["state"] = state
    if tick is not None:
        inn["tick"] = tick
    if queue is not None:
        inn["queue"] = queue
    if args is not None:
        inn["args"] = args
    if seams is not None:
        inn["seams"] = seams
    if hook_present:
        inn["hook_present"] = True
    if draft_id_counter:
        inn["draft_id_counter"] = draft_id_counter
    return {"func": func, "in": inn}


def txt(s):
    return {"kind": "text", "text": s}


def done():
    return {"kind": "done"}


def seg():
    return {"kind": "new_segment"}


def reopen():
    return {"kind": "reopen_seed"}


def final(s):
    return {"kind": "final_text", "text": s}


def prog(line):
    return {"kind": "tool_progress", "line": line}


def comm(s):
    return {"kind": "commentary", "text": s}


def flush(eid):
    return {"kind": "flush", "event": eid}


def boundary(fid, cancelled=False):
    return {"kind": "approval_boundary", "future": fid, "cancelled": cancelled}


def tmap(m, default=None):
    d = {"map": m}
    if default is not None:
        d["default"] = default
    return d


CASES = []


def add(c):
    CASES.append(c)


# ── _drain_queue: intake / coalescing / sentinel stop ─────────────────────

add(case("drain_queue", queue=[]))
add(case("drain_queue", state={"_use_native_streaming": True},
         queue=[txt("Hello"), txt(" world"), prog("searching\u2026")]))
add(case("drain_queue", queue=[txt("a"), done(), txt("b")]))
add(case("drain_queue", queue=[seg(), txt("x")]))
add(case("drain_queue", queue=[txt("a"), comm("note"), done()]))
add(case("drain_queue", queue=[txt("a"), flush(7), done()]))
add(case("drain_queue",
         state={"_accumulated": "abc", "_stream_ledger": "abc",
                "_turn_split_delivery": True},
         queue=[final("abcdef"), done()]))
add(case("drain_queue", queue=[prog("x"), reopen(), done()]))
add(case("drain_queue", queue=[boundary(3), done()]))
add(case("drain_queue", state={"_ceiling_hit": True}, queue=[txt("abc")]))

# ── _should_edit: throttle timing / flood backoff / deferral ──────────────

add(case("should_edit", tick={"got_done": True}))
add(case("should_edit", cfg={"buffer_only": True}, tick={}))
add(case("should_edit",
         state={"_use_native_streaming": True, "_accumulated": "hi",
                "_settled_for": "hi", "_redacted_prefix": "hi"},
         tick={}))
add(case("should_edit",
         state={"_accumulated": "hello", "_last_edit_time": 9.0,
                "_current_edit_interval": 0.8, "_settled_for": "hello",
                "_redacted_prefix": "hello"},
         tick={}, seams={"clock": [10.0]}))
add(case("should_edit",
         cfg={"buffer_threshold": 24},
         state={"_accumulated": "x" * 30, "_last_edit_time": 9.9,
                "_current_edit_interval": 0.8, "_flood_strikes": 2,
                "_settled_for": "x" * 30, "_redacted_prefix": "x" * 30},
         tick={}, seams={"clock": [10.0]}))
add(case("should_edit",
         cfg={"buffer_threshold": 24},
         state={"_accumulated": "y" * 24, "_last_edit_time": 9.99,
                "_flood_strikes": 0, "_settled_for": "y" * 24,
                "_redacted_prefix": "y" * 24},
         tick={}, seams={"clock": [10.0]}))
add(case("should_edit",
         state={"_use_native_streaming": True, "_accumulated": "NO",
                "_settled_for": "NO", "_redacted_prefix": "NO"},
         tick={},
         seams={"is_partial_silence_marker": tmap({"NO": True})}))
add(case("should_edit",
         state={"_use_native_streaming": True,
                "_accumulated": "answer\n\nNO",
                "_settled_for": "answer\n\nNO",
                "_redacted_prefix": "answer\n\nNO"},
         tick={},
         seams={"ends_with_partial_silence_marker": tmap({"answer\n\nNO": True})}))
add(case("should_edit",
         state={"_accumulated": "text MEDIA:pic.png", "_settled_for": "",
                "_redacted_prefix": ""},
         tick={},
         seams={"redact_settled": [{"text": "text MEDIA:pic.png", "settled": 5}],
                "has_pending_media_directive": tmap({"text ": True})}))
add(case("should_edit",
         state={"_use_draft_streaming": True, "_accumulated": "abc",
                "_settled_for": "abc", "_redacted_prefix": "abc"},
         tick={}, seams={"should_push_draft": [True]}))

# ── _tick_ends_text ───────────────────────────────────────────────────────

add(case("tick_ends_text", tick={"got_done": True}))
add(case("tick_ends_text", tick={"got_segment_break": True}))
add(case("tick_ends_text", tick={"commentary_text": "note"}))
add(case("tick_ends_text", state={"_use_native_streaming": True},
         tick={"commentary_text": "note"}))
add(case("tick_ends_text", state={"_use_draft_streaming": True},
         tick={"commentary_text": "note"},
         seams={"stream_is_message": [True]}))
add(case("tick_ends_text", tick={}))
add(case("tick_ends_text", state={"_use_draft_streaming": True},
         tick={"commentary_text": "note"},
         seams={"stream_is_message": [False]}))

# ── _redact_settled_text ──────────────────────────────────────────────────

add(case("redact_settled_text",
         state={"_accumulated": "abc", "_settled_for": "abc",
                "_redacted_prefix": "ab"},
         args={"done": False}))
add(case("redact_settled_text",
         state={"_accumulated": "abc secret def", "_settled_for": "",
                "_redacted_prefix": ""},
         args={"done": False},
         seams={"redact_settled": [{"text": "abc [REDACTED] def", "settled": 14}]}))
add(case("redact_settled_text",
         state={"_accumulated": "abc", "_settled_for": "abc",
                "_redacted_prefix": "abc"},
         args={"done": True},
         seams={"redact_settled": [{"text": "abc", "settled": 3}]}))
add(case("redact_settled_text",
         state={"_accumulated": "h\u00e9llo w\u00f6rld", "_settled_for": "",
                "_redacted_prefix": ""},
         args={"done": False},
         seams={"redact_settled": [{"text": "h\u00e9llo w\u00f6rld", "settled": 5}]}))
add(case("redact_settled_text",
         state={"_accumulated": "", "_settled_for": "", "_redacted_prefix": ""},
         args={"done": False}))
add(case("redact_settled_text",
         state={"_accumulated": "secret", "_settled_for": ""},
         args={"done": False},
         seams={"redact_settled": [{"text": "[REDACTED]", "settled": 0}]}))

# ── _push_update ──────────────────────────────────────────────────────────

add(case("push_update",
         state={"_accumulated": "hello", "_settled_for": "hello",
                "_redacted_prefix": "hello", "_last_edit_time": 1.0},
         cfg={"cursor": CURSOR}, tick={}, seams={"clock": [5.0]}))
add(case("push_update",
         state={"_use_native_streaming": True, "_accumulated": "body",
                "_settled_for": "body", "_redacted_prefix": "body",
                "_tool_progress_lines": ["working"],
                "_tool_progress_active": True},
         cfg={"cursor": CURSOR}, tick={},
         seams={"compose_frame_content": tmap({"body": "body\n\n---\nworking"}),
                "clock": [7.0]}))
add(case("push_update",
         state={"_use_native_streaming": True, "_accumulated": "",
                "_settled_for": "", "_redacted_prefix": ""},
         cfg={"cursor": CURSOR}, tick={},
         seams={"compose_frame_content": tmap({"": ""}), "clock": [7.5]}))
add(case("push_update",
         state={"_use_draft_streaming": True, "_accumulated": "final",
                "_settled_for": "final", "_redacted_prefix": "final",
                "_message_id": None},
         cfg={"cursor": CURSOR}, tick={"got_done": True},
         seams={"clock": [9.0]}))
add(case("push_update",
         state={"_accumulated": "seg", "_settled_for": "seg",
                "_redacted_prefix": "seg"},
         cfg={"cursor": CURSOR}, tick={"got_segment_break": True},
         seams={"clock": [3.0]}))
add(case("push_update",
         state={"_accumulated": "x", "_settled_for": "x",
                "_redacted_prefix": "x", "_last_edit_time": 1.0,
                "_tool_progress_active": True},
         tick={},
         seams={"send_or_edit": [{"raise": True}], "clock": [99.0]}))
add(case("push_update",
         state={"_accumulated": "hello world", "_settled_for": "",
                "_redacted_prefix": ""},
         cfg={"cursor": CURSOR}, tick={},
         seams={"redact_settled": [{"text": "hello world", "settled": 5}],
                "clock": [4.0]}))

# ── _seal_overflow_heads ──────────────────────────────────────────────────

add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_accumulated": "short",
                "_safe_limit": 100},
         args={}))
add(case("seal_overflow_heads", args={"final": True},
         seams={"ceiling_final_cut": [True]}))
add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_accumulated": "head line\nmore text here",
                "_safe_limit": 10, "_settled_for": "", "_redacted_prefix": ""},
         args={"done": True},
         seams={"redact_settled": [{"text": "head line\nmore text here",
                                    "settled": 24}],
                "custom_unit_to_cp": [10], "send_or_edit": [True]}))
add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_accumulated": "head line\nmore text here",
                "_safe_limit": 10, "_settled_for": "", "_redacted_prefix": ""},
         args={"done": True},
         seams={"redact_settled": [{"text": "head line\nmore text here",
                                    "settled": 24}],
                "custom_unit_to_cp": [10], "send_or_edit": [False]}))
add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_accumulated": "head line\nmore text here",
                "_safe_limit": 10, "_settled_for": "", "_redacted_prefix": ""},
         args={"done": True},
         seams={"redact_settled": [{"text": "head line\nmore text here",
                                    "settled": 24}],
                "custom_unit_to_cp": [10],
                "send_or_edit": [{"ret": True, "fallback": True}]}))
add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_accumulated": "   \nmore text",
                "_safe_limit": 5, "_settled_for": "", "_redacted_prefix": ""},
         args={},
         seams={"redact_settled": [{"text": "   \nmore text", "settled": 13}],
                "custom_unit_to_cp": [5]}))
add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_accumulated": "abcdefghij\nrest",
                "_safe_limit": 8, "_settled_for": "", "_redacted_prefix": ""},
         args={},
         seams={"redact_settled": [{"text": "abcdefghij\nrest", "settled": 4}],
                "custom_unit_to_cp": [8], "send_or_edit": [True]}))
add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_edit_supported": False,
                "_accumulated": "x" * 20, "_safe_limit": 5},
         args={}))
add(case("seal_overflow_heads",
         state={"_message_id": "m1", "_accumulated": "head line\nmore text here",
                "_safe_limit": 10, "_settled_for": "", "_redacted_prefix": ""},
         args={"done": True},
         seams={"redact_settled": [{"text": "head line\nmore text here",
                                    "settled": 24}],
                "custom_unit_to_cp": [10], "ceiling_seal_cut": [True]}))

# ── _split_first_send ─────────────────────────────────────────────────────

add(case("split_first_send", tick={},
         seams={"ceiling_split_first_send": [True]}))
add(case("split_first_send", tick={},
         seams={"ceiling_split_first_send": [False]}))
add(case("split_first_send",
         state={"_accumulated": "   ", "_settled_for": "",
                "_redacted_prefix": ""},
         tick={},
         seams={"redact_settled": [{"text": "   ", "settled": 3}]}))
add(case("split_first_send",
         state={"_accumulated": "head tail", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 100},
         tick={},
         seams={"redact_settled": [{"text": "head tail", "settled": 4}],
                "clock": [5.0], "send_new_chunk": ["h1"],
                "turn_reset": [False]}))
add(case("split_first_send",
         state={"_accumulated": "AAAABBBBCC", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 4,
                "_initial_reply_to_id": "r0"},
         tick={},
         seams={"redact_settled": [{"text": "AAAABBBBCC", "settled": 10}],
                "truncate_for_stream": [["AAAA", "BBBB", "CC"]],
                "send_new_chunk": ["h1", "h2"], "clock": [6.0],
                "turn_reset": [False, False]}))
add(case("split_first_send",
         state={"_accumulated": "AAAABBBBCC", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 4,
                "_initial_reply_to_id": "r0"},
         tick={},
         seams={"redact_settled": [{"text": "AAAABBBBCC", "settled": 10}],
                "truncate_for_stream": [["AAAA", "BBBB", "CC"]],
                "send_new_chunk": ["h1", None], "clock": [6.0],
                "turn_reset": [False, False]}))
add(case("split_first_send",
         state={"_accumulated": "AAAABBBB", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 4},
         tick={"got_done": True},
         seams={"redact_settled": [{"text": "AAAABBBB", "settled": 8}],
                "truncate_for_stream": [["AAAA", "BBBB"]],
                "send_new_chunk": ["__partial__"]}))
add(case("split_first_send",
         state={"_accumulated": "head tail", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 100},
         tick={"got_done": True},
         seams={"redact_settled": [{"text": "head tail", "settled": 4}],
                "send_new_chunk": ["h1"], "send_or_edit": [True],
                "clock": [7.0], "turn_reset": [False],
                "display_payload": tmap({" tail": "tail"})}))
add(case("split_first_send",
         state={"_accumulated": "head", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 100},
         tick={"got_done": True},
         seams={"redact_settled": [{"text": "head", "settled": 4}],
                "send_new_chunk": ["h1"], "clock": [8.0],
                "turn_reset": [False]}))
add(case("split_first_send",
         state={"_accumulated": "AAAABBBBCC", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 4,
                "_initial_reply_to_id": "r0"},
         tick={},
         seams={"redact_settled": [{"text": "AAAABBBBCC", "settled": 10}],
                "truncate_for_stream": [["AAAA", "BBBB", "CC"]],
                "send_new_chunk": ["__no_edit__", "h2"], "clock": [9.0],
                "turn_reset": [False, False]}))
add(case("split_first_send",
         state={"_accumulated": "AAAABBBBCC", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 4,
                "_initial_reply_to_id": "r0"},
         tick={},
         seams={"redact_settled": [{"text": "AAAABBBBCC", "settled": 10}],
                "truncate_for_stream": [["AAAA", "BBBB", "CC"]],
                "send_new_chunk": ["h1", "h2"], "clock": [6.0],
                "turn_reset": [False, True]}))
add(case("split_first_send",
         state={"_accumulated": "head tail", "_settled_for": "",
                "_redacted_prefix": "", "_safe_limit": 100,
                "_fallback_final_send": True, "_fallback_prefix": "pfx"},
         tick={"got_segment_break": True},
         seams={"redact_settled": [{"text": "head tail", "settled": 4}],
                "send_new_chunk": ["h1"], "clock": [10.0],
                "turn_reset": [False]}))# ── _finalize_edit ────────────────────────────────────────────────────────

add(case("finalize_edit", args={"text": "final answer", "record": True},
         seams={"send_or_edit": [True]}))
add(case("finalize_edit", args={"text": "x", "record": False},
         seams={"send_or_edit": [True]}))
add(case("finalize_edit", args={"text": "x", "record": True},
         seams={"send_or_edit": [False]}))
add(case("finalize_edit", args={"text": "x", "record": True},
         seams={"send_or_edit": [{"raise": True}]}))
add(case("finalize_edit",
         state={"_turn_split_delivery": True,
                "_stream_ledger": "full ledger text"},
         args={"text": "tail", "record": True},
         seams={"send_or_edit": [True]}))
add(case("finalize_edit", args={"text": "", "record": True},
         seams={"send_or_edit": [True]}))

# ── _finalize_edit_path: the branch ladder ────────────────────────────────

add(case("finalize_edit_path",
         state={"_fallback_final_send": True, "_accumulated": "unsent"},
         tick={}))
add(case("finalize_edit_path",
         state={"_final_response_sent": True, "_accumulated": "acc"},
         tick={}))
add(case("finalize_edit_path",
         state={"_accumulated": "acc", "_last_sent_text": "acc" + CURSOR},
         cfg={"cursor": CURSOR}, tick={"update_visible": True}))
add(case("finalize_edit_path",
         state={"_adapter_requires_finalize": True,
                "_last_edit_overflowed": True, "_last_sent_text": "acc",
                "_accumulated": "acc"},
         cfg={"cursor": ""}, tick={"update_visible": True}))
add(case("finalize_edit_path",
         state={"_adapter_requires_finalize": True,
                "_last_sent_text": "acc", "_accumulated": "acc"},
         cfg={"cursor": ""},
         tick={"update_visible": True, "draft_final_fresh_send": True}))
add(case("finalize_edit_path",
         state={"_message_id": "m1", "_accumulated": "acc"},
         tick={},
         seams={"send_or_edit": [{"ret": False, "fallback": True}]}))
add(case("finalize_edit_path",
         state={"_message_id": "m1", "_accumulated": "acc"},
         tick={}, seams={"send_or_edit": [True]}))
add(case("finalize_edit_path",
         state={"_accumulated": "acc"},
         tick={}, seams={"send_or_edit": [True]}))
add(case("finalize_edit_path",
         state={"_already_sent": True, "_accumulated": "acc"},
         tick={}))
add(case("finalize_edit_path",
         state={"_adapter_requires_finalize": True, "_message_id": "m1",
                "_accumulated": "acc"},
         tick={"update_visible": True}, seams={"send_or_edit": [True]}))

# ── _finalize_turn ────────────────────────────────────────────────────────

add(case("finalize_turn", tick={}, seams={"ceiling_finish": [True]}))
add(case("finalize_turn",
         state={"_use_native_streaming": True, "_accumulated": "body",
                "_message_id": "m1"},
         tick={"update_visible": True}, hook_present=True,
         seams={"hook": {"ret": "ok"}}))
add(case("finalize_turn",
         state={"_use_native_streaming": True, "_accumulated": ""},
         tick={}, seams={"send_or_edit": [True]}))
add(case("finalize_turn",
         state={"_accumulated": "answer", "_message_id": "m1"},
         tick={"update_visible": True}))
add(case("finalize_turn",
         state={"_use_native_streaming": True,
                "_awaiting_reopen_after_boundary": True,
                "_native_stream_opened": False, "_accumulated": "",
                "_message_id": None},
         tick={}))
add(case("finalize_turn",
         state={"_reopen_seeded_eagerly": True, "_native_stream_opened": True,
                "_use_native_streaming": True, "_accumulated": ""},
         tick={"update_visible": False}))
add(case("finalize_turn",
         state={"_use_native_streaming": True, "_accumulated": "body",
                "_superseded_reply_ids": ["old2", "old1"],
                "_message_id": "m1"},
         tick={"update_visible": True}))
add(case("finalize_turn",
         state={"_accumulated": "x", "_use_native_streaming": True},
         tick={"update_visible": True}, hook_present=True,
         seams={"hook": {"ret": "raise"}}))
add(case("finalize_turn",
         state={"_accumulated": "x", "_use_native_streaming": True},
         tick={"update_visible": True}, hook_present=True,
         seams={"hook": {"ret": "awaitable"}}))
add(case("finalize_turn", tick={}))
add(case("finalize_turn",
         state={"_before_finalize_notified": True, "_accumulated": "x",
                "_use_native_streaming": True},
         tick={"update_visible": True}, hook_present=True))

# ── _end_segment ──────────────────────────────────────────────────────────

add(case("end_segment", state={"_use_native_streaming": True}, tick={}))
add(case("end_segment",
         state={"_accumulated": "pre-boundary", "_message_id": "m1",
                "_message_created_ts": 12.5,
                "_last_sent_text": "pre-boundary"},
         tick={"update_visible": False},
         seams={"stale_preview_ids": [["m1"]]}))
add(case("end_segment",
         state={"_message_id": "__no_edit__", "_accumulated": "text",
                "_last_sent_text": "text"},
         tick={}))
add(case("end_segment",
         state={"_accumulated": "text", "_message_id": "m1",
                "_last_sent_text": "text"},
         tick={"update_visible": True}))
add(case("end_segment",
         state={"_use_draft_streaming": True, "_message_id": "m1",
                "_last_sent_text": "seg", "_accumulated": "seg"},
         tick={"update_visible": True}, draft_id_counter=41,
         seams={"stream_is_message": [False, False],
                "stale_preview_ids": [["m1"]]}))
add(case("end_segment",
         state={"_message_id": "m1", "_last_sent_text": "",
                "_accumulated": ""},
         tick={}))

# ── _on_cancelled ─────────────────────────────────────────────────────────

add(case("on_cancelled", seams={"ceiling_cancelled": [True]}))
add(case("on_cancelled",
         state={"_accumulated": "partial answer", "_message_id": "m1"},
         seams={"send_or_edit": [True]}))
add(case("on_cancelled",
         state={"_accumulated": "partial answer", "_message_id": "m1"},
         seams={"send_or_edit": [{"raise": True}]}))
add(case("on_cancelled",
         state={"_accumulated": "partial answer", "_message_id": "m1"},
         seams={"send_or_edit": [False]}))
add(case("on_cancelled",
         state={"_accumulated": "x", "_message_id": None}))
add(case("on_cancelled",
         state={"_accumulated": "x", "_message_id": ""}))
add(case("on_cancelled",
         state={"_accumulated": "x", "_message_id": "m1",
                "_final_response_sent": True,
                "_delivered_final_text": "prior"},
         seams={"send_or_edit": [True]}))

# ── _adopt_final_text ─────────────────────────────────────────────────────

add(case("adopt_final_text", args={"final_raw": "x"},
         seams={"ceiling_adopt_final": [True]}))
add(case("adopt_final_text", args={"final_raw": "x"}))
add(case("adopt_final_text",
         state={"_accumulated": "old", "_stream_ledger": "old"},
         args={"final_raw": "new final"},
         seams={"clean_for_display": tmap({"new final": "new final",
                                           "old": "old"})}))
add(case("adopt_final_text",
         state={"_accumulated": "same", "_stream_ledger": "same"},
         args={"final_raw": "same"},
         seams={"clean_for_display": tmap({"same": "same"})}))
add(case("adopt_final_text",
         state={"_accumulated": "old", "_stream_ledger": "old"},
         args={"final_raw": "MEDIA:x.png"},
         seams={"clean_for_display": tmap({"MEDIA:x.png": ""})}))
add(case("adopt_final_text",
         state={"_accumulated": "abc", "_stream_ledger": "abc",
                "_turn_split_delivery": True},
         args={"final_raw": "abcdef"}))
add(case("adopt_final_text",
         state={"_accumulated": "abc", "_stream_ledger": "abc",
                "_turn_split_delivery": True},
         args={"final_raw": "xyzabc"}))
add(case("adopt_final_text",
         state={"_accumulated": "abc", "_stream_ledger": "abc",
                "_turn_split_delivery": True},
         args={"final_raw": "abc"}))
add(case("adopt_final_text",
         state={"_accumulated": "NO_REPLY", "_stream_ledger": "NO_REPLY",
                "_message_id": None,
                "_last_segment_delivery": ["m9", 5.0, "the answer", ["m9"]],
                "_delivered_segment_texts": ["the answer"]},
         args={"final_raw": "the answer"},
         seams={"is_intentional_silence_response": tmap({"NO_REPLY": True,
                                                         "the answer": False})}))

# ── _adopt_recovered_reply ────────────────────────────────────────────────

add(case("adopt_recovered_reply",
         state={"_message_id": "m1"}, args={"final_raw": "x"}))
add(case("adopt_recovered_reply",
         state={"_use_native_streaming": True, "_message_id": None},
         args={"final_raw": "x"}))
add(case("adopt_recovered_reply", args={"final_raw": "x"}))
add(case("adopt_recovered_reply",
         state={"_accumulated": "real text",
                "_last_segment_delivery": ["m1", None, "t", ["m1"]],
                "_delivered_segment_texts": ["t"]},
         args={"final_raw": "x"}))
add(case("adopt_recovered_reply",
         state={"_accumulated": "NO_REPLY",
                "_last_segment_delivery": ["m1", None, "t", ["m1"]],
                "_delivered_segment_texts": ["t"]},
         args={"final_raw": "NO_REPLY"},
         seams={"is_intentional_silence_response": tmap({"NO_REPLY": True})}))
add(case("adopt_recovered_reply",
         state={"_accumulated": "NO_REPLY", "_stream_ledger": "NO_REPLY",
                "_last_segment_delivery": ["m9", 5.0, "the answer", ["m9"]],
                "_delivered_segment_texts": ["the answer"]},
         args={"final_raw": "the answer"},
         seams={"is_intentional_silence_response": tmap({"NO_REPLY": True,
                                                         "the answer": False})}))
add(case("adopt_recovered_reply",
         state={"_accumulated": "NO_REPLY", "_stream_ledger": "NO_REPLY",
                "_last_segment_delivery": ["m9", 5.0, "prior text",
                                           ["m9", "h2", "h1"]],
                "_delivered_segment_texts": ["prior text"],
                "_preview_message_ids": ["stale"]},
         args={"final_raw": "new augmented answer"},
         seams={"is_intentional_silence_response": tmap({"NO_REPLY": True})}))
add(case("adopt_recovered_reply",
         state={"_accumulated": "NO_REPLY", "_stream_ledger": "NO_REPLY",
                "_last_segment_delivery": ["m9", 5.0, "prior text",
                                           ["m9", "h1"]],
                "_delivered_segment_texts": []},
         args={"final_raw": "new augmented answer"},
         seams={"is_intentional_silence_response": tmap({"NO_REPLY": True})}))

# ── _wake_flush_waiters ───────────────────────────────────────────────────

add(case("wake_flush_waiters", queue=[]))
add(case("wake_flush_waiters", queue=[flush(1), txt("x"), flush(2)]))
add(case("wake_flush_waiters", queue=[comm("c"), done(), seg()]))
add(case("wake_flush_waiters", queue=[flush(1), flush(2)],
         seams={"flush_set_raise": True}))
add(case("wake_flush_waiters", queue=[boundary(5), flush(3)]))
add(case("wake_flush_waiters", queue=[txt("a"), reopen(), flush(4)]))

# ── _mark_skip_redundant_finalize ─────────────────────────────────────────

add(case("mark_skip_redundant_finalize",
         state={"_last_sent_text": "answer" + CURSOR},
         cfg={"cursor": CURSOR}))
add(case("mark_skip_redundant_finalize",
         state={"_last_sent_text": "", "_accumulated": "acc" + CURSOR},
         cfg={"cursor": CURSOR}))
add(case("mark_skip_redundant_finalize",
         state={"_last_sent_text": "answer"}, cfg={"cursor": ""}))
add(case("mark_skip_redundant_finalize",
         state={"_last_sent_text": "answer"}, cfg={"cursor": CURSOR}))
add(case("mark_skip_redundant_finalize",
         state={"_last_sent_text": "tail" + CURSOR,
                "_turn_split_delivery": True,
                "_stream_ledger": "whole answer"},
         cfg={"cursor": CURSOR}))
add(case("mark_skip_redundant_finalize",
         state={"_last_sent_text": "done" + HOURGLASS},
         cfg={"cursor": HOURGLASS}))


def main():
    # fail-closed: every ported function needs >= 6 cases
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    expected = {
        "drain_queue", "should_edit", "tick_ends_text", "redact_settled_text",
        "push_update", "seal_overflow_heads", "split_first_send",
        "finalize_edit", "finalize_edit_path", "finalize_turn", "end_segment",
        "on_cancelled", "adopt_final_text", "adopt_recovered_reply",
        "wake_flush_waiters", "mark_skip_redundant_finalize",
    }
    assert set(counts) == expected, f"func set drift: {set(counts) ^ expected}"
    for f, n in sorted(counts.items()):
        assert n >= 6, f"{f} has only {n} cases"
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for f, n in sorted(counts.items()):
        print(f"  {f}: {n}")


if __name__ == "__main__":
    main()
