#!/usr/bin/env python3
"""Generator for tests/fixtures/tool_executor_flow.jsonl — the fixture-driven
cases for src/agent/tool_executor_flow.c (agent/tool_executor.py execution-
decision FLOW cores + leaves).

Case shape: {"func": <live name>, "in": {...}}. The "io" object scripts the
universal seam: absent/null -> None on every call; {"seq": [...]} -> per-call
sequence (None when exhausted); any other value -> reused verbatim (a bare
array is a RESULT, e.g. the 2-tuple of _pre_tool_block, NOT a queue).
Scripted exceptions: {"__raise__": "<ClassName>", "msg": str(e)}.

Value model (shared with the C port and the oracle):
  {"__attr__": {...}}      SimpleNamespace attribute double
  {"__callable__": true}   inert truthy callable; "<seam>" -> recorded stub
  {"__gate__" / "__future__" / "__budget__" / "__guardrails__": true} facades
  {"_type": "_ToolCallRef" | "_ManagedToolResult" | "_ParsedCall", ...fields}
  {"_type": "_ToolTimeoutResult" | "_ToolCancelledResult", "value": str}

Run:  python tests/gen_tool_executor_flow_cases.py
Writes tests/fixtures/tool_executor_flow.jsonl (LF, ensure_ascii=False).
"""
import json
import os
from collections import Counter

# ── shared templates ─────────────────────────────────────────────────────────

def ref(name="terminal", args=None):
    return {"_type": "_ToolCallRef", "name": name,
            "args": {"command": "ls"} if args is None else args,
            "task_id": "t1", "call_id": "c1", "trace": []}

def managed(args=None):
    return {"_type": "_ManagedToolResult", "result": None,
            "args": {"command": "ls"} if args is None else args,
            "middleware_trace": [], "blocked": False, "dispatched": False,
            "data_use": []}

AGENT_EMPTY = {"__attr__": {}}
AGENT_IRQ = {"__attr__": {"_interrupt_requested": False}}
AGENT_IRQ_TRUE = {"__attr__": {"_interrupt_requested": True}}
AGENT_TS = {"__attr__": {"enabled_toolsets": ["core"], "disabled_toolsets": None}}
AGENT_DISPATCH = {"__attr__": {
    "tool_progress_callback": {"__callable__": True},
    "clarify_callback": None,
    "_tool_guardrails": {"__guardrails__": True},
    "_turns_since_memory": 5, "_iters_since_skill": 3}}
AGENT_BOTH_CB = {"__attr__": {"tool_complete_callback": {"__callable__": True},
                              "tool_progress_callback": {"__callable__": True}}}
AGENT_NO_CB = {"__attr__": {"tool_complete_callback": None,
                            "tool_progress_callback": None}}
GATE = {"__gate__": True}
FUTURE = {"__future__": True}
BUDGET = {"__budget__": True}
CB = {"__callable__": "callback"}
BEGIN = {"__callable__": "begin_execution"}
EXEC = {"__callable__": "execute"}
GUARD_ALLOW = {"__attr__": {"allows_execution": True}}
TC = {"__attr__": {"function": {"__attr__": {
    "name": "read_file", "arguments": "{\"path\": \"x\"}"}}}}

def rais(cls, msg=""):
    return {"__raise__": cls, "msg": msg}

def dispatch_io(**over):
    io = {
        "hosted_tool_refusal": None,
        "_pre_tool_block": [None, {"command": "ls"}],
        "_context_pruned_argument_paths": [],
        "guardrails.before_call": GUARD_ALLOW,
        "prepare_current_terminal": None,
        "begin_execution": True,
        "_begin_tool_execution": None,
        "bound_tool_progress": None,
        "collect_data_use": ["b", "a", "b"],
        "bound_form_asker": None,
        "run_with_activity_heartbeat": True,
        "execute": "ok",
    }
    io.update(over)
    return io

def dispatch_case(name="terminal", args=None, gate=None, begin=BEGIN,
                  scope=None, io=None, **io_over):
    r = ref(name, args)
    return {
        "execute": EXEC, "begin_execution": begin, "authorization_gate": gate,
        "scope_block": scope, "display_index": 1,
        "state": {"agent": AGENT_DISPATCH, "state": managed(r["args"]), "ref": r},
        "io": dispatch_io(**io_over) if io is None else io,
    }

