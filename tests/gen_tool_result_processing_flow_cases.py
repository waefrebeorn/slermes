#!/usr/bin/env python3
"""Generator for tests/fixtures/tool_result_processing_flow.jsonl — the
fixture-driven cases for src/agent/tool_result_processing_flow.c (the
agent/tool_executor.py tool-result-processing FLOW cores + leaves).

Case shape: {"func": <live name>, "in": {...}}. The "io" object scripts the
universal seam: absent/null -> None on every call; {"seq": [...]} -> per-call
sequence (None when exhausted); any other value -> reused verbatim (a bare
array is a RESULT, e.g. the 3-tuple of _commit_tool_result, NOT a queue).
Scripted exceptions: {"__raise__": "<ClassName>", "msg": str(e)}.

Value model (shared with the C port and the oracle):
  {"__attr__": {...}}      SimpleNamespace attribute double
  {"__callable__": true}   inert truthy callable; "<seam>" -> recorded stub
  {"__budget__" / "__guardrails__" / "__subdir__" / "__spinner__" /
   "__event__" / "__thread__" / "__lock__": true} facades
  {"__set__": true, "items": [...]} worker-thread set double
  {"_type": "_ToolCallRef" | "_ManagedToolResult" | "_ParsedCall" |
   "_SequentialDispatch", ...fields}
  {"_type": "_ToolTimeoutResult" | "_ToolCancelledResult", "value": str}

Domain notes: event.wait scripts must terminate (a truthy value within the
seq) — a None wait loops forever live; str()-using leaves take scalar
results; unpack seams return fixed-length arrays.

Run:  python tests/gen_tool_result_processing_flow_cases.py
Writes tests/fixtures/tool_result_processing_flow.jsonl (LF, ensure_ascii=False).
"""
import json
import os
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tool_result_processing_flow.jsonl")


def ref(name="terminal", args=None, task_id="t1", call_id="c1", trace=None):
    return {"_type": "_ToolCallRef", "name": name,
            "args": {"command": "ls"} if args is None else args,
            "task_id": task_id, "call_id": call_id,
            "trace": [] if trace is None else trace}


def managed(result="ok", args=None, trace=None, blocked=False, data_use=None):
    return {"_type": "_ManagedToolResult", "result": result,
            "args": {"command": "ls"} if args is None else args,
            "middleware_trace": [] if trace is None else trace,
            "blocked": blocked, "dispatched": True,
            "data_use": [] if data_use is None else data_use}


def pc(name="terminal", parse_error=None, scope_block=None, tool_call=None):
    return {"_type": "_ParsedCall",
            "tool_call": {"__attr__": {"id": "c1"}} if tool_call is None else tool_call,
            "name": name, "args": {"command": "ls"}, "middleware_trace": [],
            "parse_error": parse_error, "scope_block": scope_block}


def dispatch_d(execute=None, spinner=None, error_result=None, error_log="",
               handles_ki=False, is_delegate=False, finish_spinner=True,
               finish_in_finally=True, transform_applied=False, trace_arg=None):
    return {"_type": "_SequentialDispatch",
            "execute": {"__callable__": "dispatch_execute"} if execute is None else execute,
            "spinner": spinner, "middleware_trace_arg": trace_arg,
            "error_result": error_result, "error_log": error_log,
            "handles_keyboard_interrupt": handles_ki, "is_delegate": is_delegate,
            "finish_spinner": finish_spinner, "finish_in_finally": finish_in_finally,
            "transform_applied": transform_applied}


