#!/usr/bin/env python3
"""Oracle for chat_completion_helpers_flow.c -- drives the LIVE
agent/chat_completion_helpers.py streaming/tool-call FLOW with SCRIPTED I/O and
compares the DECISION SEQUENCE against the C port.

The harness (tests/t_port_cch_flow.c) emits {"func","in","out"} lines; this
oracle reads them, replays the same scripted inputs through the live Python
(monkeypatched recording stubs at the I/O seams), and diffs the sequences.

The quarry import chain needs the app venv; when the current interpreter lacks
it, re-exec under the quarry venv (stdin survives execv).
"""
import os
import sys
import json
import logging
import contextlib
from types import SimpleNamespace as NS
from unittest.mock import patch

logging.disable(logging.CRITICAL)

for _quarry in ("~/hermes-agent", "~/hermes-agent-dev"):
    _p = os.path.expanduser(_quarry)
    if os.path.isdir(os.path.join(_p, "agent")):
        sys.path.insert(0, _p)
        break

try:
    import agent.chat_completion_helpers as M
    from agent.chat_completion_helpers_tool_calls import _ToolCallAccumulator
    import httpx
except ImportError:
    _venv = os.path.expanduser("~/hermes-agent/.venv/Scripts/python.exe")
    if not os.path.isfile(_venv):
        _venv = os.path.expanduser("~/hermes-agent/.venv/bin/python")
    if os.path.isfile(_venv) and os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise

sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def jnorm(x):
    """JSON-normalize live objects (SimpleNamespace/dict/int-keys/tuple)."""
    if isinstance(x, NS):
        return {k: jnorm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {str(k): jnorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [jnorm(v) for v in x]
    return x


def make_agent(**kw):
    a = NS(model="m", provider="p", base_url="", api_mode="chat_completions",
           _interrupt_requested=False, _disable_streaming=False,
           _current_streamed_assistant_text="", _consecutive_stale_streams=0)
    for k, v in kw.items():
        setattr(a, k, v)
    return a


def make_instance(agent=None, api_kwargs=None):
    return M._StreamingCall(agent or make_agent(), api_kwargs or {}, None)


def mk_tc(d):
    fn = d.get("function")
    fns = NS(name=fn.get("name"), arguments=fn.get("arguments")) if fn else None
    if fn and "thought_signature" in fn:
        fns.thought_signature = fn["thought_signature"]
    tc = NS(index=d.get("index"), id=d.get("id"), function=fns,
            extra_content=d.get("extra_content"))
    if "model_extra" in d:
        tc.model_extra = d["model_extra"]
    return tc


def mk_delta(d):
    delta = NS(content=d.get("content"), reasoning_content=d.get("reasoning_content"),
               reasoning=d.get("reasoning"), refusal=d.get("refusal"),
               reasoning_details=d.get("reasoning_details"),
               tool_calls=[mk_tc(t) for t in d["tool_calls"]] if d.get("tool_calls") else None)
    if "model_extra" in d:
        delta.model_extra = d["model_extra"]
    return delta


def mk_chunk(d):
    choices = []
    for c in d.get("choices") or []:
        choices.append(NS(delta=mk_delta(c.get("delta") or {}),
                          finish_reason=c.get("finish_reason"), index=c.get("index", 0)))
    chunk = NS(choices=choices or None, usage=d.get("usage"),
               id=d.get("id"), model=d.get("model"), provider=d.get("provider"),
               error_type=d.get("error_type"), error_message=d.get("error_message"),
               lastOne=d.get("lastOne"))
    if "model_extra" in d:
        chunk.model_extra = d["model_extra"]
    return chunk


def make_error(inp):
    if inp.get("is_empty_stream"):
        return M.EmptyStreamError("empty")
    if inp.get("is_timeout"):
        return httpx.ReadTimeout("timeout")
    if inp.get("is_conn_err"):
        return httpx.ConnectError("conn")
    return Exception((inp.get("error") or {}).get("kind", "error"))


# ---------------------------------------------------------------------------
# drivers
# ---------------------------------------------------------------------------

def d_tc_acc(inp):
    acc = _ToolCallAccumulator()
    seq = []
    for d in inp.get("deltas", []):
        name = acc.feed(mk_tc(d))
        seq.append({"a": "feed", "ret": name})
    if inp.get("materialize"):
        acc.materialize()
        seq.append({"a": "materialize", "acc": jnorm(acc.acc)})
    return seq


def d_choiceless(inp):
    chunk = mk_chunk(inp.get("chunk") or {})
    # keep usage/model_extra as plain dicts (live passes them through)
    try:
        usage, finish = M._StreamingCall._choiceless_chunk(chunk, inp.get("finish_reason"))
        return [{"a": "choiceless", "usage": jnorm(usage) if usage else None,
                 "finish_reason": finish}]
    except M.ProviderStreamError as e:
        return [{"a": "raise", "kind": "ProviderStreamError", "status_code": e.status_code,
                 "body": jnorm(e.body), "raw_text": e.raw_text, "message": str(e)}]


def _script_repair_rep(stack, inp):
    rq = list(inp.get("repair_results", []))
    pq = list(inp.get("repetition", []))
    stack.enter_context(patch.object(M, "_repair_tool_call_arguments",
                                     lambda args, name="?": rq.pop(0) if rq else "{}"))
    stack.enter_context(patch.object(M, "is_repetition_dominated",
                                     lambda text: pq.pop(0) if pq else False))


def d_assemble(inp):
    acc = {int(k): v for k, v in (inp.get("acc") or {}).items()}
    with contextlib.ExitStack() as stack:
        _script_repair_rep(stack, inp)
        mock, trunc = M._StreamingCall._assemble_tool_calls(acc, inp.get("finish_reason"))
    return [{"a": "assemble", "tool_calls": [jnorm(m) for m in mock] if mock else None,
             "has_truncated": bool(trunc)}]


def d_finish(inp):
    inst = make_instance()
    acc = {int(k): v for k, v in (inp.get("acc") or {}).items()}
    flushed = [False]

    def flush_pending():
        flushed[0] = True

    with contextlib.ExitStack() as stack:
        _script_repair_rep(stack, inp)
        import agent.agent_runtime_helpers as ARH
        stack.enter_context(patch.object(ARH, "extract_reasoning",
                                         lambda agent, msg: inp.get("extract_reasoning")))
        stack.enter_context(patch.object(M, "uuid",
                                         NS(uuid4=lambda: inp.get("uuid", "00000000-0000-0000-0000-000000000000"))))
        try:
            resp = inst._finish_chat_stream(
                NS(response=None), inp.get("role", "assistant"),
                inp.get("content_parts", []), inp.get("reasoning_parts", []),
                acc, inp.get("finish_reason"), inp.get("model"), inp.get("usage"),
                flush_pending=flush_pending, response_id=inp.get("response_id"),
                upstream_provider=inp.get("upstream_provider"),
                reasoning_details=inp.get("reasoning_details"),
                refusal_parts=inp.get("refusal_parts"))
        except M.EmptyStreamError as e:
            return [{"a": "raise", "kind": "EmptyStreamError", "message": str(e)}]
        except M.ProviderStreamError as e:
            return [{"a": "raise", "kind": "ProviderStreamError", "status_code": e.status_code,
                     "body": jnorm(e.body), "message": str(e)}]
    if getattr(resp, "id", None) == M.PARTIAL_STREAM_STUB_ID:
        return [{"a": "stub", "shape": jnorm(resp)}]
    return [{"a": "response", "response": jnorm(resp), "flushed": flushed[0]}]


def d_pse_text(inp):
    r = M._provider_stream_error_from_text(inp.get("text", ""), inp.get("finish_reason"))
    if r is None:
        return []
    return [{"a": "error", "status_code": r.status_code, "body": jnorm(r.body),
             "raw_text": r.raw_text, "message": str(r)}]


def d_build_stub(inp):
    stub = M._build_partial_stream_stub(
        inp.get("role"), inp.get("full_content"), inp.get("full_reasoning"),
        inp.get("model_name"), inp.get("usage"),
        dropped_tool_names=inp.get("dropped_tool_names"),
        overflow_terminal=inp.get("overflow_terminal", False),
        api_mode=inp.get("api_mode"), clean_eof=inp.get("clean_eof", False))
    return [{"a": "stub", "shape": jnorm(stub)}]


def d_partial_stub(inp):
    agent = make_agent(model=inp.get("model", "unknown"), api_mode=inp.get("api_mode"),
                       _current_streamed_assistant_text=inp.get("streamed_text", ""))
    agent._warning_presentation_enabled = lambda: inp.get("warning_presentation", False)
    agent._fire_stream_delta = lambda *a, **k: None
    inst = make_instance(agent)
    inst.result["partial_tool_names"] = list(inp.get("partial_tool_names", []))
    import agent.error_classifier as EC
    reason = (inp.get("classify") or {}).get("reason")
    # Live compares _cls.reason against FailoverReason ENUM members (a plain
    # enum.Enum, so a raw string never equals one); the stub must carry the
    # real enum member, not the scripted string.
    cls = NS(reason=EC.FailoverReason(reason)) if reason else None
    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(EC, "classify_api_error",
                                         lambda error, provider, model: cls))
        stub = inst._partial_stream_stub(Exception("drop"))
    return [{"a": "stub", "shape": jnorm(stub)}]


def d_maybe_disable(inp):
    agent = make_agent(api_mode=inp.get("api_mode", ""), provider=inp.get("provider", ""))
    notice = {}
    agent._emit_warning = lambda text: notice.update(kind="warning", text=text)
    agent._safe_print = lambda text, **k: notice.update(kind="print", text=text)
    inst = make_instance(agent)
    err = Exception((inp.get("error") or {}).get("text", ""))
    import agent.anthropic_adapter as AA
    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(M, "_streams_llamacpp", lambda a: inp.get("llamacpp", False)))
        stack.enter_context(patch.object(M, "_is_provider_stream_empty_frame_error",
                                         lambda e: inp.get("empty_frame", False)))
        stack.enter_context(patch.object(AA, "_is_stream_unavailable_error",
                                         lambda e: inp.get("stream_unavailable", False)))
        inst._maybe_disable_streaming(err)
    return [{"a": "result", "disabled": bool(agent._disable_streaming),
             "notice_kind": notice.get("kind"), "notice_text": notice.get("text")}]