# ── cases ────────────────────────────────────────────────────────────────────

def build():
    out = []
    def add(func, **inp):
        out.append({"func": func, "in": inp})

    # _tc_name: getattr chain + `or "tool"` falsy set
    for tc in (
        {"__attr__": {"function": {"__attr__": {"name": "read_file"}}}},
        {"__attr__": {"function": {"__attr__": {"name": ""}}}},
        {"__attr__": {"function": {"__attr__": {"name": None}}}},
        {"__attr__": {}},
        {"__attr__": {"function": None}},
        {"function": {"name": "x"}},                      # plain dict: no attrs
        {"__attr__": {"function": {"__attr__": {"name": 0}}}},
        None,
    ):
        add("_tc_name", tool_call=tc)

    # _tool_progress_enabled: not quiet_mode and mode != "off"
    for attrs in (
        {"quiet_mode": False, "tool_progress_mode": "all"},
        {"quiet_mode": True, "tool_progress_mode": "all"},
        {"quiet_mode": False, "tool_progress_mode": "off"},
        {"quiet_mode": False},
        {"quiet_mode": False, "tool_progress_mode": "verbose"},
        {"tool_progress_mode": "all"},                    # missing quiet_mode
        {"quiet_mode": 0, "tool_progress_mode": "off"},
    ):
        add("_tool_progress_enabled", agent={"__attr__": attrs})

    # _preview: codepoint slice + "..." / len / comparison TypeErrors
    for text, limit in (
        ("hello world", 5), ("hello", 5), ("hello", 10),
        ("héllo wörld", 5), ("abcdef", -1), ("abc", 0), ("", 0),
        ("日本語テキスト", 3), (5, 3), ("abc", "x"),
    ):
        add("_preview", text=text, limit=limit)

    # _start_order_gate_timeout: None -> stock; min(120, t/2)
    for bt in (None, 0, 100, 240, 300, 0.5, -10):
        add("_start_order_gate_timeout", batch_timeout=bt)

    # _delegate_spinner_label: str(action or "").strip().lower() branches
    for fa in (
        {"action": "list"},
        {"action": " STEER "},
        {"action": "Stop"},
        {"action": "run", "tasks": [1, 2]},
        {"action": "run", "tasks": [], "goal": "fix the bug"},
        {"goal": "a very long goal that exceeds thirty codepoints"},
        {},
        {"goal": 123},                                    # not subscriptable
        {"goal": 1.5},
        {"action": None, "tasks": "x", "goal": ""},
        {"action": "\u00a0LIST\u00a0"},                   # NBSP strip
        {"goal": "日本語のゴールテキストで三十文字を超える長い文章を書く"},
    ):
        add("_delegate_spinner_label", function_args=fa)

    # _safe_callback: falsy skip, call, except Exception -> logging.debug
    add("_safe_callback", callback=None, label="L")
    add("_safe_callback", callback=0, label="L")
    add("_safe_callback", callback="", label="L")
    add("_safe_callback", callback=CB, label="Tool progress",
        args=[1, "x"], kwargs={"k": True}, io={"callback": None})
    add("_safe_callback", callback=CB, label="Tool progress", args=[], kwargs={},
        io={"callback": rais("RuntimeError", "boom")})
    add("_safe_callback", callback=CB, label="L", args=[], kwargs={},
        io={"callback": rais("KeyboardInterrupt", "stop")})
    add("_safe_callback", callback=5, label="L")          # not callable
    add("_safe_callback", callback=CB, label="L", args=["a"], kwargs={"z": 1},
        io={"callback": rais("ValueError", "bad")})

    # _interrupt_worker_tids: per-tid _ra()._set_interrupt under suppress
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[])
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[1, 2],
        io={"_set_interrupt": None})
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[7], reason="stop",
        io={"_set_interrupt": None})
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[7], reason=None,
        io={"_set_interrupt": None})
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[1, 2, 3],
        io={"_set_interrupt": {"seq": [None, rais("RuntimeError", "x"), None]}})
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[1, 2],
        io={"_set_interrupt": rais("KeyboardInterrupt", "stop")})
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[1, 2],
        io={"_ra": rais("RuntimeError", "no run_agent")})

    # _blocked_tool_result: block_body vs guardrail projection + emit_post
    def blocked(block_body, bet, decision=None, io=None):
        kw = dict(agent=AGENT_EMPTY, ref=ref(), block_body=block_body,
                  block_error_type=bet)
        if decision is not None:
            kw["guardrail_decision"] = decision
        if io is not None:
            kw["io"] = io
        add("_blocked_tool_result", **kw)
    blocked({"error": "nope", "message": "detail"}, "tool_scope_block",
            io={"_emit_terminal_post_tool_call": None})
    blocked({"error": "nope"}, "plugin_block",
            io={"_emit_terminal_post_tool_call": None})
    blocked({"message": ""}, "plugin_block")              # KeyError 'error'
    blocked({"message": 0, "error": "e"}, "hosted_tool_refused",
            io={"_emit_terminal_post_tool_call": None})
    blocked(None, "unused", {"__attr__": {"message": "denied"}},
            {"guardrail_block_result": "blocked-json",
             "_emit_terminal_post_tool_call": None})
    blocked(None, "unused", {"__attr__": {"message": None}},
            {"guardrail_block_result": "bj", "_emit_terminal_post_tool_call": None})
    blocked(None, "unused", {},
            {"guardrail_block_result": "bj", "_emit_terminal_post_tool_call": None})
    blocked({"error": "no", "message": "héllo ⟪"}, "tool_scope_block",
            io={"_emit_terminal_post_tool_call": None})
    blocked(None, "unused", {"__attr__": {"message": "m"}},
            {"guardrail_block_result": rais("RuntimeError", "x")})
    blocked({"error": "e"}, "plugin_block",
            io={"_emit_terminal_post_tool_call": rais("RuntimeError", "hook down")})

    # _pre_tool_block: import -> tool_hook_ids -> dispatch; except -> fallback
    def pre(io):
        add("_pre_tool_block", agent=AGENT_EMPTY, ref=ref(), io=io)
    pre({"_dispatch_pre_tool_call_hooks": ["blocked", None]})
    pre({"_dispatch_pre_tool_call_hooks": [None, {"x": 1}]})
    pre({"_dispatch_pre_tool_call_hooks": [None, None]})
    pre({"import_dispatch_pre_tool_call_hooks": rais("ImportError", "no plugins")})
    pre({"_dispatch_pre_tool_call_hooks": rais("RuntimeError", "hook broke")})
    pre({"_dispatch_pre_tool_call_hooks": rais("KeyboardInterrupt", "stop")})
    pre({"tool_hook_ids": {"session_id": "s1", "turn_id": "t9"},
         "_dispatch_pre_tool_call_hooks": [None, None]})
    pre({"_dispatch_pre_tool_call_hooks": None})          # unpack None TypeError
    pre({"tool_hook_ids": rais("RuntimeError", "ids broke")})

    # _unwrap_tool_search_call: bridge peel + scope + deferred-schema probe
    def unwrap(fname, fargs, io=None, flatten=False):
        kw = dict(agent=AGENT_TS, function_name=fname, function_args=fargs)
        if flatten:
            kw["flatten_probe"] = True
        if io is not None:
            kw["io"] = io
        add("_unwrap_tool_search_call", **kw)
    unwrap("read_file", {"path": "x"})
    unwrap("tool_call", {}, {"import_tool_search": rais("ImportError", "no ts")})
    unwrap("tool_call", {"name": "terminal"},
           {"resolve_underlying_call": ["terminal", {"command": "ls"}, None],
            "_tool_search_scoped_names": ["terminal"],
            "validate_deferred_call_args": None})
    unwrap("tool_call", {}, {"resolve_underlying_call": [None, {}, "bad args"]})
    unwrap("tool_call", {}, {"resolve_underlying_call": ["", {}, None]})
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["connectors__execute", {"calls": []}, None]})
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["gui_tool", {}, None],
            "_tool_search_scoped_names": [],
            "scope_block_reason": "'gui_tool' is not available in this session."})
    probe = '{"error": "E", "parameters": {"a": 1}, "hint": "H"}'
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["terminal", {"command": "ls"}, None],
            "_tool_search_scoped_names": ["terminal"],
            "validate_deferred_call_args": probe})
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["terminal", {"command": "ls"}, None],
            "_tool_search_scoped_names": ["terminal"],
            "validate_deferred_call_args": probe}, flatten=True)
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["terminal", {}, None],
            "_tool_search_scoped_names": ["terminal"],
            "validate_deferred_call_args": "not json"}, flatten=True)
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["terminal", {}, None],
            "_tool_search_scoped_names": ["terminal"],
            "validate_deferred_call_args": "[1, 2]"}, flatten=True)
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["terminal", {}, None],
            "_tool_search_scoped_names": None})
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["terminal", {}, None],
            "_tool_search_scoped_names": ["terminal"],
            "validate_deferred_call_args": rais("RuntimeError", "schema broke")})
    unwrap("tool_call", {},
           {"resolve_underlying_call": ["terminal", {}, None],
            "_tool_search_scoped_names": ["terminal"],
            "validate_deferred_call_args":
                '{"parameters": {"q": "héllo"}, "hint": "  use it  "}'},
           flatten=True)

    # _parse_tool_call: canonicalize -> parse args -> unwrap -> _ParsedCall
    def parse(io=None, flatten=False, tc=TC):
        kw = dict(agent=AGENT_EMPTY, tool_call=tc)
        if flatten:
            kw["flatten_probe"] = True
        if io is not None:
            kw["io"] = io
        add("_parse_tool_call", **kw)
    parse({"_canonical_tool_name": "read_file",
           "_parse_tool_arguments": [{"path": "x"}, None],
           "_unwrap_tool_search_call": ["read_file", {"path": "x"}, None]})
    parse({"_canonical_tool_name": "read_file",
           "_parse_tool_arguments": [{}, "Invalid tool arguments"]})
    parse({"_canonical_tool_name": "tool_call",
           "_parse_tool_arguments": [{"name": "terminal"}, None],
           "_unwrap_tool_search_call": ["terminal", {"command": "ls"}, None]},
          flatten=True)
    parse({"_canonical_tool_name": rais("RuntimeError", "alias broke")})
    parse({"_canonical_tool_name": "read_file", "_parse_tool_arguments": None})
    parse({"_canonical_tool_name": "read_file",
           "_parse_tool_arguments": [{"path": "x"}, None],
           "_unwrap_tool_search_call": ["read_file", {"path": "x"}]})
    parse(tc={"function": {"name": "x"}})                 # plain dict carrier

    # _dispatch_authorized_once: the admission/guard/dispatch decision chain
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={"hosted_tool_refusal": "hosted says no",
                                    "_blocked_tool_result": "R"}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(scope="out of scope",
                           io_over={"_blocked_tool_result": "R2"}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={"_pre_tool_block": ["blocked by plugin", None],
                                    "_blocked_tool_result": "R3"}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(gate=GATE,
                           io_over={"_pre_tool_block": ["no", None],
                                    "gate.run": True, "_blocked_tool_result": "R4"}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={
            "_pre_tool_block": [None, {"command": "rewritten-by-plugin"}],
            "_context_pruned_argument_paths": ["command"],
            "_blocked_tool_result": "R5"}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={
            "guardrails.before_call":
                {"__attr__": {"allows_execution": False, "message": "deny"}},
            "_blocked_tool_result": "R6"}))
    add("_dispatch_authorized_once", **dispatch_io_case())
    add("_dispatch_authorized_once",
        **dispatch_io_case(name="memory", args={"action": "add"},
                           io_over={"execute": "mem-ok"}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(name="skill_manage", args={"action": "create"},
                           io_over={"execute": "skill-ok"}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={"execute": rais("RuntimeError", "tool broke"),
                                    "collect_data_use": ["z"]}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(gate=GATE,
                           io_over={"gate.run": rais("RuntimeError", "wedged")}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={"hosted_tool_refusal": "no",
                                    "begin_execution": rais("RuntimeError",
                                                            "gate wedged")}))
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={"collect_data_use": None}))
    add("_dispatch_authorized_once", **dispatch_io_case(begin=None))
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={
            "import_hosted_tool_policy": rais("ImportError", "no policy")}))

    # _poll_sequential_future: interrupt-poll slices + deadline extension
    def poll(deadline, io, irq=False):
        add("_poll_sequential_future",
            agent=AGENT_IRQ_TRUE if irq else AGENT_IRQ, future=FUTURE,
            function_name="terminal", deadline=deadline, started=0.0,
            authorization_gate=GATE, io=io)
    poll(None, {"future.result": "done!"})
    poll(10, {"gate.excluded_seconds": 0, "time.monotonic": 100})
    poll(100, {"gate.excluded_seconds": 0, "time.monotonic": 99.5,
               "future.result": "x"})
    poll(None, {"future.result": {"seq": [rais("TimeoutError"), "late"]},
                "future.done": False, "time.monotonic": 5})
    poll(None, {"future.result": {"seq": [rais("TimeoutError"), "settled"]},
                "future.done": True})
    poll(None, {"future.result": rais("TimeoutError"), "future.done": False},
         irq=True)
    poll(None, {"future.result": {"seq": [rais("TimeoutError"),
                                          rais("TimeoutError"), "end"]},
                "future.done": False, "time.monotonic": {"seq": [31, 61]},
                "_touch_activity": None})
    poll(None, {"future.result": {"seq": [rais("TimeoutError"),
                                          rais("TimeoutError"), "end"]},
                "future.done": False, "time.monotonic": {"seq": [10, 39]},
                "_touch_activity": None})
    poll(None, {"future.result": rais("ValueError", "worker died")})
    poll(10, {"gate.excluded_seconds": 50, "time.monotonic": 20,
              "future.result": "ok"})
    poll(None, {"future.result": rais("KeyboardInterrupt", "stop")})

    # _unfinished_tool_result: timeout / interrupt / thread-missing projection
    def unfinished(timed_out, ts, io, irq=False):
        add("_unfinished_tool_result",
            agent=AGENT_IRQ_TRUE if irq else AGENT_IRQ, ref=ref(),
            timed_out=timed_out, timeout_s=ts, io=io)
    unfinished(True, 420, {"_emit_terminal_post_tool_call": None})
    unfinished(True, None, {"_emit_terminal_post_tool_call": None})
    unfinished(True, 0, {"_emit_terminal_post_tool_call": None})
    unfinished(True, 12.34, {"_emit_terminal_post_tool_call": None})
    unfinished(False, None, {"interrupt_skip_wording": "User pressed stop",
                             "_emit_terminal_post_tool_call": None}, irq=True)
    unfinished(False, None, {"interrupt_skip_wording": "",
                             "_emit_terminal_post_tool_call": None}, irq=True)
    unfinished(False, None, {"_emit_terminal_post_tool_call": None})
    unfinished(True, 420,
               {"_emit_terminal_post_tool_call": rais("RuntimeError", "hook down")})
    unfinished(False, None,
               {"interrupt_skip_wording": rais("RuntimeError", "x")}, irq=True)

    # _abandoned_sequential_result: emit_post + marker-wrapped managed result
    def abandoned(msg, cls, outcome, io):
        add("_abandoned_sequential_result", agent=AGENT_EMPTY, ref=ref(),
            message=msg, result_cls=cls, outcome=outcome, io=io)
    abandoned("timed out", "_ToolTimeoutResult",
              {"duration_ms": 1000, "status": "timeout",
               "error_type": "tool_timeout", "error_message": "timed out"},
              {"_emit_terminal_post_tool_call": None})
    abandoned("cancelled", "_ToolCancelledResult", {},
              {"_emit_terminal_post_tool_call": None})
    abandoned("m", "_ToolTimeoutResult", {"status": "timeout", "extra": "x"},
              {"_emit_terminal_post_tool_call": None})
    abandoned("m", "_ToolTimeoutResult", {},
              {"_emit_terminal_post_tool_call": rais("RuntimeError", "x")})
    abandoned("m", "_ToolCancelledResult", {"duration_ms": 0, "status": "cancelled"},
              {"_emit_terminal_post_tool_call": None})
    abandoned("m", "_ToolCancelledResult",
              {"status": "cancelled", "error_type": "tool_interrupted",
               "error_message": "m"},
              {"_emit_terminal_post_tool_call": None})

    # _persist_multimodal_text_parts: per-part spill + summary reuse
    def persist(result, io=None, env=None):
        kw = dict(result=result, tool_name="browser_exec", tool_call_id="c1",
                  env=env, budget=BUDGET)
        if io is not None:
            kw["io"] = io
        add("_persist_multimodal_text_parts", **kw)
    persist({})
    persist({"content": None})
    persist({"content": []})
    persist({"content": [{"type": "text", "text": "big"}]},
            {"maybe_persist_tool_result": "SPILL"})
    persist({"content": [{"type": "image", "data": "x"}]})
    persist({"content": ["raw"]})
    persist({"content": [{"type": "text", "text": 5}]})
    persist({"content": [{"type": "text", "text": "a"},
                         {"type": "text", "text": "b"}]},
            {"maybe_persist_tool_result": {"seq": ["S1", "S2"]}}, env={"cwd": "/w"})
    persist({"content": [{"type": "text", "text": "big"}]},
            {"maybe_persist_tool_result": "big"})
    persist({"content": [{"type": "text", "text": "a"},
                         {"type": "text", "text": "b"}]},
            {"maybe_persist_tool_result": {"seq": ["", "X"]}})
    persist({"content": [{"type": "text", "text": "big"}]},
            {"maybe_persist_tool_result": None})
    persist({"content": [{"type": "text", "text": "big"}],
             "text_summary": "long summary"},
            {"maybe_persist_tool_result": "S", "budget.resolve_threshold": 5})
    persist({"content": [{"type": "text", "text": "big"}],
             "text_summary": "long summary"},
            {"maybe_persist_tool_result": "S", "budget.resolve_threshold": 100})
    persist({"content": [{"type": "text", "text": "big"}], "text_summary": 5},
            {"maybe_persist_tool_result": "S"})
    persist({"content": 5})                               # not iterable

    # _emit_tool_complete_and_risk: complete callback + output-risk projection
    def emit(agent, risk, blocked, io=None):
        kw = dict(agent=agent, ref=ref(), result="r", risk_metadata=risk,
                  blocked=blocked)
        if io is not None:
            kw["io"] = io
        add("_emit_tool_complete_and_risk", **kw)
    emit(AGENT_BOTH_CB, None, True)
    emit(AGENT_NO_CB, None, False)
    emit(AGENT_BOTH_CB, None, False,
         {"_redact_tool_args_for_display": {"secret": "[redacted]"},
          "_safe_callback": None})
    emit(AGENT_BOTH_CB, None, False,
         {"_redact_tool_args_for_display": None, "_safe_callback": None})
    emit(AGENT_BOTH_CB, None, False,
         {"_redact_tool_args_for_display": {}, "_safe_callback": None})
    emit(AGENT_BOTH_CB, None, False,
         {"_redact_tool_args_for_display": rais("RuntimeError", "redact broke")})
    emit(AGENT_BOTH_CB, None, False,
         {"_redact_tool_args_for_display": rais("KeyboardInterrupt", "stop")})
    emit(AGENT_BOTH_CB, {"risk": "low"}, True)
    emit(AGENT_BOTH_CB, {"risk": "high"}, True, {"_safe_callback": None})
    emit(AGENT_BOTH_CB, {"risk": "high", "detail": "d"}, False,
         {"_redact_tool_args_for_display": None,
          "_safe_callback": {"seq": [None, None]}})
    emit(AGENT_NO_CB, {"risk": "high"}, False, {"_safe_callback": None})
    emit(AGENT_BOTH_CB, [1], True)                        # .get AttributeError
    emit({"__attr__": {"tool_progress_callback": None}}, None, False)

    # advance (_WorkerStartOnce): once-only gate advance / _BatchAbandoned
    def advance(advanced, cb, io=None):
        kw = dict(state={"_advanced": advanced, "_order": 2,
                         "_tool_name": "terminal", "_gate": GATE}, callback=cb)
        if io is not None:
            kw["io"] = io
        add("advance", **kw)
    advance(False, None, {"gate.begin_in_order": True})
    advance(True, None)
    advance(False, None, {"gate.begin_in_order": False})
    advance(False, None, {"gate.begin_in_order": None})
    advance(False, None, {"gate.begin_in_order": rais("RuntimeError", "gate broke")})
    advance(False, CB, {"gate.begin_in_order": True})

    # ── added edge / fail-closed cases ──────────────────────────────────────
    # _pre_tool_block: **non-mapping hook ids -> TypeError swallowed -> fallback
    pre({"tool_hook_ids": None})
    pre({"tool_hook_ids": ["a"]})
    # _dispatch_authorized_once: no scripts at all (fail-closed None seams)
    add("_dispatch_authorized_once", **dispatch_io_case(io={}))
    # execute BaseException propagates past finally + CM exits
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={"execute": rais("KeyboardInterrupt", "stop")}))
    # a CM __exit__ raise replaces the successful return
    add("_dispatch_authorized_once",
        **dispatch_io_case(io_over={
            "collect_data_use.__exit__": rais("RuntimeError", "exit broke")}))
    # direct _begin_tool_execution (no start-order gate) raising
    add("_dispatch_authorized_once",
        **dispatch_io_case(begin=None,
                           io_over={"_begin_tool_execution":
                                    rais("RuntimeError", "begin broke")}))
    # _unwrap_tool_search_call: fail-closed (resolve None -> unpack TypeError)
    unwrap("tool_call", {}, {})
    # _poll_sequential_future: excluded_seconds seam raise propagates
    poll(10, {"gate.excluded_seconds": rais("RuntimeError", "no gate")})
    # _persist: spill seam raise mid-loop; threshold seam raise
    persist({"content": [{"type": "text", "text": "big"}]},
            {"maybe_persist_tool_result": rais("OSError", "disk full")})
    persist({"content": [{"type": "text", "text": "big"}],
             "text_summary": "long summary"},
            {"maybe_persist_tool_result": "S",
             "budget.resolve_threshold": rais("RuntimeError", "no budget")})
    # _emit_tool_complete_and_risk: logging.debug raise on the redact-error path
    emit(AGENT_BOTH_CB, None, False,
         {"_redact_tool_args_for_display": rais("RuntimeError", "redact broke"),
          "logging.debug": rais("RuntimeError", "log down")})
    # _safe_callback seam BaseException propagates out of the risk projection
    emit(AGENT_BOTH_CB, {"risk": "high"}, True,
         {"_safe_callback": rais("SystemExit", "bye")})
    # _blocked_tool_result: non-dict block_body; emit BaseException
    blocked("x", "plugin_block")
    blocked({"error": "e"}, "plugin_block",
            io={"_emit_terminal_post_tool_call": rais("KeyboardInterrupt", "stop")})
    # _unfinished_tool_result: wording BaseException propagates
    unfinished(False, None,
               {"interrupt_skip_wording": rais("SystemExit", "bye")}, irq=True)
    # _safe_callback: logging.debug raise after a caught callback error
    add("_safe_callback", callback=CB, label="L", args=[], kwargs={},
        io={"callback": rais("ValueError", "bad"),
            "logging.debug": rais("RuntimeError", "log down")})
    # _interrupt_worker_tids: GeneratorExit passes suppress(Exception)
    add("_interrupt_worker_tids", agent=AGENT_EMPTY, tids=[1, 2],
        io={"_set_interrupt": rais("GeneratorExit")})
    # advance: gate BaseException propagates; _advanced stays set
    advance(False, None, {"gate.begin_in_order": rais("KeyboardInterrupt", "stop")})
    # _parse_tool_call: unwrap seam raise propagates (no try)
    parse({"_canonical_tool_name": "read_file",
           "_parse_tool_arguments": [{"path": "x"}, None],
           "_unwrap_tool_search_call": rais("RuntimeError", "unwrap broke")})
    # small leaf edges
    add("_tc_name", tool_call={"__attr__": {"function": {"__attr__": {"name": False}}}})
    add("_tool_progress_enabled",
        agent={"__attr__": {"quiet_mode": None, "tool_progress_mode": "off"}})
    add("_preview", text="abcd", limit=True)
    add("_delegate_spinner_label", function_args={"tasks": {"a": 1}, "goal": "g"})

    return out


def dispatch_io_case(name="terminal", args=None, gate=None, begin=BEGIN,
                     scope=None, io=None, io_over=None):
    """Keyword form of dispatch_case for add(**...)."""
    return dispatch_case(name=name, args=args, gate=gate, begin=begin,
                         scope=scope, io=io, **(io_over or {}))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "fixtures", "tool_executor_flow.jsonl")
    cs = build()
    cnt = Counter(c["func"] for c in cs)
    short = {f: n for f, n in cnt.items() if n < 5}
    if short:
        raise SystemExit("fixture coverage below 5/func: %s" % short)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for c in cs:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases across %d funcs -> %s" % (len(cs), len(cnt), path))
    for f, n in sorted(cnt.items()):
        print("  %-32s %d" % (f, n))


if __name__ == "__main__":
    main()