#!/usr/bin/env python3
"""Differential oracle for gateway/stream_consumer_fallback.py (gate w19_streamcons_39b4a2)."""
import ast
import json
import re
import sys
import types
import __future__
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "gateway" / "stream_consumer_fallback.py"
FENCES = Q / "gateway" / "stream_consumer_fences.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_streamcons_39b4a2"


class BaseAdapter:
    MAX_MESSAGE_LENGTH = 4096

    def delete_message(self, *a, **k):
        return True


def extract_fences():
    tree = ast.parse(FENCES.read_text(encoding="utf-8"))
    mod = types.ModuleType("fences_live")
    sys.modules["fences_live"] = mod
    g = mod.__dict__
    g.update(dict(re=re))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in (
                "open_code_fence_span", "ensure_closed_code_fences",
                "escape_code_fences_for_display"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(FENCES), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


class _Logger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("streamfallback_live")
    sys.modules["streamfallback_live"] = mod
    g = mod.__dict__
    fg = extract_fences()
    g.update(dict(ensure_closed_code_fences=fg["ensure_closed_code_fences"],
                  open_code_fence_span=fg["open_code_fence_span"],
                  escape_code_fences_for_display=fg["escape_code_fences_for_display"],
                  _BasePlatformAdapter=BaseAdapter, reply_file_off=lambda v: None,
                  asyncio=types.SimpleNamespace(sleep=None), contextlib=types.SimpleNamespace(
                      suppress=lambda *a, **k: None), logger=_Logger(),
                  Any=object, Callable=object, Optional=object))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_edit_skipped":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "StreamFallbackMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
MIXIN = G["StreamFallbackMixin"]


def new_obj(**attrs):
    obj = MIXIN()
    obj._fallback_prefix = ""
    obj._last_sent_text = ""
    obj._message_id = None
    obj._fragment_cut = None
    obj._fallback_preserve_partial_messages = False
    obj.cfg = SimpleNamespace(cursor=None)
    obj.chat_id = "chat-1"
    obj.adapter = SimpleNamespace()
    obj._clean_for_display = lambda text: text
    obj._track_preview_ids_from_result = lambda result: None
    obj._notify_new_message = lambda: None
    obj._entered = []
    obj._enter_fallback_mode = lambda prefix: obj._entered.append(prefix)
    for k, v in attrs.items():
        setattr(obj, k, v)
    return obj


def adapter_for(can_delete):
    if can_delete:
        class Deleting(SimpleNamespace):
            def delete_message(self, *a, **k):
                return True
            MAX_MESSAGE_LENGTH = 4096
        return Deleting()
    return SimpleNamespace(MAX_MESSAGE_LENGTH=4096)


def result_obj(raw, message_id=None):
    return SimpleNamespace(raw_response=raw, message_id=message_id, success=False)


def run_case(r):
    f = r.get("func")
    if f == "edit_skipped":
        return G["_edit_skipped"](result_obj(r.get("raw_response")))
    if f == "visible_prefix":
        o = new_obj(_last_sent_text=r.get("last_sent_text") or "",
                    cfg=SimpleNamespace(cursor=r.get("cursor")))
        return o._visible_prefix()
    if f == "continuation_text":
        o = new_obj(_last_sent_text=r.get("last_sent_text") or "",
                    _fallback_prefix=r.get("fallback_prefix") or "",
                    cfg=SimpleNamespace(cursor=r.get("cursor")))
        o._fragment_can_be_replaced = lambda: bool(r.get("fragment_can_be_replaced"))
        text = o._continuation_text(r.get("final_text") or "")
        return {"text": text, "fragment_cut": o._fragment_cut}
    if f == "prose_resume_point":
        return MIXIN._prose_resume_point(r.get("final_text") or "", int(r.get("cut") or 0))
    if f == "fragment_can_be_replaced":
        o = new_obj(_message_id=r.get("message_id"),
                    _fallback_preserve_partial_messages=bool(r.get("preserve_partial")),
                    adapter=adapter_for(bool(r.get("adapter_can_delete"))))
        return o._fragment_can_be_replaced()
    if f == "send_failure_may_have_delivered":
        cls = type(str(r.get("exc_name") or "SendResult"), (), {})
        obj = cls()
        obj.retryable = r.get("retryable")
        obj.raw_response = {"delivery_uncertain": True} if r.get("delivery_uncertain") else None
        obj.error = r.get("error")
        return MIXIN._send_failure_may_have_delivered(obj)
    if f == "is_flood_error":
        o = new_obj()
        return o._is_flood_error(SimpleNamespace(error=r.get("error")))
    if f == "flood_retry_delay":
        o = new_obj()
        o._max_fallback_flood_retry_seconds = float(r.get("max_wait", 30.0))
        res = SimpleNamespace(error=r.get("error"), retry_after=r.get("retry_after"))
        delay = o._fallback_flood_retry_delay(res)
        return delay
    if f == "adopt_partial_head":
        o = new_obj()
        adopted = o._adopt_partial_head(result_obj(r.get("raw_response"), r.get("result_message_id")),
                                        r.get("text") or "")
        return {"adopted": adopted, "message_id": o._message_id,
                "last_sent_text": o._last_sent_text,
                "preserve_partial": o._fallback_preserve_partial_messages,
                "prefix": o._entered[-1] if o._entered else ""}
    if f == "raw_message_limit":
        def chat_raises(*a, **k):
            raise RuntimeError("boom")

        class MaxAdapter(BaseAdapter):
            MAX_MESSAGE_LENGTH = r.get("max_message_length", 4096)

            def max_message_length_for_chat(self, chat_id):
                if r.get("chat_limit_raises"):
                    raise RuntimeError("boom")
                if "chat_limit" in r:
                    return r["chat_limit"]
                raise RuntimeError("no limit")

            def streaming_overflow_limit(self):
                if r.get("cap_kind") == "none" or "cap_kind" not in r:
                    return None
                if r.get("cap_kind") == "float":
                    return float(r.get("cap"))
                return r.get("cap")
        if r.get("is_base"):
            o = new_obj(adapter=MaxAdapter())
        else:
            o = new_obj(adapter=SimpleNamespace(MAX_MESSAGE_LENGTH=r.get("max_message_length", 4096)))
        return o._raw_message_limit()
    if f == "open_code_fence_span":
        span = G["open_code_fence_span"](r.get("text") or "", int(r.get("upto") or 0))
        return None if span is None else {"span": list(span)}
    if f == "ensure_closed_code_fences":
        return G["ensure_closed_code_fences"](r.get("text") or "")
    return None


FUNCS = {"edit_skipped", "visible_prefix", "continuation_text", "prose_resume_point",
         "fragment_can_be_replaced", "send_failure_may_have_delivered", "is_flood_error",
         "flood_retry_delay", "adopt_partial_head", "raw_message_limit",
         "open_code_fence_span", "ensure_closed_code_fences"}


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