def rais(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def seq(*vals):
    return {"seq": list(vals)}


def cb(name):
    return {"__callable__": name}


BUDGET = {"__budget__": True}
SPINNER = {"__spinner__": True}
EVENT = {"__event__": True}
LOCK = {"__lock__": True}
TC = {"__attr__": {"function": {"__attr__": {"name": "read_file"}}, "id": "c1"}}
TC2 = {"__attr__": {"function": {"__attr__": {"name": "patch"}}, "id": "c2"}}

AGENT_COMMIT = {"__attr__": {
    "verbose_logging": False,
    "_append_guardrail_observation": cb("_append_guardrail_observation"),
    "_record_file_mutation_result": cb("_record_file_mutation_result"),
    "_touch_activity": cb("_touch_activity"),
    "_subdirectory_hints": {"__subdir__": True},
    "_tool_result_content_for_active_model": cb("_tool_result_content_for_active_model"),
    "tool_progress_callback": cb("tool_progress_callback"),
}}
AGENT_QUIET = {"__attr__": {
    "_should_emit_quiet_tool_messages": cb("_should_emit_quiet_tool_messages"),
    "_should_start_quiet_spinner": cb("_should_start_quiet_spinner"),
    "_print_fn": cb("_print_fn"),
    "_safe_print": cb("_safe_print"),
    "_vprint": cb("_vprint"),
    "log_prefix": "[s] ",
}}
AGENT_WORKER = {"__attr__": {
    "_tool_worker_threads_lock": LOCK,
    "_tool_worker_threads": {"__set__": True, "items": []},
}}


def commit_io(**over):
    io = {
        "import_observe_tool_outcome": None,
        "observe_tool_outcome": None,
        "observe_onboarding_result": "observed-result",
        "_is_multimodal_tool_result": False,
        "get_active_env": {"__attr__": {"cwd": "/w"}},
        "maybe_persist_tool_result": "persisted-text",
        "_record_persisted_path_for_stub": None,
        "note_tool_result": None,
        "subdirectory_hints.check_tool_call": None,
        "_tool_result_content_for_active_model": "model-content",
        "make_tool_result_message": {"role": "tool", "content": "model-content",
                                     "tool_call_id": "c1", "_tool_output_risk": {"level": "low"}},
        "lsp.forward_to_agent": None,
        "plugin_live_notes.attach_to_tool_message": None,
        "stamp_data_use": None,
        "_flush_session_db_after_tool_progress": True,
        "_safe_callback": None,
    }
    io.update(over)
    return io


def build():
    out = []

    def add(func, **inp):
        out.append({"func": func, "in": inp})

    # ── _record_persisted_path_for_stub ──
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result="spill ref", state={"agent": {"__attr__": {
            "_tool_guardrails": {"__guardrails__": True}}}},
        io={"extract_persisted_path": "/spill/c1.txt",
            "guardrails.record_persisted_result": None})
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result="no spill", state={"agent": {"__attr__": {
            "_tool_guardrails": {"__guardrails__": True}}}},
        io={"extract_persisted_path": None})
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result={"text_summary": "sum", "content": [
            {"type": "text", "text": "part0"}, "not-a-dict",
            {"type": "text", "text": "part1"}]},
        state={"agent": {"__attr__": {"_tool_guardrails": {"__guardrails__": True}}}},
        io={"_is_multimodal_tool_result": True,
            "extract_persisted_path": seq(None, "/spill/part0.txt"),
            "guardrails.record_persisted_result": None})
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result={"plain": "dict"},
        state={"agent": {"__attr__": {"_tool_guardrails": {"__guardrails__": True}}}},
        io={"_is_multimodal_tool_result": False})
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result="fail-closed", state={"agent": {"__attr__": {
            "_tool_guardrails": {"__guardrails__": True}}}}, io={})
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result="boom", state={"agent": {"__attr__": {
            "_tool_guardrails": {"__guardrails__": True}}}},
        io={"extract_persisted_path": rais("ValueError", "bad path")})
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result="boom", state={"agent": {"__attr__": {
            "_tool_guardrails": {"__guardrails__": True}}}},
        io={"extract_persisted_path": rais("KeyboardInterrupt")})
    add("_record_persisted_path_for_stub", tool_call_id="c1",
        function_result=None, state={"agent": {"__attr__": {
            "_tool_guardrails": {"__guardrails__": True}}}},
        io={"_is_multimodal_tool_result": True})

    # ── _canonical_tool_name ──
    add("_canonical_tool_name", function_name="terminal",
        io={"import_legacy_tool_aliases": {"terminal": "run_terminal"}})
    add("_canonical_tool_name", function_name="read_file",
        io={"import_legacy_tool_aliases": {"terminal": "run_terminal"}})
    add("_canonical_tool_name", function_name="read_file", io={})
    add("_canonical_tool_name", function_name="read_file",
        io={"import_legacy_tool_aliases": rais("RuntimeError", "no module")})
    add("_canonical_tool_name", function_name="read_file",
        io={"import_legacy_tool_aliases": rais("SystemExit", "0")})
    add("_canonical_tool_name", function_name="terminal",
        io={"import_legacy_tool_aliases": {"terminal": 5}})

    # ── _append_skipped_tool_results ──
    skip_state = {"agent": {"__attr__": {}}, "messages": []}
    add("_append_skipped_tool_results", tool_calls=[TC, TC2],
        effective_task_id="t1", content="skipped {name} twice {name}",
        state=skip_state,
        io={"_tc_name": seq("read_file", "patch"),
            "_pairing_tool_call_id": seq("c1", "c2"),
            "make_tool_result_message": seq(
                {"role": "tool", "content": "skipped read_file twice read_file"},
                {"role": "tool", "content": "skipped patch twice patch"})})
    add("_append_skipped_tool_results", tool_calls=[TC],
        effective_task_id="t1", content="cancelled {name}",
        hook_error_type="user_interrupt", hook_id=cb("hook_id"),
        state=skip_state,
        io={"_tc_name": "read_file", "hook_id": "h1",
            "_pairing_tool_call_id": "c1",
            "make_tool_result_message": {"role": "tool"},
            "interrupt_skip_wording": "Turn interrupted", "emit_post": None})
    add("_append_skipped_tool_results", tool_calls=[TC],
        effective_task_id="t1", content="cancelled {name}",
        hook_error_type="user_interrupt",
        state=skip_state,
        io={"_tc_name": "read_file", "_pairing_tool_call_id": seq("c1", "c1"),
            "make_tool_result_message": {"role": "tool"},
            "interrupt_skip_wording": "User interrupt", "emit_post": None})
    add("_append_skipped_tool_results", tool_calls=[TC, TC2],
        effective_task_id="t1", content="x {name}", flush_stage="cancelled tool result",
        state=skip_state,
        io={"_tc_name": seq("read_file", "patch"),
            "_pairing_tool_call_id": seq("c1", "c2"),
            "make_tool_result_message": {"role": "tool"},
            "_flush_session_db_after_tool_progress": True})
    add("_append_skipped_tool_results", tool_calls=[TC, TC2],
        effective_task_id="t1", content="x {name}", flush_stage="stage",
        state=skip_state,
        io={"_tc_name": seq("read_file", "patch"),
            "_pairing_tool_call_id": seq("c1", "c2"),
            "make_tool_result_message": {"role": "tool"},
            "_flush_session_db_after_tool_progress": seq(True, False)})
    add("_append_skipped_tool_results", tool_calls=[TC, TC2],
        effective_task_id="t1", content="x {name}", flush_stage="stage",
        stop_on_flush_failure=False, state=skip_state,
        io={"_tc_name": seq("read_file", "patch"),
            "_pairing_tool_call_id": seq("c1", "c2"),
            "make_tool_result_message": {"role": "tool"},
            "_flush_session_db_after_tool_progress": seq(False, False)})
    add("_append_skipped_tool_results", tool_calls=[TC],
        effective_task_id="t1", content="x {name}", state=skip_state, io={})
    add("_append_skipped_tool_results", tool_calls=[TC],
        effective_task_id="t1", content="x {name}", state=skip_state,
        io={"_tc_name": "read_file", "_pairing_tool_call_id": "c1",
            "make_tool_result_message": rais("SystemExit", "1")})

    # ── _commit_tool_result ──
    commit_state = {"agent": AGENT_COMMIT, "messages": []}
    add("_commit_tool_result", ref=ref(), function_result="raw-result",
        budget=BUDGET, tool_duration=1.25, is_error=False, blocked=False,
        effect_disposition=None, state=commit_state, io=commit_io())
    add("_commit_tool_result", ref=ref(), function_result="err-body",
        budget=BUDGET, tool_duration=0.5, is_error=True, blocked=False,
        effect_disposition=None, observed=True, error_preview=cb("error_preview"),
        state={"agent": {"__attr__": dict(AGENT_COMMIT["__attr__"], verbose_logging=False)},
               "messages": []},
        io=commit_io(observe_onboarding_result=None,
                     _append_guardrail_observation="guarded-err",
                     error_preview="err-preview",
                     _record_file_mutation_result=None))
    add("_commit_tool_result", ref=ref(), function_result="ok-body",
        budget=BUDGET, tool_duration=2.0, is_error=False, blocked=False,
        effect_disposition=None, observed=True, success_log_chars=7,
        state=commit_state,
        io=commit_io(_append_guardrail_observation="guarded-ok",
                     _record_file_mutation_result=None))
    add("_commit_tool_result", ref=ref(), function_result="blocked-body",
        budget=BUDGET, tool_duration=0.0, is_error=False, blocked=True,
        effect_disposition="none", observed=True, state=commit_state,
        io=commit_io(observe_onboarding_result=None))
    add("_commit_tool_result", ref=ref(), function_result="raw",
        budget=BUDGET, tool_duration=0.1, is_error=False, blocked=False,
        effect_disposition=None, state=commit_state,
        io=commit_io(_flush_session_db_after_tool_progress=False))
    add("_commit_tool_result", ref=ref(),
        function_result={"text_summary": "sum", "content": [{"type": "text", "text": "big"}]},
        budget=BUDGET, tool_duration=0.2, is_error=False, blocked=False,
        effect_disposition=None, state=commit_state,
        io=commit_io(_is_multimodal_tool_result=True,
                     _persist_multimodal_text_parts={"text_summary": "sum-bounded",
                                                     "content": [{"type": "text", "text": "ref"}]},
                     **{"subdirectory_hints.check_tool_call": "hint",
                        "_append_subdir_hint_to_multimodal": None}))
    add("_commit_tool_result", ref=ref(), function_result="raw",
        budget=BUDGET, tool_duration=0.3, is_error=False, blocked=False,
        effect_disposition=None, state=commit_state,
        io=commit_io(**{"subdirectory_hints.check_tool_call": "\n[subdir hint]"}))
    add("_commit_tool_result", ref=ref(), function_result="raw",
        budget=BUDGET, tool_duration=0.4, is_error=False, blocked=False,
        effect_disposition=None,
        state={"agent": {"__attr__": dict(AGENT_COMMIT["__attr__"],
                                          tool_result_metadata_callback=cb("prepare_metadata"))},
               "messages": []},
        io=commit_io(_redact_tool_args_for_display={"command": "***"},
                     prepare_metadata={"display": True}))
    add("_commit_tool_result", ref=ref(), function_result="raw",
        budget=BUDGET, tool_duration=0.4, is_error=False, blocked=False,
        effect_disposition=None,
        state={"agent": {"__attr__": dict(AGENT_COMMIT["__attr__"],
                                          tool_result_metadata_callback=cb("prepare_metadata"))},
               "messages": []},
        io=commit_io(_redact_tool_args_for_display=None,
                     prepare_metadata=rais("ValueError", "cb boom")))
    add("_commit_tool_result", ref=ref(), function_result="raw",
        budget=BUDGET, tool_duration=0.5, is_error=True, blocked=False,
        effect_disposition=None, observed=True,
        state={"agent": {"__attr__": dict(AGENT_COMMIT["__attr__"], verbose_logging=True)},
               "messages": []},
        io=commit_io(_append_guardrail_observation="g",
                     _record_file_mutation_result=rais("RuntimeError", "verifier boom"),
                     error_preview=None, verbose_text=cb("verbose_text")))
    add("_commit_tool_result", ref=ref(), function_result="raw",
        budget=BUDGET, tool_duration=0.5, is_error=False, blocked=False,
        effect_disposition=None, state=commit_state,
        io=commit_io(observe_tool_outcome=rais("KeyboardInterrupt")))
    add("_commit_tool_result", ref=ref(), function_result="raw",
        budget=BUDGET, tool_duration=0.5, is_error=False, blocked=False,
        effect_disposition=None, state=commit_state, io={})
    add("_commit_tool_result", ref=ref(), function_result="x" * 1000000,
        budget=BUDGET, tool_duration=0.5, is_error=False, blocked=False,
        effect_disposition=None, state=commit_state,
        io=commit_io(maybe_persist_tool_result="x" * 1000000))

    # ── _finalize_tool_batch ──
    fin_msgs = [{"role": "tool", "content": "a"}, {"role": "tool", "content": "b"},
                {"role": "tool", "content": "c"}]
    fin_state = {"agent": {"__attr__": {
        "_apply_pending_steer_to_tool_results": cb("_apply_pending_steer_to_tool_results")}},
        "messages": fin_msgs}
    add("_finalize_tool_batch", effective_task_id="t1", num_tools=0,
        budget=BUDGET, state=fin_state, io={})
    add("_finalize_tool_batch", effective_task_id="t1", num_tools=2,
        budget=BUDGET, state=fin_state,
        io={"get_active_env": {"__attr__": {"cwd": "/w"}},
            "enforce_turn_budget": None,
            "plugin_live_notes.keep_after_turn_budget": None,
            "sync_rewritten_tool_rows": None, "record_tool_batch": None,
            "_apply_pending_steer_to_tool_results": None})
    add("_finalize_tool_batch", effective_task_id="t1", num_tools=9,
        budget=BUDGET, state=fin_state,
        io={"get_active_env": None, "enforce_turn_budget": None,
            "plugin_live_notes.keep_after_turn_budget": None,
            "sync_rewritten_tool_rows": None, "record_tool_batch": None,
            "_apply_pending_steer_to_tool_results": None})
    add("_finalize_tool_batch", effective_task_id="t1", num_tools=2,
        budget=BUDGET,
        state={"agent": fin_state["agent"],
               "messages": [{"role": "tool"}, {"role": "tool", "content": None}]},
        io={"get_active_env": None, "enforce_turn_budget": None,
            "plugin_live_notes.keep_after_turn_budget": None,
            "sync_rewritten_tool_rows": None, "record_tool_batch": None,
            "_apply_pending_steer_to_tool_results": None})
    add("_finalize_tool_batch", effective_task_id="t1", num_tools=1,
        budget=BUDGET, state=fin_state,
        io={"get_active_env": None, "enforce_turn_budget": rais("RuntimeError", "budget")})
    add("_finalize_tool_batch", effective_task_id="t1", num_tools=1,
        budget=BUDGET, state=fin_state, io={})

    # ── _print_tool_completed ──
    add("_print_tool_completed", index=1, tool_duration=1.5, result="full result",
        state={"agent": {"__attr__": {"verbose_logging": True,
                                      "_wrap_verbose": cb("_wrap_verbose")}}},
        io={"print": None, "_wrap_verbose": "Result: full result"})
    add("_print_tool_completed", index=2, tool_duration=0.25, result="a long result body",
        state={"agent": {"__attr__": {"verbose_logging": False, "log_prefix_chars": 8}}},
        io={"_preview": "a long r", "print": None})
    add("_print_tool_completed", index=3, tool_duration=2.0, result=None,
        state={"agent": {"__attr__": {"verbose_logging": False, "log_prefix_chars": 20}}},
        io={"_preview": "None", "print": None})
    add("_print_tool_completed", index=4, tool_duration=3.0, result=42,
        state={"agent": {"__attr__": {"verbose_logging": False, "log_prefix_chars": 20}}},
        io={"_preview": "42", "print": None})
    add("_print_tool_completed", index=5, tool_duration=1.0, result="x",
        state={"agent": {"__attr__": {}}}, io={})
    add("_print_tool_completed", index=6, tool_duration=1.0, result="x",
        state={"agent": {"__attr__": {"verbose_logging": False, "log_prefix_chars": 20}}},
        io={})

    # ── _append_batch_results ──
    def batch_d(results, pcs=None, toi=None, timeout_s=30.0):
        return {"__attr__": {
            "parsed_calls": [pc("read_file"), pc("patch")] if pcs is None else pcs,
            "results": results, "timed_out_indices": [] if toi is None else toi,
            "timeout_s": timeout_s}}

    def worker_r(result="ok", blocked=False, data_use=None, parse_error=None):
        return {"__attr__": {"ref": ref(), "result": result, "duration": 0.5,
                             "is_error": False, "blocked": blocked,
                             "data_use": [] if data_use is None else data_use}}

    ab_state = {"agent": AGENT_QUIET, "messages": []}
    ab_io = {
        "_commit_tool_result": ["persisted", "display", {"level": "low"}],
        "_should_emit_quiet_tool_messages": False,
        "_tool_progress_enabled": True,
        "_multimodal_text_summary": "display",
        "_print_tool_completed": None,
        "_emit_tool_complete_and_risk": None,
    }
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([worker_r(), worker_r("ok2")]), state=ab_state, io=dict(ab_io))
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([None, worker_r()], toi=[0]), state=ab_state,
        io=dict(ab_io, parsed_call_ref=ref("read_file"),
                _unfinished_tool_result=["timeout-body", 30.0, "unknown"]))
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([worker_r()], pcs=[pc("read_file", parse_error="bad json")]),
        state=ab_state, io=dict(ab_io, emit_invalid_arguments=None))
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([worker_r(), worker_r()]), state=ab_state,
        io=dict(ab_io, _commit_tool_result=seq(["p", "d", None], None)))
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([worker_r()]), state=ab_state,
        io=dict(ab_io, _should_emit_quiet_tool_messages=True,
                _get_cute_tool_message_impl="cute line", _safe_print=None))
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([worker_r()]), state=ab_state,
        io=dict(ab_io, _tool_progress_enabled=False))
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([worker_r()]), state=ab_state,
        io=dict(ab_io, _commit_tool_result=rais("SystemExit", "1")))
    add("_append_batch_results", effective_task_id="t1", budget=BUDGET,
        batch=batch_d([worker_r()]), state=ab_state, io={})

    # ── _start_quiet_tool_spinner ──
    spin_io = {
        "_should_emit_quiet_tool_messages": True,
        "_should_start_quiet_spinner": True,
        "kawaii.get_waiting_faces": ["(o.o)", "(^.^)"],
        "random.choice": "(o.o)",
        "_redact_tool_args_for_display": {"command": "***"},
        "_tool_row_emoji": "[T]",
        "_build_tool_label": "terminal ls",
        "kawaii.ctor": None,
        "spinner.start": None,
    }
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={"command": "ls"}, state={"agent": AGENT_QUIET}, io=dict(spin_io))
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={}, state={"agent": AGENT_QUIET},
        io=dict(spin_io, _should_emit_quiet_tool_messages=False))
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={}, state={"agent": AGENT_QUIET},
        io=dict(spin_io, _should_start_quiet_spinner=False))
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={}, gate=False, state={"agent": AGENT_QUIET}, io=dict(spin_io))
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={}, label="custom label", state={"agent": AGENT_QUIET},
        io=dict(spin_io))
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={"command": "ls"}, state={"agent": AGENT_QUIET},
        io=dict(spin_io, _redact_tool_args_for_display=None, _build_tool_label=None))
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={}, state={"agent": AGENT_QUIET},
        io=dict(spin_io, **{"spinner.start": rais("RuntimeError", "no spin")}))
    add("_start_quiet_tool_spinner", function_name="terminal",
        function_args={}, state={"agent": AGENT_QUIET}, io={})

    # ── _finish_quiet_tool_spinner ──
    add("_finish_quiet_tool_spinner", spinner=SPINNER, function_name="terminal",
        function_args={}, tool_duration=0.5, result="ok",
        state={"agent": AGENT_QUIET},
        io={"_get_cute_tool_message_impl": "done!", "spinner.stop": None})
    add("_finish_quiet_tool_spinner", spinner=None, function_name="terminal",
        function_args={}, tool_duration=0.5, result="ok",
        state={"agent": AGENT_QUIET},
        io={"_should_emit_quiet_tool_messages": True,
            "_get_cute_tool_message_impl": "done!", "_vprint": None})
    add("_finish_quiet_tool_spinner", spinner=None, function_name="terminal",
        function_args={}, tool_duration=0.5, result="ok",
        state={"agent": AGENT_QUIET},
        io={"_should_emit_quiet_tool_messages": False})
    add("_finish_quiet_tool_spinner", spinner=SPINNER, function_name="terminal",
        function_args={}, tool_duration=0.5, result="ok",
        state={"agent": AGENT_QUIET},
        io={"_get_cute_tool_message_impl": "done!",
            "spinner.stop": rais("ValueError", "stop boom")})
    add("_finish_quiet_tool_spinner", spinner=None, function_name="terminal",
        function_args={}, tool_duration=0.5, result="ok",
        state={"agent": AGENT_QUIET}, io={})

    # ── _resolve_sequential_dispatch ──
    rd_state = {"agent": {"__attr__": {"quiet_mode": False}}, "messages": []}
    add("_resolve_sequential_dispatch", ref=ref("read_file"),
        inline_executors={"read_file": cb("inline_exec")}, state=rd_state,
        io={"inline_tool_context.ctor": None})
    add("_resolve_sequential_dispatch", ref=ref("delegate_task", args={"goal": "g"}),
        inline_executors={"delegate_task": cb("inline_exec")},
        state={"agent": {"__attr__": {}}, "messages": []},
        io={"_delegate_spinner_label": "delegating",
            "_start_quiet_tool_spinner": SPINNER,
            "import_arg_validation": None, "import_registry": None})
    add("_resolve_sequential_dispatch", ref=ref("context_engine_tool"),
        state=rd_state,
        io={"is_context_engine_tool": True, "_start_quiet_tool_spinner": None})
    add("_resolve_sequential_dispatch", ref=ref("hindsight_retain"),
        state=rd_state,
        io={"is_context_engine_tool": False, "is_memory_provider_tool": True,
            "_start_quiet_tool_spinner": SPINNER})
    add("_resolve_sequential_dispatch", ref=ref("terminal", trace=[{"m": 1}]),
        state={"agent": {"__attr__": {"quiet_mode": True}}, "messages": []},
        io={"is_context_engine_tool": False, "is_memory_provider_tool": False,
            "_start_quiet_tool_spinner": SPINNER})
    add("_resolve_sequential_dispatch", ref=ref("terminal"),
        state=rd_state,
        io={"is_context_engine_tool": False, "is_memory_provider_tool": False})
    add("_resolve_sequential_dispatch", ref=ref("read_file"),
        inline_executors={}, state=rd_state, io={})

    # ── _skip_remaining_sequential ──
    sr_state = {"agent": {"__attr__": {"log_prefix": "[s] ", "_vprint": cb("_vprint")}},
                "messages": []}
    add("_skip_remaining_sequential", remaining=[TC, TC2], effective_task_id="t1",
        notice="tool call(s)",
        skip_kwargs={"content": "cancelled {name}", "hook_error_type": "user_interrupt",
                     "flush_stage": "cancelled tool result"},
        state=sr_state, io={"_vprint": None, "_append_skipped_tool_results": True})
    add("_skip_remaining_sequential", remaining=[TC], effective_task_id="t1",
        notice="remaining tool call(s)",
        skip_kwargs={"content": "skipped {name}", "flush_stage": "skipped tool result"},
        state=sr_state, io={"_vprint": None, "_append_skipped_tool_results": False})
    add("_skip_remaining_sequential", remaining=[TC], effective_task_id="t1",
        notice="tool call(s)",
        skip_kwargs={"content": "c", "hook_id": cb("hook_id")},
        state=sr_state, io={"_vprint": None, "_append_skipped_tool_results": True})
    add("_skip_remaining_sequential", remaining=[TC], effective_task_id="t1",
        notice="tool call(s)", skip_kwargs={"content": "c"},
        state={"agent": {"__attr__": {}}, "messages": []}, io={})
    add("_skip_remaining_sequential", remaining=[], effective_task_id="t1",
        notice="tool call(s)", skip_kwargs={}, state=sr_state, io={})

    # ── _append_invalid_arguments_result ──
    ia_state = {"agent": {"__attr__": {}}, "messages": []}
    add("_append_invalid_arguments_result", ref=ref(), parse_error="bad json",
        state=ia_state,
        io={"emit_invalid_arguments": None,
            "make_tool_result_message": {"role": "tool", "content": "bad json"},
            "_flush_session_db_after_tool_progress": True})
    add("_append_invalid_arguments_result", ref=ref(), parse_error="bad json",
        state=ia_state,
        io={"emit_invalid_arguments": None, "make_tool_result_message": {"role": "tool"},
            "_flush_session_db_after_tool_progress": False})
    add("_append_invalid_arguments_result", ref=ref(), parse_error="bad json",
        state=ia_state,
        io={"emit_invalid_arguments": None, "make_tool_result_message": {"role": "tool"},
            "_flush_session_db_after_tool_progress": rais("RuntimeError", "flush boom")})
    add("_append_invalid_arguments_result", ref=None, parse_error="bad json",
        state=ia_state, io={})
    add("_append_invalid_arguments_result", ref=ref(), parse_error="bad json",
        state=ia_state, io={})

    # ── _run_sequential_call ──
    rc_state = {"agent": {"__attr__": {"interrupt": cb("interrupt")}},
                "ref": ref(), "messages": []}
    rc_io = {
        "middleware_kwargs": {"function_name": "terminal", "function_args": {"command": "ls"},
                              "effective_task_id": "t1", "tool_call_id": "c1",
                              "middleware_trace": []},
        "_run_sequential_tool_execution_middleware": managed("ok"),
        "time.time": 10.5,
        "_finish_quiet_tool_spinner": None,
    }
    add("_run_sequential_call", dispatch=dispatch_d(spinner=SPINNER),
        scope_block=None, remaining_calls=[TC2], display_index=1,
        tool_start_time=10.0, state=rc_state, io=dict(rc_io))
    add("_run_sequential_call",
        dispatch=dispatch_d(spinner=SPINNER, finish_in_finally=False),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0, state=rc_state, io=dict(rc_io))
    add("_run_sequential_call", dispatch=dispatch_d(finish_spinner=False),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0, state=rc_state, io=dict(rc_io))
    add("_run_sequential_call",
        dispatch=dispatch_d(handles_ki=True, error_result=cb("dispatch_error_result")),
        scope_block=None, remaining_calls=[TC2], display_index=1,
        tool_start_time=10.0, state=rc_state,
        io=dict(rc_io, _run_sequential_tool_execution_middleware=rais("KeyboardInterrupt"),
                interrupt=None, emit_cancelled="cancelled-json",
                interrupt_skip_wording="Turn interrupted",
                _append_skipped_tool_results=True))
    add("_run_sequential_call", dispatch=dispatch_d(handles_ki=False),
        scope_block=None, remaining_calls=[TC2], display_index=1,
        tool_start_time=10.0, state=rc_state,
        io=dict(rc_io, _run_sequential_tool_execution_middleware=rais("KeyboardInterrupt")))
    add("_run_sequential_call",
        dispatch=dispatch_d(error_result=cb("dispatch_error_result"),
                            error_log="handle_function_call raised for %s: %s"),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0, state=rc_state,
        io=dict(rc_io, _run_sequential_tool_execution_middleware=rais("ValueError", "bad arg"),
                dispatch_error_result="error-body", **{"logger.error": None}))
    add("_run_sequential_call", dispatch=dispatch_d(error_result=None),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0, state=rc_state,
        io=dict(rc_io, _run_sequential_tool_execution_middleware=rais("ValueError", "bad arg")))
    add("_run_sequential_call", dispatch=dispatch_d(),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0, state=rc_state,
        io=dict(rc_io, _run_sequential_tool_execution_middleware=rais("SystemExit", "0")))
    add("_run_sequential_call", dispatch=dispatch_d(is_delegate=True),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0,
        state={"agent": {"__attr__": {"_delegate_spinner": SPINNER}},
               "ref": ref(), "messages": []},
        io=dict(rc_io))
    add("_run_sequential_call",
        dispatch=dispatch_d(error_result=cb("dispatch_error_result")),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0, state=rc_state,
        io={"dispatch_error_result": "fallback", "logger.error": None,
            "time.time": 11.0, "_finish_quiet_tool_spinner": None})
    add("_run_sequential_call", dispatch=dispatch_d(),
        scope_block=None, remaining_calls=[], display_index=1,
        tool_start_time=10.0, state=rc_state,
        io=dict(rc_io, **{"time.time": rais("RuntimeError", "clock boom")}))

    # ── _publish_sequential_result ──
    ps_state = {"agent": {"__attr__": {}}, "ref": ref(), "messages": []}
    ps_io = {
        "emit_post": None,
        "apply_transform_tool_result": "transformed",
        "_detect_tool_failure": [False, None],
        "_commit_tool_result": ["persisted", "display", {"level": "low"}],
        "import_mark_batch_outcome": None,
        "mark_batch_outcome": None,
        "_emit_tool_complete_and_risk": None,
        "_tool_progress_enabled": True,
        "_print_tool_completed": None,
    }
    add("_publish_sequential_result", managed=managed("raw"), tool_duration=0.5,
        index=1, budget=BUDGET, transform_applied=False, state=ps_state, io=dict(ps_io))
    add("_publish_sequential_result", managed=managed("raw"), tool_duration=0.5,
        index=1, budget=BUDGET, transform_applied=True, state=ps_state, io=dict(ps_io))
    add("_publish_sequential_result", managed=managed("raw", blocked=True),
        tool_duration=0.5, index=1, budget=BUDGET, transform_applied=False,
        state=ps_state, io=dict(ps_io))
    add("_publish_sequential_result",
        managed=managed({"_type": "_ToolTimeoutResult", "value": "timed out"}),
        tool_duration=30.0, index=1, budget=BUDGET, transform_applied=False,
        state=ps_state, io=dict(ps_io))
    add("_publish_sequential_result", managed=managed("raw"), tool_duration=0.5,
        index=1, budget=BUDGET, transform_applied=False, state=ps_state,
        io=dict(ps_io, _commit_tool_result=None))
    add("_publish_sequential_result", managed=managed("raw"), tool_duration=0.5,
        index=1, budget=BUDGET, transform_applied=False, state=ps_state,
        io=dict(ps_io, _tool_progress_enabled=False))
    add("_publish_sequential_result", managed=managed("raw"), tool_duration=0.5,
        index=1, budget=BUDGET, transform_applied=False, state=ps_state,
        io=dict(ps_io, emit_post=rais("GeneratorExit")))
    add("_publish_sequential_result", managed=managed("raw"), tool_duration=0.5,
        index=1, budget=BUDGET, transform_applied=False, state=ps_state, io={})

    # ── _execute_tool_calls_sequential ──
    def es_io(**over):
        io = {
            "_budget_for_agent": BUDGET,
            "_parse_tool_call": pc(),
            "parsed_call_ref": ref(),
            "time.time": seq(10.0, 20.0),
            "_resolve_sequential_dispatch": dispatch_d(transform_applied=True),
            "_run_sequential_call": [managed("ok"), 0.5],
            "_publish_sequential_result": True,
            "_finalize_tool_batch": None,
        }
        io.update(over)
        return io

    es_state = {"agent": {"__attr__": {"_interrupt_requested": False}}, "messages": []}
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC, TC2]}},
        effective_task_id="t1", finalize=True, state=es_state, io=es_io())
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC, TC2]}},
        effective_task_id="t1", finalize=True,
        state={"agent": {"__attr__": {"_interrupt_requested": True}}, "messages": []},
        io=es_io(interrupt_skip_wording="Turn interrupted",
                 _skip_remaining_sequential=True))
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC, TC2]}},
        effective_task_id="t1", finalize=True, state=es_state,
        io=es_io(_parse_tool_call=seq(pc(parse_error="bad json"), pc()),
                 _append_invalid_arguments_result=True))
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC, TC2]}},
        effective_task_id="t1", finalize=True, state=es_state,
        io=es_io(_publish_sequential_result=False))
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC, TC2]}},
        effective_task_id="t1", finalize=True,
        state={"agent": {"__attr__": {"_interrupt_requested": seq(False, True)}},
               "messages": []},
        io=es_io(interrupt_skip_wording="Turn interrupted",
                 _skip_remaining_sequential=True))
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC]}},
        effective_task_id="t1", finalize=True,
        state={"agent": {"__attr__": {"_interrupt_requested": False,
                                      "_incremental_persistence_failed": True}},
               "messages": []},
        io=es_io())
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC]}},
        effective_task_id="t1", finalize=False, state=es_state, io=es_io())
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC]}},
        effective_task_id="t1", finalize=True, state=es_state,
        io=es_io(_run_sequential_call=rais("KeyboardInterrupt")))
    add("_execute_tool_calls_sequential",
        assistant_message={"__attr__": {"tool_calls": [TC]}},
        effective_task_id="t1", finalize=True, state=es_state, io={})

    # ── middleware_kwargs ──
    add("middleware_kwargs", ref=ref())
    add("middleware_kwargs", ref=ref("read_file", args={}, trace=[{"m": 1}]))
    add("middleware_kwargs", ref=ref("patch", args={"path": "x"}, task_id="t9",
                                     call_id="c9", trace=[]))
    add("middleware_kwargs", ref=ref("", args=None))
    add("middleware_kwargs", ref=ref("terminal", args={"a": [1, 2]}, trace=["t"]))

    # ── emit_post ──
    add("emit_post", ref=ref(trace=[{"m": 1}]), result="ok",
        agent={"__attr__": {}}, io={"_emit_terminal_post_tool_call": None})
    add("emit_post", ref=ref(trace=[{"m": 1}]), result="ok", trace=[],
        agent={"__attr__": {}}, io={"_emit_terminal_post_tool_call": None})
    add("emit_post", ref=ref(), result="err",
        outcome={"status": "error", "error_type": "tool_error",
                 "error_message": "boom", "duration_ms": 5},
        agent={"__attr__": {}}, io={"_emit_terminal_post_tool_call": None})
    add("emit_post", ref=ref(), result=None, agent={"__attr__": {}},
        io={"_emit_terminal_post_tool_call": None})
    add("emit_post", ref=ref(), result="ok", agent={"__attr__": {}},
        io={"_emit_terminal_post_tool_call": rais("RuntimeError", "hook boom")})
    add("emit_post", ref=ref(), result="ok", agent={"__attr__": {}}, io={})

    # ── emit_cancelled ──
    add("emit_cancelled", ref=ref(), agent={"__attr__": {}}, start_time=10.0,
        io={"interrupt_skip_wording": "Turn interrupted", "time.time": 10.25,
            "emit_post": None})
    add("emit_cancelled", ref=ref(), agent={"__attr__": {}}, start_time=10.0,
        io={"interrupt_skip_wording": "Turn aborted — system stop",
            "time.time": 11.0, "emit_post": None})
    add("emit_cancelled", ref=ref(), agent={"__attr__": {}}, start_time=10.0,
        io={"interrupt_skip_wording": "Turn interrupted",
            "time.time": rais("RuntimeError", "clock")})
    add("emit_cancelled", ref=ref(), agent={"__attr__": {}}, start_time=10.0,
        io={"interrupt_skip_wording": "Turn interrupted", "time.time": 10.5,
            "emit_post": rais("KeyboardInterrupt")})
    add("emit_cancelled", ref=ref(), agent={"__attr__": {}}, start_time=10.5, io={})

    # ── emit_invalid_arguments ──
    add("emit_invalid_arguments", ref=ref(), result="bad json",
        agent={"__attr__": {}}, io={"emit_post": None})
    add("emit_invalid_arguments", ref=ref(), result="", agent={"__attr__": {}},
        io={"emit_post": None})
    add("emit_invalid_arguments", ref=ref(), result="bad json",
        agent={"__attr__": {}}, io={"emit_post": rais("ValueError", "hook")})
    add("emit_invalid_arguments", ref=ref(), result=None, agent={"__attr__": {}},
        io={"emit_post": None})
    add("emit_invalid_arguments", ref=ref(), result="bad json",
        agent={"__attr__": {}}, io={})

    # ── ref (_ParsedCall.ref) ──
    add("ref", pc=pc(), task_id="t1", io={"_pairing_tool_call_id": "c1"})
    add("ref", pc=pc("read_file"), task_id="t2", io={"_pairing_tool_call_id": "pair-2"})
    add("ref", pc=pc(tool_call={"__attr__": {"id": "raw-id"}}), task_id="t1",
        io={"_pairing_tool_call_id": "coalesced"})
    add("ref", pc=pc(), task_id="t1",
        io={"_pairing_tool_call_id": rais("RuntimeError", "pair boom")})
    add("ref", pc=pc(), task_id="t1", io={})

    # ── _set_worker_activity_callback ──
    sac_state = {"agent": {"__attr__": {"_touch_activity": cb("_touch_activity")}}}
    add("_set_worker_activity_callback", state=sac_state,
        io={"import_set_activity_callback": None, "set_activity_callback": None})
    add("_set_worker_activity_callback", state=sac_state,
        io={"import_set_activity_callback": rais("ValueError", "no env")})
    add("_set_worker_activity_callback", state=sac_state,
        io={"import_set_activity_callback": rais("SystemExit", "0")})
    add("_set_worker_activity_callback", state={"agent": {"__attr__": {}}},
        io={"import_set_activity_callback": None})
    add("_set_worker_activity_callback", state=sac_state,
        io={"import_set_activity_callback": None,
            "set_activity_callback": rais("RuntimeError", "bind boom")})
    add("_set_worker_activity_callback", state=sac_state, io={})

    # ── _run_tool_activity_heartbeat ──
    hb_state = {"agent": {"__attr__": {"_touch_activity": cb("_touch_activity")}}}
    add("_run_tool_activity_heartbeat", stop_event=EVENT, label="tool running: x",
        interval=30.0, worker_tid=7, state=hb_state,
        io={"import_is_thread_interrupted": None,
            "event.wait": seq(False, False, True),
            "is_thread_interrupted": False, "_touch_activity": None})
    add("_run_tool_activity_heartbeat", stop_event=EVENT, label="hb",
        worker_tid=7, state=hb_state,
        io={"import_is_thread_interrupted": None, "event.wait": seq(False, True),
            "is_thread_interrupted": seq(False, True)})
    add("_run_tool_activity_heartbeat", stop_event=EVENT, label="hb",
        state=hb_state,
        io={"import_is_thread_interrupted": None,
            "event.wait": rais("ValueError", "wait boom")})
    add("_run_tool_activity_heartbeat", stop_event=EVENT, label="hb",
        state=hb_state,
        io={"import_is_thread_interrupted": None,
            "event.wait": rais("KeyboardInterrupt")})
    add("_run_tool_activity_heartbeat", stop_event=EVENT, label="hb",
        state=hb_state,
        io={"import_is_thread_interrupted": rais("RuntimeError", "no module")})
    add("_run_tool_activity_heartbeat", stop_event=EVENT, label="hb",
        state=hb_state,
        io={"import_is_thread_interrupted": None, "event.wait": seq(False, True),
            "is_thread_interrupted": False,
            "_touch_activity": rais("ValueError", "touch boom")})
    add("_run_tool_activity_heartbeat", stop_event=EVENT, label="hb",
        interval=0.5, worker_tid=None, state=hb_state,
        io={"import_is_thread_interrupted": None, "event.wait": seq(True)})

    # ── _registered_tool_worker ──
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None, "lock.__exit__": None, "body": None,
            "_ra": None, "_set_interrupt": None})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None, "lock.__exit__": None,
            "body": rais("ValueError", "body boom"),
            "_ra": None, "_set_interrupt": None})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None, "lock.__exit__": None,
            "body": rais("KeyboardInterrupt"),
            "_ra": None, "_set_interrupt": None})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None, "lock.__exit__": None,
            "body": rais("SystemExit", "0"),
            "_ra": None, "_set_interrupt": None})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None, "lock.__exit__": None, "body": None,
            "_ra": None, "_set_interrupt": rais("ValueError", "clear boom")})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None, "lock.__exit__": None,
            "body": rais("ValueError", "body boom"),
            "_ra": None, "_set_interrupt": rais("KeyboardInterrupt")})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None,
            "lock.__exit__": seq(None, rais("RuntimeError", "exit boom")),
            "body": None, "_ra": None, "_set_interrupt": None})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER},
        io={"current_thread_ident": 7, "lock.__enter__": None,
            "worker_threads.add": None, "lock.__exit__": None, "body": None,
            "worker_threads.discard": rais("ValueError", "discard boom"),
            "_ra": None, "_set_interrupt": None})
    add("_registered_tool_worker", state={"agent": AGENT_WORKER}, io={})

    # ── _run_with_activity_heartbeat ──
    rwh_state = {"agent": {"__attr__": {}}}
    rwh_io = {
        "threading.Event": None, "current_thread_ident": 7,
        "threading.Thread": None, "thread.start": None,
        "fn": "tool-result", "event.set": None, "thread.join": None,
    }
    add("_run_with_activity_heartbeat", function_name="terminal", fn=cb("fn"),
        state=rwh_state, io=dict(rwh_io))
    add("_run_with_activity_heartbeat", function_name="terminal", fn=cb("fn"),
        state=rwh_state, io=dict(rwh_io, fn=rais("ValueError", "tool boom")))
    add("_run_with_activity_heartbeat", function_name="terminal", fn=cb("fn"),
        state=rwh_state, io=dict(rwh_io, fn=rais("KeyboardInterrupt")))
    add("_run_with_activity_heartbeat", function_name="terminal", fn=cb("fn"),
        state=rwh_state, io=dict(rwh_io, **{"thread.join": rais("RuntimeError", "join boom")}))
    add("_run_with_activity_heartbeat", function_name="terminal", fn=cb("fn"),
        state=rwh_state, io=dict(rwh_io, **{"event.set": rais("RuntimeError", "set boom")}))
    add("_run_with_activity_heartbeat",
        function_name="a_very_long_tool_name_over_24_chars", fn=cb("fn"),
        state=rwh_state, io=dict(rwh_io))
    add("_run_with_activity_heartbeat", function_name="terminal", fn=None,
        state=rwh_state, io=dict(rwh_io))

    return out


def main():
    cases = build()
    counts = Counter(c["func"] for c in cases)
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        raise SystemExit("functions with <5 cases: %s" % thin)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in cases:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases (%d functions) -> %s" % (len(cases), len(counts), OUT))
    for fn, n in sorted(counts.items()):
        print("  %-36s %d" % (fn, n))


if __name__ == "__main__":
    main()