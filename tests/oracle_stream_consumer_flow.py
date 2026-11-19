#!/usr/bin/env python3
"""Oracle for the gateway/stream_consumer.py streaming-delivery FLOW cores
(src/cli/port_stream_consumer_flow.c).

Drives the LIVE GatewayStreamConsumer with SCRIPTED I/O: the class and its
module constants are AST-extracted from the quarry (no heavy gateway import),
the four mixin bases are stub classes, and every mixin method / clock / text
leaf the 16 flows touch is replaced by a recording monkeypatch stub returning
the SAME scripted results the C harness feeds its seams. Compares the DECISION
SEQUENCE (ordered seam trace) plus the return value, the mutated tick and the
fixed state-key snapshot. Async awaits run under asyncio.run; a scripted
{"raise": true} send models the live exception unwind (ret {"__raise__": ...}).

Input: stdin JSON lines from t_port_stream_consumer_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE stream_consumer_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import asyncio
import concurrent.futures
import contextlib
import inspect
import json
import logging
import os
import queue
import secrets
import sys
import threading
import types
import uuid
import __future__
from dataclasses import dataclass, field
from typing import Any, Callable, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "stream_consumer.py")

# short case name -> live method name (all on GatewayStreamConsumer)
FUNCS = {
    "drain_queue": "_drain_queue",
    "should_edit": "_should_edit",
    "tick_ends_text": "_tick_ends_text",
    "redact_settled_text": "_redact_settled_text",
    "push_update": "_push_update",
    "seal_overflow_heads": "_seal_overflow_heads",
    "split_first_send": "_split_first_send",
    "finalize_edit": "_finalize_edit",
    "finalize_edit_path": "_finalize_edit_path",
    "finalize_turn": "_finalize_turn",
    "end_segment": "_end_segment",
    "on_cancelled": "_on_cancelled",
    "adopt_final_text": "_adopt_final_text",
    "adopt_recovered_reply": "_adopt_recovered_reply",
    "wake_flush_waiters": "_wake_flush_waiters",
    "mark_skip_redundant_finalize": "_mark_skip_redundant_finalize",
}
ASYNC_FUNCS = {
    "push_update", "seal_overflow_heads", "split_first_send", "finalize_edit",
    "finalize_edit_path", "finalize_turn", "end_segment", "on_cancelled",
}
TICK_FUNCS = {
    "should_edit", "tick_ends_text", "push_update", "split_first_send",
    "finalize_edit_path", "finalize_turn", "end_segment",
}

MISSING = object()   # distinguishes "no script entry" from a scripted null

CUR = {"in": {}, "trace": [], "idx": {}, "consumer": None, "tick": None}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


def seams():
    return cin("seams") or {}


def seq_next(key):
    arr = seams().get(key)
    if not isinstance(arr, list) or not arr:
        return MISSING
    i = CUR["idx"].get(key, 0)
    if i >= len(arr):
        i = len(arr) - 1          # past the end: repeat last
    CUR["idx"][key] = i + 1
    return arr[i]


def map_get(key, text):
    m = seams().get(key)
    if not isinstance(m, dict):
        return None
    mp = m.get("map")
    if isinstance(mp, dict) and text in mp:
        return mp[text]
    return m.get("default")


def node_bool(e, default):
    if e is MISSING or e is None:
        return default
    if isinstance(e, bool):
        return e
    if isinstance(e, (int, float)):
        return e != 0
    return default


def fatal(msg):
    print(f"ORACLE stream_consumer_flow: FATAL {msg}", file=sys.stderr)
    sys.exit(1)


class NoopLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


class EventStub:
    """threading.Event double: identity + scripted set() (may raise)."""
    def __init__(self, eid):
        self.eid = eid

    def set(self):
        raised = bool(seams().get("flush_set_raise", False))
        rec({"seam": "flush_event_set", "event": self.eid, "raised": raised})
        if raised:
            raise RuntimeError("scripted event.set failure")


class FutureStub:
    def __init__(self, fid):
        self.fid = fid


def tick_json(t):
    if t is None:
        return None
    fe = t.flush_event
    ab = t.approval_boundary
    return {
        "got_done": t.got_done,
        "got_segment_break": t.got_segment_break,
        "got_flush": t.got_flush,
        "flush_event": fe.eid if fe is not None else None,
        "got_reopen_seed": t.got_reopen_seed,
        "approval_boundary": [ab[0].fid, ab[1]["cancelled"]] if ab is not None else None,
        "commentary_text": t.commentary_text,
        "update_visible": t.update_visible,
        "draft_final_fresh_send": t.draft_final_fresh_send,
    }


def make_tick(ns, d):
    t = ns["_Tick"]()
    d = d or {}
    t.got_done = bool(d.get("got_done", False))
    t.got_segment_break = bool(d.get("got_segment_break", False))
    t.got_flush = bool(d.get("got_flush", False))
    fe = d.get("flush_event")
    t.flush_event = EventStub(int(fe)) if fe is not None else None
    t.got_reopen_seed = bool(d.get("got_reopen_seed", False))
    ab = d.get("approval_boundary")
    t.approval_boundary = (FutureStub(int(ab[0])), {"cancelled": bool(ab[1])}) if ab is not None else None
    t.commentary_text = d.get("commentary_text")
    t.update_visible = bool(d.get("update_visible", False))
    t.draft_final_fresh_send = bool(d.get("draft_final_fresh_send", False))
    return t


def queue_obj(ns, d):
    k = d["kind"]
    if k == "text": return d["text"]
    if k == "done": return ns["_DONE"]
    if k == "new_segment": return ns["_NEW_SEGMENT"]
    if k == "reopen_seed": return ns["_REOPEN_SEED"]
    if k == "final_text": return (ns["_FINAL_TEXT"], d["text"])
    if k == "tool_progress": return (ns["_TOOL_PROGRESS"], d["line"])
    if k == "commentary": return (ns["_COMMENTARY"], d["text"])
    if k == "flush": return (ns["_FLUSH"], EventStub(int(d["event"])))
    if k == "approval_boundary":
        return (ns["_APPROVAL_BOUNDARY"], FutureStub(int(d["future"])),
                {"cancelled": bool(d.get("cancelled", False))})
    fatal(f"unknown queue kind {k!r}")


def queue_desc(ns, item):
    for name, kind in (("_DONE", "done"), ("_NEW_SEGMENT", "new_segment"),
                       ("_REOPEN_SEED", "reopen_seed")):
        if item is ns[name]:
            return {"kind": kind}
    if isinstance(item, str):
        return {"kind": "text", "text": item}
    if isinstance(item, tuple) and item:
        k = item[0]
        if k is ns["_FINAL_TEXT"]: return {"kind": "final_text", "text": item[1]}
        if k is ns["_TOOL_PROGRESS"]: return {"kind": "tool_progress", "line": item[1]}
        if k is ns["_COMMENTARY"]: return {"kind": "commentary", "text": item[1]}
        if k is ns["_FLUSH"]: return {"kind": "flush", "event": item[1].eid}
        if k is ns["_APPROVAL_BOUNDARY"]:
            return {"kind": "approval_boundary", "future": item[1].fid,
                    "cancelled": item[2]["cancelled"]}
    fatal(f"unmappable queue item {item!r}")


# ── extraction ─────────────────────────────────────────────────────────────

def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {
        "__name__": "gateway_stream_consumer_extracted",
        "asyncio": asyncio,
        "concurrent": types.SimpleNamespace(futures=concurrent.futures),
        "contextlib": contextlib, "inspect": inspect, "logging": logging,
        "queue": queue, "secrets": secrets, "threading": threading,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0),
        "uuid": uuid, "dataclass": dataclass, "field": field,
        "Any": Any, "Callable": Callable, "Optional": Optional,
    }
    flag = __future__.annotations.compiler_flag

    def x(node, srcname=SRC):
        exec(compile(ast.Module(body=[node], type_ignores=[]), srcname,
                     "exec", flags=flag), ns)

    # config defaults from the LIVE gateway/config.py (AST only — no imports)
    cfgpath = os.path.join(QUARRY, "gateway", "config.py")
    cfgtree = ast.parse(open(cfgpath, encoding="utf-8").read())
    want = {"DEFAULT_STREAMING_EDIT_INTERVAL", "DEFAULT_STREAMING_BUFFER_THRESHOLD",
            "DEFAULT_STREAMING_CURSOR"}
    found = set()
    for node in cfgtree.body:
        names = []
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names = [node.target.id]
        if want.intersection(names) and getattr(node, "value", None):
            x(node, cfgpath)
            found.update(want.intersection(names))
    if found != want:
        fatal(f"config constants missing: {sorted(want - found)}")
    for n in want:
        ns["_" + n] = ns[n]

    # module constants: sentinels, _FUTURE_TYPES, boundary placeholder, logger
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            x(node)
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) and node.value:
            x(node)
    ns["logger"] = NoopLogger()

    # dataclasses the flows construct / return
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("StreamConsumerConfig", "_Tick"):
            x(node)

    # stub mixin bases: every mixin method the flows call is patched below
    for base in ("StreamTransportMixin", "StreamFallbackMixin",
                 "StreamThinkFilterMixin", "StreamCeilingMixin"):
        ns[base] = type(base, (), {})

    # module-level leaves the class bodies reference (seamed or out of scope)
    ns["_redact_for_egress"] = lambda s: s
    ns["_BasePlatformAdapter"] = types.SimpleNamespace(
        strip_media_directives_for_display=lambda s: s)
    ns["_custom_unit_to_cp"] = lambda text, limit, len_fn: limit
    ns["_is_intentional_silence_response"] = lambda s: False
    ns["_is_partial_silence_marker"] = lambda s: False
    ns["_ends_with_partial_silence_marker"] = lambda s: False
    ns["_strip_trailing_silence_marker"] = lambda s: s
    ns["has_pending_media_directive"] = lambda s: False
    ns["strip_image_tags_for_display"] = lambda s: s
    ns["ensure_closed_code_fences"] = lambda s: s
    ns["redact_settled"] = lambda text, prefix, done=False: (text, len(text))
    ns["redact_to_fixpoint"] = lambda s: s
    ns["reply_file_off"] = contextlib.nullcontext

    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayStreamConsumer":
            x(node)
    cls = ns.get("GatewayStreamConsumer")
    if cls is None:
        fatal("GatewayStreamConsumer class not extracted")
    for short, live in FUNCS.items():
        if not hasattr(cls, live):
            fatal(f"live method {live} missing from extracted class")
    return ns


# ── recording seam stubs (same scripts / event shapes as the C harness) ────

def install_seams(ns):
    cls = ns["GatewayStreamConsumer"]

    def clock_stub():
        e = seq_next("clock")
        v = float(e) if isinstance(e, (int, float)) and not isinstance(e, bool) else 0.0
        rec({"seam": "clock", "ret": v})
        return v
    ns["time"] = types.SimpleNamespace(monotonic=clock_stub)

    def bool_map(key):
        def f(text):
            e = map_get(key, text)
            r = bool(e) if isinstance(e, bool) else (e != 0 if isinstance(e, (int, float)) else False)
            rec({"seam": key, "text": text, "ret": r})
            return r
        return f
    ns["_is_intentional_silence_response"] = bool_map("is_intentional_silence_response")
    ns["_is_partial_silence_marker"] = bool_map("is_partial_silence_marker")
    ns["_ends_with_partial_silence_marker"] = bool_map("ends_with_partial_silence_marker")
    ns["has_pending_media_directive"] = bool_map("has_pending_media_directive")

    def text_map(key):
        def f(text):
            e = map_get(key, text)
            r = e if isinstance(e, str) else text
            rec({"seam": key, "text": text, "ret": r})
            return r
        return f

    def display_payload(self, text):
        return text_map("display_payload")(text)
    cls._display_payload = display_payload

    def compose_frame_content(self, text):
        return text_map("compose_frame_content")(text)
    cls._compose_frame_content = compose_frame_content

    cls._clean_for_display = staticmethod(text_map("clean_for_display"))
    cls._display_text = staticmethod(text_map("display_text"))

    def stream_is_message(self):
        r = node_bool(seq_next("stream_is_message"), False)
        rec({"seam": "stream_is_message", "ret": r})
        return r
    cls._stream_is_message = stream_is_message

    def redact_settled_stub(text, prefix, done=False):
        e = seq_next("redact_settled")
        nt, stl = text, len(text)
        if isinstance(e, dict):
            nt = e.get("text", text)
            stl = e.get("settled", stl)
        rec({"seam": "redact_settled", "text": text, "prefix": prefix,
             "done": done, "ret": {"text": nt, "settled": stl}})
        return nt, stl
    ns["redact_settled"] = redact_settled_stub

    def custom_unit_to_cp_stub(text, limit, len_fn):
        e = seq_next("custom_unit_to_cp")
        r = int(e) if isinstance(e, (int, float)) and not isinstance(e, bool) else limit
        rec({"seam": "custom_unit_to_cp", "text": text, "limit": limit, "ret": r})
        return r
    ns["_custom_unit_to_cp"] = custom_unit_to_cp_stub

    def len_stub(text):
        e = map_get("len_fn", text)
        r = int(e) if isinstance(e, (int, float)) and not isinstance(e, bool) else len(text)
        rec({"seam": "len_fn", "text": text, "ret": r})
        return r
    CUR["len_stub"] = len_stub

    async def send_or_edit(self, text, *, finalize=False, is_turn_final=True,
                           redacted=False):
        e = seq_next("send_or_edit")
        ret, do_raise = True, False
        if isinstance(e, bool):
            ret = e
        elif isinstance(e, dict):
            ret = e.get("ret", True)
            do_raise = e.get("raise", False)
            if "fallback" in e:
                self._fallback_final_send = bool(e["fallback"])
            if "message_id" in e:
                self._message_id = e["message_id"]
        ev = {"seam": "send_or_edit", "text": text, "finalize": finalize,
              "is_turn_final": is_turn_final, "redacted": redacted}
        if do_raise:
            ev["raised"] = True
            rec(ev)
            raise RuntimeError("scripted send failure")
        ev["ret"] = ret
        rec(ev)
        return ret
    cls._send_or_edit = send_or_edit

    async def send_new_chunk(self, text, reply_to_id, *, final=False, first=False):
        e = seq_next("send_new_chunk")
        ret = "m1"
        if isinstance(e, str):
            ret = e
        elif e is None:
            ret = None
        rec({"seam": "send_new_chunk", "text": text, "reply_to": reply_to_id,
             "final": final, "first": first, "ret": ret})
        return ret
    cls._send_new_chunk = send_new_chunk

    async def send_fallback_final(self, text):
        rec({"seam": "send_fallback_final", "text": text})
    cls._send_fallback_final = send_fallback_final

    async def send_commentary(self, text):
        rec({"seam": "send_commentary", "text": text})
    cls._send_commentary = send_commentary

    def flush_think_buffer(self):
        rec({"seam": "flush_think_buffer"})
    cls._flush_think_buffer = flush_think_buffer

    def filter_accumulate(self, text):
        rec({"seam": "filter_accumulate", "text": text})
        # scripted append effect — identical to the C harness stub
        if self._tool_progress_lines:
            self._tool_progress_lines.clear()
            self._tool_progress_active = False
        if not getattr(self, "_ceiling_hit", False):
            self._accumulated += text
        self._stream_ledger += text
    cls._filter_and_accumulate = filter_accumulate

    def ceiling_adopt_final(self, final_raw):
        r = node_bool(seq_next("ceiling_adopt_final"), False)
        rec({"seam": "ceiling_adopt_final", "final_raw": final_raw, "ret": r})
        return r
    cls._ceiling_adopt_final = ceiling_adopt_final

    async def ceiling_split_first_send(self, tick):
        e = seq_next("ceiling_split_first_send")
        r = None if e is MISSING or e is None else bool(e)
        rec({"seam": "ceiling_split_first_send", "tick": tick_json(tick), "ret": r})
        return r
    cls._ceiling_split_first_send = ceiling_split_first_send

    def ceiling_final_cut(self):
        r = node_bool(seq_next("ceiling_final_cut"), False)
        rec({"seam": "ceiling_final_cut", "ret": r})
        return r
    cls._ceiling_final_cut = ceiling_final_cut

    def ceiling_seal_cut(self, split_at, *, final):
        r = node_bool(seq_next("ceiling_seal_cut"), False)
        rec({"seam": "ceiling_seal_cut", "split_at": split_at, "final": final, "ret": r})
        return r
    cls._ceiling_seal_cut = ceiling_seal_cut

    async def ceiling_finish(self, tick):
        r = node_bool(seq_next("ceiling_finish"), False)
        rec({"seam": "ceiling_finish", "tick": tick_json(tick), "ret": r})
        return r
    cls._ceiling_finish = ceiling_finish

    async def ceiling_segment_end(self, tick=None):
        rec({"seam": "ceiling_segment_end", "tick": tick_json(tick)})
    cls._ceiling_segment_end = ceiling_segment_end

    async def ceiling_cancelled(self):
        r = node_bool(seq_next("ceiling_cancelled"), False)
        rec({"seam": "ceiling_cancelled", "ret": r})
        return r
    cls._ceiling_cancelled = ceiling_cancelled

    def reset_ceiling(self):
        # the live _reset_ceiling clears _ceiling_hit (observable state)
        self._ceiling_hit = False
        rec({"seam": "ceiling_reset"})
    cls._reset_ceiling = reset_ceiling

    def turn_reset(self):
        r = node_bool(seq_next("turn_reset"), False)
        rec({"seam": "turn_reset", "ret": r})
        return r
    cls._turn_reset = turn_reset

    def stale_preview_ids(self, *, segment_only=False):
        e = seq_next("stale_preview_ids")
        ids = sorted(set(e)) if isinstance(e, list) else []
        rec({"seam": "stale_preview_ids", "segment_only": segment_only, "ret": ids})
        return set(ids)
    cls._stale_preview_ids = stale_preview_ids

    async def delete_previews(self, stale_ids, *, skip=None, label,
                              retry_on_false=False, skip_sentinel=True):
        rec({"seam": "delete_previews", "ids": sorted(stale_ids),
             "skip": skip, "label": label})
    cls._delete_previews = delete_previews

    async def abandon_native_stream(self):
        rec({"seam": "abandon_native_stream"})
    cls._abandon_native_stream = abandon_native_stream

    async def close_empty_native_bubble(self, fail_log):
        rec({"seam": "close_empty_native_bubble", "fail_log": fail_log})
    cls._close_empty_native_bubble = close_empty_native_bubble

    async def flush_segment_tail_on_edit_failure(self):
        rec({"seam": "flush_segment_tail_on_edit_failure"})
    cls._flush_segment_tail_on_edit_failure = flush_segment_tail_on_edit_failure

    def should_push_draft(self):
        r = node_bool(seq_next("should_push_draft"), False)
        rec({"seam": "should_push_draft", "ret": r})
        return r
    cls._should_push_draft = should_push_draft

    def truncate_for_stream(self, text, limit, len_fn):
        e = seq_next("truncate_for_stream")
        chunks = list(e) if isinstance(e, list) else [text, ""]
        rec({"seam": "truncate_for_stream", "text": text, "limit": limit, "ret": chunks})
        return chunks
    cls._truncate_for_stream = truncate_for_stream

    def split_text_chunks(text, limit, len_fn=len):
        e = seq_next("split_text_chunks")
        chunks = list(e) if isinstance(e, list) else [text, ""]
        rec({"seam": "split_text_chunks", "text": text, "limit": limit, "ret": chunks})
        return chunks
    cls._split_text_chunks = staticmethod(split_text_chunks)

    def hook_stub():
        h = seams().get("hook")
        r = h.get("ret", "ok") if isinstance(h, dict) else "ok"
        rec({"seam": "before_finalize_hook", "ret": r})
        if r == "raise":
            raise RuntimeError("scripted hook failure")
        if r == "awaitable":
            async def coro():
                rec({"seam": "hook_await"})
            return coro()
        return None
    CUR["hook_stub"] = hook_stub


# ── per-case construction / state ──────────────────────────────────────────

SET_KEYS = {"_preview_message_ids", "_segment_preview_message_ids",
            "_superseded_reply_ids"}
LIST_KEYS = {"_tool_progress_lines", "_delivered_commentary_texts",
             "_delivered_segment_texts"}
STATE_KEYS = SET_KEYS | LIST_KEYS | {
    "_accumulated", "_stream_ledger", "_redacted_prefix", "_settled_for",
    "_last_sent_text", "_fallback_prefix", "_boundary_placeholder",
    "_boundary_reason", "_chat_id", "_message_id", "_delivered_final_text",
    "_initial_reply_to_id", "_fallback_final_send",
    "_fallback_preserve_partial_messages", "_preview_uncertain",
    "_tool_progress_active", "_final_response_sent", "_final_content_delivered",
    "_turn_split_delivery", "_delivery_ambiguous", "_delivery_uncertain",
    "_already_sent", "_edit_supported", "_last_edit_overflowed",
    "_before_finalize_notified", "_use_draft_streaming", "_use_native_streaming",
    "_native_stream_opened", "_boundary_reopen", "_awaiting_reopen_after_boundary",
    "_reopen_seeded_eagerly", "_ceiling_hit", "_adapter_requires_finalize",
    "_last_edit_time", "_current_edit_interval", "_flood_strikes",
    "_native_last_pushed_len", "_safe_limit", "_message_created_ts", "_draft_id",
    "_last_segment_delivery",
}


def build_consumer(ns):
    cls = ns["GatewayStreamConsumer"]
    cls._draft_id_counter = int(cin("draft_id_counter", 0))
    cfg = ns["StreamConsumerConfig"](**(cin("cfg") or {}))
    hook_present = bool(cin("hook_present", False))
    c = cls(adapter=types.SimpleNamespace(), chat_id="c1", config=cfg,
            metadata=cin("metadata"),
            initial_reply_to_id=None,
            on_before_finalize=CUR["hook_stub"] if hook_present else None,
            run_still_current=lambda: True)
    c._len_fn = CUR["len_stub"]
    c._safe_limit = 4000
    for k, v in (cin("state") or {}).items():
        if k not in STATE_KEYS:
            fatal(f"unknown state key {k!r}")
        if k in SET_KEYS:
            setattr(c, k, set(v))
        elif k in LIST_KEYS:
            setattr(c, k, list(v))
        elif k == "_last_segment_delivery":
            if v is None:
                setattr(c, k, None)
            else:
                setattr(c, k, (v[0], v[1], v[2], set(v[3])))
        else:
            setattr(c, k, v)
    for d in cin("queue") or []:
        c._queue.put(queue_obj(ns, d))
    CUR["consumer"] = c
    CUR["tick"] = make_tick(ns, cin("tick"))
    CUR["trace"] = []
    CUR["idx"] = {}


def state_json(ns):
    c = CUR["consumer"]
    cls = ns["GatewayStreamConsumer"]
    lsd = c._last_segment_delivery
    return {
        "accumulated": c._accumulated,
        "adapter_requires_finalize": c._adapter_requires_finalize,
        "already_sent": c._already_sent,
        "awaiting_reopen_after_boundary": c._awaiting_reopen_after_boundary,
        "before_finalize_notified": c._before_finalize_notified,
        "boundary_placeholder": c._boundary_placeholder,
        "boundary_reason": c._boundary_reason,
        "boundary_reopen": c._boundary_reopen,
        "ceiling_hit": c._ceiling_hit,
        "current_edit_interval": c._current_edit_interval,
        "delivered_commentary_texts": list(c._delivered_commentary_texts),
        "delivered_final_text": c._delivered_final_text,
        "delivered_segment_texts": list(c._delivered_segment_texts),
        "delivery_ambiguous": c._delivery_ambiguous,
        "delivery_uncertain": c._delivery_uncertain,
        "draft_id": c._draft_id,
        "draft_id_counter": cls._draft_id_counter,
        "edit_supported": c._edit_supported,
        "fallback_final_send": c._fallback_final_send,
        "fallback_prefix": c._fallback_prefix,
        "fallback_preserve_partial_messages": c._fallback_preserve_partial_messages,
        "final_content_delivered": c._final_content_delivered,
        "final_response_sent": c._final_response_sent,
        "flood_strikes": c._flood_strikes,
        "last_edit_overflowed": c._last_edit_overflowed,
        "last_edit_time": c._last_edit_time,
        "last_segment_delivery": [lsd[0], lsd[1], lsd[2], sorted(lsd[3])] if lsd is not None else None,
        "last_sent_text": c._last_sent_text,
        "message_created_ts": c._message_created_ts,
        "message_id": c._message_id,
        "native_last_pushed_len": c._native_last_pushed_len,
        "native_stream_opened": c._native_stream_opened,
        "preview_message_ids": sorted(c._preview_message_ids),
        "preview_uncertain": c._preview_uncertain,
        "redacted_prefix": c._redacted_prefix,
        "reopen_seeded_eagerly": c._reopen_seeded_eagerly,
        "safe_limit": c._safe_limit,
        "segment_preview_message_ids": sorted(c._segment_preview_message_ids),
        "settled_for": c._settled_for,
        "stream_ledger": c._stream_ledger,
        "superseded_reply_ids": sorted(c._superseded_reply_ids),
        "tool_progress_active": c._tool_progress_active,
        "tool_progress_lines": list(c._tool_progress_lines),
        "turn_split_delivery": c._turn_split_delivery,
        "use_draft_streaming": c._use_draft_streaming,
        "use_native_streaming": c._use_native_streaming,
    }


def drive(ns, func):
    c = CUR["consumer"]
    m = getattr(c, FUNCS[func])
    tick = CUR["tick"]
    args = cin("args") or {}
    if func == "drain_queue":
        ret = m()
    elif func in TICK_FUNCS:
        ret = m(tick)
    elif func == "redact_settled_text":
        ret = m(done=bool(args.get("done", False)))
    elif func == "seal_overflow_heads":
        ret = m(final=bool(args.get("final", False)), done=bool(args.get("done", False)))
    elif func == "finalize_edit":
        ret = m(args.get("text", ""), record=bool(args.get("record", True)))
    elif func in ("adopt_final_text", "adopt_recovered_reply"):
        ret = m(args.get("final_raw", ""))
    elif func in ("on_cancelled", "wake_flush_waiters", "mark_skip_redundant_finalize"):
        ret = m()
    else:
        fatal(f"no driver for {func}")
    if func in ASYNC_FUNCS:
        ret = asyncio.run(ret)
    return ret


def compute_out(ns, func):
    try:
        ret = drive(ns, func)
    except Exception as e:
        ret = {"__raise__": type(e).__name__}
    if func == "drain_queue" and not isinstance(ret, dict):
        ret = tick_json(ret)
    out = {"ret": ret, "trace": CUR["trace"], "state": state_json(ns)}
    if func in TICK_FUNCS:
        out["tick"] = tick_json(CUR["tick"])
    if func in ("drain_queue", "wake_flush_waiters"):
        c = CUR["consumer"]
        out["queue_remaining"] = [queue_desc(ns, it) for it in list(c._queue.queue)]
    return out


def first_diff(a, b, path="out"):
    if type(a) is not type(b) and not (isinstance(a, (int, float)) and isinstance(b, (int, float))):
        return f"{path}: type {type(a).__name__} != {type(b).__name__}"
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a:
                return f"{path}.{k}: missing on C side (python={b[k]!r:.120})"
            if k not in b:
                return f"{path}.{k}: missing on python side (c={a[k]!r:.120})"
            d = first_diff(a[k], b[k], f"{path}.{k}")
            if d:
                return d
        return None
    if isinstance(a, list):
        if len(a) != len(b):
            return f"{path}: len {len(a)} != {len(b)}"
        for i, (x, y) in enumerate(zip(a, b)):
            d = first_diff(x, y, f"{path}[{i}]")
            if d:
                return d
        return None
    if a != b:
        return f"{path}: c={a!r:.160} python={b!r:.160}"
    return None


def main():
    ns = extract()
    install_seams(ns)
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            got = json.loads(line)
        except Exception as e:
            print(f"BAD HARNESS JSON: {e}", file=sys.stderr)
            mism += 1
            n += 1
            continue
        func = got.get("func")
        if func not in FUNCS:
            fatal(f"harness emitted unknown func {func!r}")
        CUR["in"] = got.get("in") or {}
        build_consumer(ns)
        want = compute_out(ns, func)
        n += 1
        c_out = got.get("out")
        if c_out == want:
            continue
        mism += 1
        if mism <= 5:
            d = first_diff(c_out, want) if isinstance(c_out, dict) else "out missing/not an object"
            print(f"MISMATCH case {n} func={func}: {d}", file=sys.stderr)
    if n == 0:
        print("ORACLE stream_consumer_flow: FATAL zero cases compared", file=sys.stderr)
        sys.exit(1)
    print(f"ORACLE stream_consumer_flow: {n} cases, {mism} mismatches")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()