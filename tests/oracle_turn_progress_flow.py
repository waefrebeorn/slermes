#!/usr/bin/env python3
"""Oracle for the gateway/run_turn_progress.py progress-reporting FLOW cores
(src/gateway/turn_progress_flow.c).

AST-extracts the live module leaves (_append_progress_repeat_count, _note_flood,
_in_flood_window, _owed_send_failed, _settle_progress_send) and the ENTIRE
GatewayTurnProgressMixin class (future-annotations compile, so no heavy gateway
import), seeds the module Assign constants (_OWED_SEND_ATTEMPTS) and the
dataclass machinery, and replaces every unported I/O collaborator (delivery
ledger flood probes, adapter send/edit/resume, display helpers, i18n-adjacent
status projection, hooks/schedule bridges, logging, clock) with recording stubs
that return the SAME scripted results the C harness feeds its universal seam.

Ported siblings run REAL on both sides (the C port calls its own ports; the
extracted class calls the live methods) — only their I/O leaves are seamed.
Unported pure collaborators (_progress_text, _cap_sized_pieces,
_split_progress_groups, _preview_cap, _progress_terminal_blocks,
_send_progress_text, _edit_progress_message, _track_progress_result,
_status_live) also run real on both sides; the C port carries internal
transcriptions of them that call the identical seams.

State is injected as a JSON mirror (st / ctx / adapters / consumers); the oracle
materializes object doubles so live attribute reads and in-place mutations agree
with the C mirror, then norms the final state back to JSON. Compares ordered
seam trace + return + final state. BaseException is caught per case (scripted
SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError propagate through live
`except Exception` exactly as in the port).

Modeling projections (shared with the C port header):
  - list[0] holder cells (last_tool, repeat_count, long_tool_hint_fired, ...)
    are plain scalar ctx fields.
  - queues are {"items": [...]} objects; presence is an IDENTITY test (a live
    queue.Queue is truthy even when empty), put appends to items; tuples model
    as JSON arrays.
  - _progress_len_fn is the "len_fn" seam; datetime.now().strftime is the
    "now_str" seam (returns the formatted stamp); _schedule is the "schedule"
    seam with the loop already resolved; render_notification always invokes its
    present() callback after recording (a scripted raise fires BEFORE present).
  - t(key, **kw) returns key (i18n identity, unrecorded) — the decision chain
    is observable through WHICH key is queued/returned.
  - clock scripts are floats; a null clock result raises the live TypeError.

Input: stdin JSON lines from t_port_turn_progress_flow.
Summary: ORACLE turn_progress_flow: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import asyncio
import builtins
import contextlib
import dataclasses
import json
import logging
import os
import queue
import re
import sys
import threading
import time
import types
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_turn_progress.py")

WANTED_MOD = {"_append_progress_repeat_count", "_note_flood", "_in_flood_window",
              "_owed_send_failed", "_settle_progress_send"}

CUR = {"func": None, "in": {}, "trace": [], "reg": {}, "io": {}, "pos": {}}


def rec(ev):
    CUR["trace"].append(ev)


class RecLogger:
    def _rec(self, level, fmt, args):
        rec({"ev": "log", "level": level, "fmt": fmt,
             "args": [str(a) if isinstance(a, BaseException) else a for a in args]})

    def debug(self, fmt, *a, **k):
        self._rec(10, fmt, a)

    def info(self, fmt, *a, **k):
        self._rec(20, fmt, a)

    def warning(self, fmt, *a, **k):
        self._rec(30, fmt, a)

    def error(self, fmt, *a, **k):
        self._rec(40, fmt, a)


# ── script access (mirrors the harness stub semantics) ─────────────────────
def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    cls = getattr(builtins, kind, None)
    if cls is None and kind == "CancelledError":
        cls = asyncio.CancelledError
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


def next_script(name, optional=False):
    io = CUR["io"]
    if name not in io:
        if optional:
            return None
        raise RuntimeError("no script for " + name)
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            raise RuntimeError("script exhausted for " + name)
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam_rec(name, args, optional=False):
    r = next_script(name, optional)
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def mat(x):
    if isinstance(x, dict):
        return SimpleNamespace(**{k: mat(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mat(v) for v in x]
    if isinstance(x, tuple):
        return tuple(mat(v) for v in x)
    return x


class ObjDouble:
    def __init__(self, fields, methods=None):
        self.__dict__["_fields"] = dict(fields)
        self.__dict__["_methods"] = dict(methods or {})

    def __getattr__(self, k):
        d = self.__dict__
        if k in d["_methods"]:
            return d["_methods"][k]
        f = d["_fields"]
        if k in f:
            return f[k]
        raise AttributeError("'%s' object has no attribute %r" % (type(self).__name__, k))

    def __setattr__(self, k, v):
        self.__dict__["_fields"][k] = v


class QueueDouble:
    """queue.Queue stand-in: ALWAYS truthy (identity presence), put appends."""
    def __init__(self, items):
        self.items = items

    def put(self, item):
        self.items.append(item)

    def empty(self):
        return not self.items

    def get_nowait(self):
        if not self.items:
            raise queue.Empty
        return self.items.pop(0)


class ResultDouble(ObjDouble):
    pass


class AdapterDouble(ObjDouble):
    def __getattr__(self, k):
        if k == "_resume_partial_send":
            # getattr(adapter, "_resume_partial_send", None): the fixture flag
            # models presence of the bound method, never a bool value.
            return self.__dict__["_methods"].get(k)
        return super().__getattr__(k)


class ConsumerDouble(ObjDouble):
    pass


class FutDouble:
    def __init__(self, fid):
        self.id = fid

    def add_done_callback(self, cb):
        return seam_rec("add_done_callback",
                        {"fut": self.id, "callback": "_track_future_cleanup_id"})


# list[0] holder cells in the live ctx; the mirror stores plain scalars.
CTX_CELLS = ("last_progress_msg", "repeat_count", "last_tool",
             "long_tool_hint_fired", "last_was_terminal_block")


def result_fields(d):
    # raw_response is read with dict .get() live — keep it a raw dict.
    return {k: (v if k == "raw_response" else mat(v)) for k, v in d.items()}


def norm_fields(x):
    return {k: norm(v) for k, v in x.__dict__["_fields"].items()}


def norm(x):
    if isinstance(x, QueueDouble):
        return {"items": [norm(v) for v in x.items]}
    if isinstance(x, FutDouble):
        return x.id
    if isinstance(x, (AdapterDouble, ConsumerDouble)):
        return x.id   # object references project to id strings in the mirror
    if isinstance(x, ObjDouble):
        return norm_fields(x)
    if isinstance(x, SimpleNamespace):
        return {k: norm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


def norm_ctx(ctx):
    out = {}
    for k, v in ctx.__dict__["_fields"].items():
        if k in CTX_CELLS and isinstance(v, list):
            out[k] = norm(v[0])
        else:
            out[k] = norm(v)
    return out


def norm_state(reg):
    return {
        "st": norm(reg["st"]) if reg.get("st") is not None else None,
        "ctx": norm_ctx(reg["ctx"]) if reg.get("ctx") is not None else None,
        "adapters": {aid: norm_fields(ad) for aid, ad in reg.get("adapters", {}).items()},
        "consumers": {cid: norm_fields(c) for cid, c in reg.get("consumers", {}).items()},
    }


# ── extraction ──────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("turn_progress_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({
        "logger": RecLogger(), "t": lambda key, **kw: key,
        "asyncio": asyncio, "dataclasses": dataclasses, "logging": logging,
        "queue": queue, "re": re, "threading": threading, "time": time,
        "suppress": contextlib.suppress, "contextlib": contextlib,
        "Any": object, "Dict": dict, "List": list, "Optional": object,
        "TYPE_CHECKING": False,
        "_accepts_keyword": lambda fn, kw: False,
        "strip_stale_dangerous_confirmations": lambda x: x,
        "Platform": SimpleNamespace(LOCAL="local"),
        "repair_explicit_computer_use_media_paths": lambda x: x,
        "BasePlatformAdapter": None, "typing_metadata_for_turn": lambda *a, **k: None,
        "TurnContext": None, "cfg_get": lambda *a, **k: None,
        "is_truthy_value": lambda v, default=False: bool(v),
    })
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_MOD:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayTurnProgressMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=flags), ns)
    for name in WANTED_MOD | {"GatewayTurnProgressMixin", "_OWED_SEND_ATTEMPTS"}:
        assert name in ns, "extraction missed %s" % name
    assert ns["_OWED_SEND_ATTEMPTS"] == 5
    return ns


def install_globals(ns):
    ns["logger"] = RecLogger()
    ns["time"] = SimpleNamespace(monotonic=lambda: (
        lambda r: float(r) if isinstance(r, (int, float)) and not isinstance(r, bool) else r
    )(seam_rec("monotonic", {})))

    class DTDouble:
        def strftime(self, fmt):
            return seam_rec("now_str", {"fmt": fmt})
    ns["datetime"] = SimpleNamespace(now=lambda: DTDouble())

    class BasePlatformAdapter:
        @staticmethod
        def truncate_message(text, limit, len_fn=None):
            r = seam_rec("truncate_message", {"text": text, "limit": limit})
            return list(r) if isinstance(r, list) else r

        @staticmethod
        def _is_partial_delivery(result):
            return seam_rec("is_partial_delivery", {"result": norm(result)})
    ns["BasePlatformAdapter"] = BasePlatformAdapter

    ns["local_viewer_turn"] = lambda runner: seam_rec("local_viewer_turn", {})

    # In-function import seams (sys.modules doubles; parent packages registered).
    gateway_pkg = types.ModuleType("gateway")
    agent_pkg = types.ModuleType("agent")
    sys.modules["gateway"] = gateway_pkg
    sys.modules["agent"] = agent_pkg

    def reg_mod(name, parent, **fns):
        m = types.ModuleType(name)
        for k, v in fns.items():
            setattr(m, k, v)
        sys.modules[name] = m
        setattr(parent, name.split(".", 1)[1], m)
        return m

    reg_mod("gateway.delivery_ledger", gateway_pkg,
            is_flood_error=lambda error: seam_rec("is_flood_error", {"error": error}),
            flood_wait_seconds=lambda error, default=30.0: seam_rec(
                "flood_wait_seconds", {"error": error, "default": default}))
    reg_mod("agent.display", agent_pkg,
            get_tool_emoji=lambda tool_name, default="": seam_rec(
                "get_tool_emoji", {"tool_name": tool_name, "default": default}),
            get_tool_preview_max_len=lambda: seam_rec("preview_max_len", {}),
            verbose_tool_args_text=lambda args, max_len, code_blocks: seam_rec(
                "verbose_tool_args_text",
                {"args": args, "max_len": max_len, "supports_code_blocks": code_blocks}),
            prepare_tool_preview=lambda tool_name, args, fallback, max_len: (
                lambda r: ResultDouble({k: mat(v) for k, v in r.items()})
                if isinstance(r, dict) else r)(seam_rec(
                    "prepare_tool_preview",
                    {"tool_name": tool_name, "args": args, "fallback": fallback,
                     "max_len": max_len})),
            get_tool_verb=lambda tool_name: seam_rec("get_tool_verb", {"tool_name": tool_name}),
            verb_drops_preview=lambda tool_name: seam_rec(
                "verb_drops_preview", {"tool_name": tool_name}),
            tool_verb_connector=lambda tool_name: seam_rec(
                "tool_verb_connector", {"tool_name": tool_name}))
    reg_mod("gateway.local_viewer_events", gateway_pkg,
            publish_progress=lambda runner, event_type, tool_name, preview, kwargs: seam_rec(
                "publish_progress",
                {"event_type": event_type, "tool_name": tool_name, "preview": preview,
                 "kwargs": kwargs}))
    reg_mod("gateway.run", gateway_pkg,
            _prepare_gateway_status_message=lambda platform, event_type, message, chat_type:
                seam_rec("prepare_status_message",
                         {"platform": norm(platform), "event_type": event_type,
                          "message": message, "chat_type": chat_type}),
            _redact_gateway_user_facing_secrets=lambda text: seam_rec(
                "redact_secrets", {"text": text}),
            _send_or_update_status_coro=lambda adapter, chat_id, status_key, prepared, metadata:
                seam_rec("send_or_update_status_coro",
                         drop_none({"adapter_id": adapter.id if adapter is not None else None,
                                    "chat_id": chat_id, "status_key": status_key,
                                    "prepared": prepared, "thread_metadata": norm(metadata)})))
    reg_mod("gateway.run_status_filter", gateway_pkg,
            status_chat_type=lambda source: seam_rec("status_chat_type", {"source": norm(source)}))
    reg_mod("gateway.warning_notifications", gateway_pkg,
            is_warning_status=lambda event_type, message: seam_rec(
                "is_warning_status", {"event_type": event_type, "message": message}),
            render_notification=lambda present, platform=None, user_config=None, diagnostic=False:
                _render_notification(present, platform, user_config, diagnostic))
    reg_mod("gateway.bot_peer_notices", gateway_pkg,
            automatic_notices_allowed=lambda source: seam_rec(
                "automatic_notices_allowed", {"source": norm(source)}))
    reg_mod("agent.conversation_compression", agent_pkg,
            compaction_status_kind=lambda event_type, message: seam_rec(
                "compaction_status_kind", {"event_type": event_type, "message": message}))


def _render_notification(present, platform, user_config, diagnostic):
    r = seam_rec("render_notification",
                 {"platform": norm(platform), "user_config": norm(user_config),
                  "diagnostic": diagnostic})
    present()
    return r


# ── per-case materialization + runner wiring ───────────────────────────────
def make_result(r):
    if r is None:
        return None
    if isinstance(r, dict):
        return ResultDouble(result_fields(r))
    return r


def materialize(inp):
    stt = inp.get("state") or {}
    reg = {"st": None, "ctx": None, "adapters": {}, "consumers": {}}
    for aid, f in (stt.get("adapters") or {}).items():
        ad = AdapterDouble({k: mat(v) for k, v in f.items()})
        ad.__dict__["id"] = aid

        async def send(chat_id=None, content=None, reply_to=None, metadata=None, ad=ad):
            r = seam_rec("adapter_send", drop_none(
                {"chat_id": chat_id, "content": content, "reply_to": reply_to,
                 "metadata": norm(metadata)}))
            return make_result(r)

        async def edit(chat_id=None, message_id=None, content=None, finalize=None, metadata=None, ad=ad):
            args = {"chat_id": chat_id, "message_id": message_id, "content": content}
            if finalize is not None:
                args["finalize"] = finalize
            if metadata is not None:
                args["metadata"] = norm(metadata)
            return make_result(seam_rec("adapter_edit", args))
        m = ad.__dict__["_methods"]
        m["send"] = send
        m["edit_message"] = edit

        def format_tool_preview(prepared, ad=ad):
            return seam_rec("format_tool_preview",
                            {"adapter_id": ad.id, "prepared": norm(prepared)})
        m["format_tool_preview"] = format_tool_preview
        if f.get("_resume_partial_send"):
            async def resume(chat_id, owed_tail, reply_to=None, metadata=None, ad=ad):
                r = seam_rec("resume_partial_send", drop_none(
                    {"chat_id": chat_id, "owed_tail": norm(owed_tail),
                     "reply_to": reply_to, "metadata": norm(metadata)}))
                return make_result(r)
            m["_resume_partial_send"] = resume
        reg["adapters"][aid] = ad
    for cid, f in (stt.get("consumers") or {}).items():
        c = ConsumerDouble({k: mat(v) for k, v in f.items()})
        c.__dict__["id"] = cid

        def on_tool_progress(text, c=c):
            return seam_rec("on_tool_progress", {"consumer_id": c.id, "text": text})
        c.__dict__["_methods"]["on_tool_progress"] = on_tool_progress
        reg["consumers"][cid] = c

    sf = stt.get("st")
    if sf is not None:
        fields = {k: mat(v) for k, v in sf.items()}
        if isinstance(fields.get("owed_tail"), SimpleNamespace):
            fields["owed_tail"] = ResultDouble(result_fields(vars(fields["owed_tail"])))
        if isinstance(fields.get("owed_bubbles"), list):
            fields["owed_bubbles"] = [tuple(b) for b in fields["owed_bubbles"]]
        st = ObjDouble(fields)
        st.__dict__["_methods"]["_progress_len_fn"] = lambda text: seam_rec("len_fn", {"text": text})
        if isinstance(fields.get("adapter"), str):
            st.__dict__["_fields"]["adapter"] = reg["adapters"].get(fields["adapter"])
        reg["st"] = st

    cf = stt.get("ctx")
    if cf is not None:
        fields = {k: mat(v) for k, v in cf.items()}
        for qk in ("progress_queue", "log_queue"):
            q = fields.get(qk)
            if isinstance(q, SimpleNamespace) and hasattr(q, "items"):
                fields[qk] = QueueDouble(list(q.items))
            elif isinstance(q, dict) and "items" in q:
                fields[qk] = QueueDouble(list(q["items"]))
        if isinstance(fields.get("_status_adapter"), str):
            fields["_status_adapter"] = reg["adapters"].get(fields["_status_adapter"])
        for ck in CTX_CELLS:
            if ck in fields:
                fields[ck] = [fields[ck]]   # live holder cells are 1-element lists
        ctx = ObjDouble(fields)
        ctx.__dict__["_methods"]["_run_still_current"] = lambda: seam_rec("run_still_current", {})
        reg["ctx"] = ctx
    return reg


def make_runner(ns, reg, inp):
    runner = ns["GatewayTurnProgressMixin"]()
    runner._ctx = reg["ctx"]
    runner._runner = SimpleNamespace(
        _delivery_adapter_for=lambda source: (
            lambda r: reg["adapters"].get(r) if r else None)(
                seam_rec("delivery_adapter_for", {"source": norm(source)})))
    runner._agent_interrupted = lambda: seam_rec("agent_interrupted", {})

    def stream_consumer():
        cid = reg["ctx"].__dict__["_fields"].get("stream_consumer_id") if reg["ctx"] else None
        return reg["consumers"].get(cid) if cid else None
    runner._stream_consumer = stream_consumer
    runner._progress_subagent_notice = lambda preview, kwargs: seam_rec(
        "subagent_notice", {"preview": preview, "kwargs": kwargs})
    runner._progress_live_status = lambda event_type, tool_name, args: seam_rec(
        "live_status", {"event_type": event_type, "tool_name": tool_name, "args": args})
    runner._progress_onboarding_hint = lambda kwargs: seam_rec("onboarding_hint", {"kwargs": kwargs})

    def schedule(coro, log_message, loop=None):
        ctx = reg["ctx"]
        resolved = ctx._loop_for_step if loop is None else loop
        r = seam_rec("schedule", drop_none(
            {"coro": coro, "log_message": log_message, "loop": resolved}))
        return FutDouble(r) if r is not None else None
    runner._schedule = schedule

    def publish_execution(event_type, payload):
        return seam_rec("publish_execution", {"event_type": event_type, "payload": norm(payload)})
    runner._publish_execution = publish_execution
    return runner


def drive(ns, func, inp):
    reg = materialize(inp)
    CUR["reg"] = reg
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    runner = make_runner(ns, reg, inp)
    st = reg["st"]
    ctx = reg["ctx"]

    def result():
        return make_result(inp.get("result"))

    if func == "_append_progress_repeat_count":
        return ns["_append_progress_repeat_count"](inp.get("message", ""), inp.get("occurrences", 0))
    if func == "_note_flood":
        return ns["_note_flood"](st, result())
    if func == "_in_flood_window":
        return ns["_in_flood_window"](st)
    if func == "_owed_send_failed":
        return ns["_owed_send_failed"](st)
    if func == "_settle_progress_send":
        ns["_settle_progress_send"](st, result())
        return None
    if func == "_hold_partial_tail":
        return runner._hold_partial_tail(st, result())
    if func == "_settle_partial_progress":
        return runner._settle_partial_progress(st, result(), inp.get("rest") or [])
    if func == "_owed_tail_blocks":
        return asyncio.run(runner._owed_tail_blocks(st))
    if func == "_progress_emit":
        runner._progress_emit(inp.get("msg", ""))
        return None
    if func == "_progress_absorb":
        raw = inp.get("raw")
        if isinstance(raw, list):
            raw = tuple(raw)
        return runner._progress_absorb(st, raw)
    if func == "_is_reset_marker":
        raw = inp.get("raw")
        if isinstance(raw, list):
            raw = tuple(raw)
        return runner._is_reset_marker(raw)
    if func == "_reset_progress_bubble":
        runner._reset_progress_bubble(st)
        return None
    if func == "_progress_send_or_edit":
        return asyncio.run(runner._progress_send_or_edit(st, inp.get("msg", "")))
    if func == "_roll_progress_overflow_if_needed":
        return asyncio.run(runner._roll_progress_overflow_if_needed(st))
    if func == "_deliver_flood_pending_progress":
        return asyncio.run(runner._deliver_flood_pending_progress(st))
    if func == "progress_callback":
        runner.progress_callback(inp.get("event_type", ""), inp.get("tool_name"),
                                 inp.get("preview"), inp.get("args"),
                                 **(inp.get("kwargs") or {}))
        return None
    if func == "_progress_build_message":
        return runner._progress_build_message(inp.get("tool_name"), inp.get("preview"),
                                              inp.get("args"))
    if func == "_status_callback_sync":
        runner._status_callback_sync(inp.get("event_type", ""), inp.get("message", ""))
        return None
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    ns = extract()
    install_globals(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["reg"] = {}
        try:
            ret = drive(ns, func, inp)
            want_ret = norm(ret)
        except BaseException as e:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
        want = {"ret": want_ret, "trace": CUR["trace"], "state": norm_state(CUR["reg"])}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE turn_progress_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