def d_retry_after_drop(inp):
    agent = make_agent()
    seq = []
    agent._emit_stream_drop = lambda **k: seq.append(
        {"a": "emit_stream_drop", "attempt": k.get("attempt"),
         "max_attempts": k.get("max_attempts"), "mid_tool_call": k.get("mid_tool_call")})
    agent._is_provider_stream_parse_error = lambda e: inp.get("is_parse_error", False)
    inst = make_instance(agent)
    inst._cancel_current_stream_attempt = lambda reason: seq.append({"a": "cancel_attempt", "reason": reason})
    inst.clients.close_once = lambda reason: seq.append({"a": "close_once", "reason": reason})
    import agent.anthropic_adapter as AA
    import agent.retry_utils as RU
    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(AA, "buffer_anthropic_tool_input",
                                         lambda *a, **k: seq.append({"a": "buffer_anthropic_tool_input"})))
        stack.enter_context(patch.object(RU, "jittered_backoff",
                                         lambda *a, **k: inp.get("backoff_delay", 0.0)))
        stack.enter_context(patch.object(M, "_wait_stream_retry_backoff",
                                         lambda a, delay: seq.append({"a": "wait_backoff", "delay": delay})))
        inst._retry_after_drop(Exception("x"), inp.get("attempt", 0), inp.get("max_retries", 0),
                               mid_tool_call=inp.get("mid_tool_call", False), reason=inp.get("reason", ""))
    return seq


def d_handle(inp):
    agent = make_agent(_current_streamed_assistant_text=inp.get("streamed_text", ""),
                       _stream_options_unsupported=inp.get("stream_options_unsupported", False))
    agent._is_provider_stream_parse_error = lambda e: inp.get("is_parse_err", False)
    agent._warning_presentation_enabled = lambda: inp.get("warning_presentation", False)
    agent._fire_stream_delta = lambda *a, **k: None
    agent._reset_stream_delivery_tracking = lambda: None
    agent._log_stream_retry = lambda **k: None
    agent._buffer_diagnostic_status = lambda *a, **k: None
    inst = make_instance(agent)
    inst._request_cancelled = {"value": inp.get("request_cancelled", False)}
    inst.deltas_were_sent = {"yes": inp.get("deltas_were_sent", False)}
    inst.first_delta_fired = {"done": False}
    inst.provider_tool_in_flight = {"yes": inp.get("provider_tool_in_flight", False)}
    inst.result = {"response": None, "error": None,
                   "partial_tool_names": list(inp.get("partial_tool_names", []))}
    inst._compat_retries = 0
    lifetime = inp.get("lifetime_error")
    inst._as_lifetime_error = lambda e: (NS(kind=lifetime["kind"]) if lifetime else e)
    inst._cancel_current_stream_attempt = lambda reason: None
    inst.clients.close_once = lambda reason: None
    inst._retry_after_drop = lambda *a, **k: None
    inst._maybe_disable_streaming = lambda e: None
    inst._unmask_server_error_with_nonstreaming = lambda e: inp.get("unmask_handles", False)
    import agent.stream_diag as SD
    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(M, "_is_sse_connection_error",
                                         lambda e: inp.get("sse_conn_predicate", False)))
        stack.enter_context(patch.object(M, "_rejects_stream_options",
                                         lambda e: inp.get("rejects_stream_options", False)))
        stack.enter_context(patch.object(SD, "buffer_connect_exhausted_notice", lambda *a, **k: None))
        retry = inst._handle_stream_error(make_error(inp), inp.get("attempt", 0), inp.get("max_retries", 0))
    re = inst.result["error"]
    result_error = (lifetime if lifetime else inp.get("error")) if re is not None else None
    return [{"a": "result", "retry": bool(retry), "result_error": jnorm(result_error)}]


def d_unmask(inp):
    agent = make_agent(api_mode=inp.get("api_mode", ""),
                       _interrupt_requested=inp.get("interrupt_requested", False),
                       _stream_5xx_probe_ts=inp.get("last_probe_ts"))
    agent._buffer_status = lambda *a, **k: None
    inst = make_instance(agent)
    inst.deltas_were_sent = {"yes": inp.get("deltas_were_sent", False)}
    inst.result = {"response": None, "error": None, "partial_tool_names": []}
    inst._stream_stale_timeout = 180.0
    outcome = inp.get("probe_outcome", "success")
    import agent.error_classifier as EC
    probe_status = inp.get("probe_status")
    status = inp.get("status")

    def fake_probe(agent_, kwargs):
        if outcome == "interrupt":
            raise InterruptedError("stop")
        if outcome == "error":
            e = Exception("probe")
            e.status_code = probe_status
            raise e
        return NS(**(inp.get("probe_response") or {}))

    def fake_extract(e):
        return getattr(e, "status_code", None)

    def fake_replay(resp):
        if not inp.get("replay_ok", True):
            raise Exception("replay failed")
        return resp

    orig_err = Exception("5xx")
    orig_err.status_code = status
    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(M, "_streams_llamacpp", lambda a: inp.get("llamacpp", False)))
        stack.enter_context(patch.object(M, "interruptible_api_call", fake_probe))
        stack.enter_context(patch.object(EC, "_extract_status_code", fake_extract))
        stack.enter_context(patch.object(M.time, "monotonic", lambda: inp.get("now", 0.0)))
        stack.enter_context(patch.object(inst, "_replay_final_response", fake_replay))
        stack.enter_context(patch.object(M, "_with_stream_emitters", lambda a, run: run()))
        try:
            handled = inst._unmask_server_error_with_nonstreaming(orig_err)
        except InterruptedError:
            return [{"a": "raise", "kind": "InterruptedError"}]
    return [{"a": "result", "handled": bool(handled),
             "result_error": jnorm(inp.get("probe_error")) if inst.result["error"] is not None else None,
             "result_response": jnorm(inst.result["response"])}]


def d_call(inp):
    agent = make_agent()
    inst = make_instance(agent)
    inst.result = {"response": None, "error": None, "partial_tool_names": []}
    wire = inp.get("wire", [])
    handle_retry = inp.get("handle_retry", [])
    route_switched = inp.get("route_switched", [])
    interrupt_before = inp.get("interrupt_before", [])
    ctr = {"n": 0, "wi": 0, "ei": 0}
    last_err = {}
    flags = {"route": False, "stop": False}
    close_reason = {}

    def fake_start():
        ctr["n"] += 1
        agent._interrupt_requested = (ctr["n"] - 1) in interrupt_before
        return ctr["n"]

    def fake_wire(attempt_id):
        i = ctr["wi"]; ctr["wi"] += 1
        w = wire[i] if i < len(wire) else {"outcome": "error", "error": {"kind": "X"}}
        if w.get("outcome") == "response":
            return NS(**(w.get("response") or {}))
        last_err["json"] = w.get("error")
        raise Exception((w.get("error") or {}).get("kind", "error"))

    def fake_handle(e, attempt, max_retries):
        i = ctr["ei"]; ctr["ei"] += 1
        r = handle_retry[i] if i < len(handle_retry) else False
        if not r:
            inst.result["error"] = e
            flags["stop"] = True
        return r

    def fake_route():
        i = ctr["ei"] - 1
        sw = route_switched[i] if i < len(route_switched) else False
        if sw:
            flags["route"] = True
        return sw

    inst._start_stream_attempt = fake_start
    inst._call_wire = fake_wire
    inst._handle_stream_error = fake_handle
    inst._route_switched_under_request = fake_route
    inst._cancel_current_stream_attempt = lambda reason: None
    inst._close_managed_stream = lambda: None
    inst.clients.close_once = lambda reason: close_reason.update(r=reason)
    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(M, "env_int",
                                         lambda name, dflt: inp.get("max_stream_retries", 2)
                                         if name == "HERMES_STREAM_RETRIES" else dflt))
        inst._call()
    if agent._interrupt_requested:
        outcome = "interrupted"
        err = {"kind": "InterruptedError"}
    elif inst.result["response"] is not None:
        outcome = "success"; err = None
    elif flags["route"]:
        outcome = "route_switched"; err = last_err.get("json")
    elif flags["stop"]:
        outcome = "handler_stop"; err = last_err.get("json")
    else:
        outcome = "budget_exhausted"; err = None
    return [{"a": "result", "outcome": outcome, "attempts": ctr["n"],
             "close_reason": close_reason.get("r"),
             "response": jnorm(inst.result["response"]), "error": jnorm(err)}]


def _mk_live_error(d):
    """Scripted error dict -> a REAL exception instance. Live run() does
    `raise error`, so a SimpleNamespace stub would raise TypeError instead of
    the scripted kind; build an exception whose type name is the kind."""
    if not d:
        return None
    import builtins
    kind = d.get("kind") or "Exception"
    base = getattr(builtins, kind, None)
    if not (isinstance(base, type) and issubclass(base, BaseException)):
        base = type(kind, (Exception,), {})
    return base(d.get("message", ""))


def d_run(inp):
    agent = make_agent(_interrupt_requested=inp.get("interrupt_requested", False),
                       _current_streamed_assistant_text=inp.get("streamed_text", ""))
    inst = make_instance(agent)
    inst.result = {"response": NS(**inp["result_response"]) if inp.get("result_response") else None,
                   "error": _mk_live_error(inp.get("result_error")),
                   "partial_tool_names": list(inp.get("partial_tool_names", []))}
    inst.deltas_were_sent = {"yes": inp.get("deltas_were_sent", False)}
    inst.provider_tool_in_flight = {"yes": inp.get("provider_tool_in_flight", False)}
    inst._abandon_error = _mk_live_error(inp.get("abandon_error"))
    inst._monitor_interrupted = {"yes": inp.get("monitor_interrupted", False)}
    inst.clients.diag = {"first_chunk_at": inp.get("diag_first_chunk_at")} if inp.get("diag_first_chunk_at") else None
    inst._resolve_stale_timeout = lambda: None
    inst._run_call = lambda: None
    inst._partial_stream_stub = lambda error: NS(**(inp.get("partial_stub") or {}))
    agent._reset_stream_delivery_tracking = lambda: None
    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(M, "should_use_direct_api_call", lambda a: True))
        stack.enter_context(patch.object(M, "_join_worker_for_relay_teardown", lambda *a, **k: None))
        stack.enter_context(patch.object(M.threading, "Thread", lambda *a, **k: NS(start=lambda: None, join=lambda **k: None)))
        try:
            resp = inst.run()
        except InterruptedError as e:
            return [{"a": "raise", "kind": "InterruptedError", "message": str(e)}]
        except Exception as e:
            return [{"a": "raise", "kind": getattr(e, "kind", type(e).__name__)}]
    if resp is not None and getattr(resp, "id", None) == M.PARTIAL_STREAM_STUB_ID:
        return [{"a": "return", "outcome": "stub", "response": jnorm(resp)}]
    return [{"a": "return", "outcome": "response", "response": jnorm(resp)}]


def d_reasoning(inp):
    agent = make_agent(reasoning_config=inp.get("reasoning_config"),
                       _ephemeral_reasoning_off=inp.get("ephemeral_reasoning_off", False),
                       _reasoning_effort_rejected=inp.get("effort_rejected", False),
                       _reasoning_disable_rejected=inp.get("disable_rejected", False),
                       _reasoning_floor_required=inp.get("floor_required", False))
    seq = []
    import agent.reasoning_params as RP
    import agent.auxiliary_reasoning_floor as AF
    orig_consume = M._consume_ephemeral_reasoning_off

    def consume(a):
        r = orig_consume(a)
        seq.append({"a": "consume_ephemeral", "consumed": bool(r)})
        return r

    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(M, "_consume_ephemeral_reasoning_off", consume))
        stack.enter_context(patch.object(RP, "unset_reasoning_default", lambda a: inp.get("unset_default")))
        stack.enter_context(patch.object(AF, "REASONING_FLOOR_EFFORT", inp.get("floor_effort", "low")))
        cfg = M._reasoning_config_for_wire(agent)
    seq.append({"a": "wire", "config": jnorm(cfg),
                "wire_reasoning_config": jnorm(getattr(agent, "_wire_reasoning_config", None)),
                "ephemeral_consumed": bool(inp.get("ephemeral_reasoning_off", False))})
    return seq


def _mk_watch_class(inp):
    w = inp.get("watch") or {}
    queues = [list(w.get("content", [])), list(w.get("reasoning", [])), list(w.get("detail", []))]
    state = {"n": 0}

    class W:
        def __init__(self):
            self.q = queues[state["n"]] if state["n"] < len(queues) else []
            state["n"] += 1

        def feed(self, text):
            if not text:
                return False
            return self.q.pop(0) if self.q else False
    return W


def d_chunk_loop(inp):
    agent = make_agent(stream_delta_callback=(lambda *a: None) if inp.get("stream_delta_callback") else None,
                       _hold_stream=inp.get("hold_stream", False))
    agent._fire_stream_delta = lambda *a, **k: None
    agent._fire_reasoning_delta = lambda *a, **k: None
    agent._fire_tool_gen_started = lambda *a, **k: None
    agent._record_streamed_assistant_text = lambda *a, **k: None
    agent._touch_activity = lambda *a, **k: None
    inst = make_instance(agent)
    inst.on_first_delta = lambda: None
    inst.deltas_were_sent = {"yes": inp.get("deltas_were_sent", False)}
    inst.result = {"response": None, "error": None, "partial_tool_names": []}
    seq = []
    chunks = [mk_chunk(c) for c in inp.get("chunks", [])]

    class FakeStream:
        final_response = NS(**inp["stream_final_response"]) if inp.get("stream_final_response") else None
        output_modified = False

        def __iter__(self):
            ia = inp.get("interrupt_at", [])
            for idx, ch in enumerate(chunks):
                agent._interrupt_requested = idx in ia
                yield ch

        def close(self):
            pass

    import agent.relay_llm as RL
    import agent.reasoning_carriers as RC
    import agent.chat_completion_helpers_relay as CHR
    orig_cl = M._StreamingCall._choiceless_chunk

    def cl(chunk, fr):
        try:
            usage, finish = orig_cl(chunk, fr)
            seq.append({"a": "choiceless", "usage": jnorm(usage) if usage else None, "finish_reason": finish})
            return usage, finish
        except M.ProviderStreamError as e:
            seq.append({"a": "raise", "kind": "ProviderStreamError", "status_code": e.status_code,
                        "body": jnorm(e.body), "raw_text": e.raw_text, "message": str(e)})
            raise

    def fire_first():
        will = not inst.first_delta_fired["done"] and inst.on_first_delta
        orig_ff()
        if will:
            seq.append({"a": "first_delta"})

    orig_ff = inst._fire_first_delta
    orig_et = inst._emit_text
    orig_er = inst._emit_reasoning
    orig_ts = inst._emit_tool_started
    orig_rs = inst._route_suppressed_text

    def emit_text(t):
        orig_et(t); seq.append({"a": "emit_text", "text": t})

    def emit_reasoning(t):
        orig_er(t); seq.append({"a": "emit_reasoning", "text": t})

    def emit_tool_started(n):
        orig_ts(n); seq.append({"a": "emit_tool_started", "name": n})

    def route_suppressed(t):
        if agent.stream_delta_callback and not getattr(agent, "_hold_stream", False):
            seq.append({"a": "route_suppressed", "text": t})
        orig_rs(t)

    def finish(stream, role, content_parts, reasoning_parts, tool_calls_acc, finish_reason,
               model_name, usage_obj, **kw):
        seq.append({"a": "finish_call", "content": "".join(content_parts),
                    "reasoning": "".join(reasoning_parts), "finish_reason": finish_reason,
                    "usage": jnorm(usage_obj), "tool_names": list(inst.result["partial_tool_names"]),
                    "acc": jnorm(tool_calls_acc), "refusal_parts": list(kw.get("refusal_parts") or []),
                    "reasoning_details": jnorm(kw.get("reasoning_details") or []), "model": model_name})
        return NS(id="sentinel")

    with contextlib.ExitStack() as stack:
        stack.enter_context(patch.object(RL, "stream", lambda *a, **k: FakeStream()))
        stack.enter_context(patch.object(M, "RunawayStreamWatch", _mk_watch_class(inp)))
        stack.enter_context(patch.object(RC, "StreamCarriers",
                                         lambda: NS(feed=lambda d, r: r, apply=lambda resp: None)))
        stack.enter_context(patch.object(CHR, "RelayChatAccumulator",
                                         lambda: NS(observe=lambda *a: None, finalize=lambda: None)))
        stack.enter_context(patch.object(M, "_streams_llamacpp", lambda a: inp.get("llamacpp", False)))
        inst._new_diag = lambda: {}
        inst._stream_attempt_is_active = lambda i: inp.get("attempt_active", True)
        inst._stream_attempt_was_cancelled = lambda i: inp.get("attempt_cancelled", False)
        inst._discard_stale_stream_chunk = lambda *a: None
        inst._close_managed_stream = lambda: None
        inst._choiceless_chunk = cl
        inst._fire_first_delta = fire_first
        inst._emit_text = emit_text
        inst._emit_reasoning = emit_reasoning
        inst._emit_tool_started = emit_tool_started
        inst._route_suppressed_text = route_suppressed
        inst._log_runaway_cut = lambda channel: seq.append({"a": "runaway_cut", "channel": channel})
        inst._finish_chat_stream = finish
        inst._adopt_final_response = lambda resp: (seq.append({"a": "adopt_final_response"}), resp)[1]
        try:
            inst._call_chat_completions(1)
        except M.ProviderStreamError:
            pass
        except Exception as e:
            if type(e).__name__ == "RemoteProtocolError":
                seq.append({"a": "raise", "kind": "RemoteProtocolError"})
            else:
                raise
    return seq


DRIVERS = {
    "tc_acc": d_tc_acc,
    "choiceless_chunk": d_choiceless,
    "assemble_tool_calls": d_assemble,
    "finish_chat_stream": d_finish,
    "provider_stream_error_from_text": d_pse_text,
    "build_partial_stream_stub": d_build_stub,
    "partial_stream_stub": d_partial_stub,
    "maybe_disable_streaming": d_maybe_disable,
    "retry_after_drop": d_retry_after_drop,
    "handle_stream_error": d_handle,
    "unmask_probe": d_unmask,
    "call_retry_loop": d_call,
    "run_outcome": d_run,
    "reasoning_config_for_wire": d_reasoning,
    "chat_chunk_loop": d_chunk_loop,
}


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        got = rec.get("out")
        cases += 1
        drv = DRIVERS.get(func)
        if drv is None:
            mism += 1
            print(f"MISMATCH {func}: no driver")
            continue
        try:
            want = jnorm(drv(inp))
        except Exception as e:
            mism += 1
            print(f"MISMATCH {func}: live driver raised {type(e).__name__}: {e}")
            continue
        if jnorm(got) != want:
            mism += 1
            print(f"MISMATCH {func}\n  C ={json.dumps(got, ensure_ascii=False)[:400]}\n"
                  f"  PY={json.dumps(want, ensure_ascii=False)[:400]}")
    print(f"ORACLE cch_flow: {cases} cases, {mism} mismatches")
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()