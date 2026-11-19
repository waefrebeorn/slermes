#!/usr/bin/env python3
"""Generate tests/fixtures/tui_dispatch_flow.jsonl for the cli_tui_mixin.py
TUI-dispatch FLOW port (src/cli/port_tui_dispatch_flow.c).

Each case carries the FULL state schema (defaults merged with overrides) so
the C harness and the oracle build identical mirrors without shared default
tables. Scripted seam results live in in.io; "import:<module>" keys must be
single entries (the live import statement checks once per attribute fetch).
Run: python tests/gen_tui_dispatch_flow_cases.py
"""
import copy
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "tui_dispatch_flow.jsonl"

ACCENT = "\x1b[1;38;2;255;215;0m"

DEFAULT_STATE = {
    "buf": {"text": "", "cursor_position": 0},
    "app_is_running": True,
    "agent": None,
    "_agent_running": False,
    "_should_exit": False,
    "_detach_on_exit": False,
    "_last_ctrl_c_time": 0,
    "_attached_images": [],
    "_pending_input": [],
    "_interrupt_queue": [],
    "_clarify_state": None,
    "_clarify_freetext": False,
    "_clarify_prefill": "",
    "_clarify_multi_base": None,
    "_sudo_state": None,
    "_secret_state": None,
    "_approval_state": None,
    "_slash_confirm_state": None,
    "_connection_state": None,
    "_model_picker_state": None,
    "_command_palette_state": None,
    "_gateway_command_wait": None,
    "busy_input_mode": "queue",
    "config": {},
    "_tui_multiline_shortcuts": False,
    "CLI_CONFIG": {},
    "_hermes_home": "/home/u/.hermes",
    "_accent": ACCENT,
}


def R(v):
    return {"ret": v}


def RAISE(t, m):
    return {"raise": {"type": t, "message": m}}


def IMP(t, m):
    return {"import_raise": {"type": t, "message": m}}


def EFF(ret, **state):
    return {"ret": ret, "state": state}


def case(func, state=None, io=None, **extra):
    st = copy.deepcopy(DEFAULT_STATE)
    st.update(state or {})
    inp = {"state": st, "io": io or {}}
    inp.update(extra)
    return {"func": func, "in": inp}


def buf(text, cursor=None):
    return {"buf": {"text": text,
                    "cursor_position": len(text) if cursor is None else cursor}}


CLARIFY = {"questions": [{"qid": "q1", "question": "Which color?"}],
           "choices": ["red", "blue"], "active": 0, "selected": 0,
           "answers": {}, "answer_meta": {}}
CONN = {"phase": "form", "fields": [{"name": "url"}, {"name": "token"}],
        "field_index": 0, "selected": 0, "target": {"name": "svc"},
        "drafts": {"svc": {}}}
CLOSERS = [
    {"state_attr": "_slash_confirm_state",
     "close": "self._submit_slash_confirm_response", "args": {"choice": "cancel"}},
    {"state_attr": "_model_picker_state", "close": "self._close_model_picker", "args": {}},
    {"state_attr": "_command_palette_state", "close": "self._close_command_palette", "args": {}},
    {"state_attr": "_gateway_command_wait", "close": "gateway_command_wait.set", "args": {}},
]

CASES = []

# ── _tui_handle_enter ────────────────────────────────────────────────────
CASES += [
    case("handle_enter", io={"self._tui_enter_overlay": R(True)}),
    case("handle_enter",
         state={**buf("hello \\"), "_tui_multiline_shortcuts": True},
         io={"self._tui_enter_overlay": R(False),
             "_is_backslash_line_continuation": R(True),
             "_apply_backslash_line_continuation": R("hello\n")}),
    case("handle_enter",
         state={**buf("hello \\", 3), "_tui_multiline_shortcuts": True},
         io={"self._tui_enter_overlay": R(False), "handle_burst_enter": R(False)}),
    case("handle_enter",
         io={"self._tui_enter_overlay": R(False), "handle_burst_enter": R(True)}),
    case("handle_enter",
         state=buf("a question"),
         io={"self._tui_enter_overlay": R(False), "handle_burst_enter": R(False),
             "self._tui_submit_buffer": R(None)}),
    case("handle_enter", io={"import:cli": IMP("RuntimeError", "cli import boom")}),
    case("handle_enter",
         state={**buf("x\\"), "_tui_multiline_shortcuts": False},
         io={"self._tui_enter_overlay": R(False), "handle_burst_enter": R(False)}),
]

# ── _tui_submit_buffer ───────────────────────────────────────────────────
CASES += [
    case("submit_buffer", state=buf("   ")),
    case("submit_buffer", state=buf("/model gpt"),
         io={"self._tui_enter_inline_command": R(True)}),
    case("submit_buffer", state=buf("hello"),
         io={"self._tui_enter_inline_command": R(False),
             "_looks_like_slash_command": R(False)}),
    case("submit_buffer", state={**buf("hi"), "_agent_running": True},
         io={"self._tui_enter_inline_command": R(False),
             "_looks_like_slash_command": R(False)}),
    case("submit_buffer", state={**buf("/help"), "_agent_running": True},
         io={"self._tui_enter_inline_command": R(False),
             "_looks_like_slash_command": R(True)}),
    case("submit_buffer", state={**buf("!git status"), "_agent_running": True},
         io={"self._tui_enter_inline_command": R(False),
             "_looks_like_slash_command": R(False)}),
    case("submit_buffer",
         state={**buf(""), "_attached_images": ["a.png", "b.png"]},
         io={"self._tui_enter_inline_command": R(False)}),
    case("submit_buffer", state=buf("  hello  "),
         io={"self._tui_enter_inline_command": R(False),
             "_looks_like_slash_command": R(False)}),
]

# ── _tui_enter_inline_command ────────────────────────────────────────────
CASES += [
    case("enter_inline_command", text="/model", has_images=False,
         io={"self._should_handle_model_command_inline": R(True),
             "self.process_command": R(True)}),
    case("enter_inline_command", text="/model", has_images=False,
         io={"self._should_handle_model_command_inline": R(True),
             "self.process_command": R(False)}),
    case("enter_inline_command", text="/model", has_images=False,
         state={"app_is_running": False},
         io={"self._should_handle_model_command_inline": R(True),
             "self.process_command": R(False)}),
    case("enter_inline_command", text="/steer focus", has_images=False,
         io={"self._should_handle_model_command_inline": R(False),
             "self._should_handle_steer_command_inline": R(True),
             "self.process_command": R(True)}),
    case("enter_inline_command", text="/bg task", has_images=False,
         io={"self._should_handle_model_command_inline": R(False),
             "self._should_handle_steer_command_inline": R(False),
             "self._should_handle_background_command_inline": R(True),
             "self.process_command": R(True)}),
    case("enter_inline_command", text="/btw note", has_images=False,
         io={"self._should_handle_model_command_inline": R(False),
             "self._should_handle_steer_command_inline": R(False),
             "self._should_handle_background_command_inline": R(False),
             "self._should_handle_stop_command_inline": R(True),
             "self.process_command": R(True)}),
    case("enter_inline_command", text="plain text", has_images=False,
         io={"self._should_handle_model_command_inline": R(False),
             "self._should_handle_steer_command_inline": R(False),
             "self._should_handle_background_command_inline": R(False),
             "self._should_handle_stop_command_inline": R(False)}),
    case("enter_inline_command", text="/model", has_images=True,
         io={"self._should_handle_model_command_inline": R(True),
             "self.process_command": R(True)}),
]

# ── _tui_enter_while_busy ────────────────────────────────────────────────
CASES += [
    case("enter_while_busy", text="hello", images=[], payload="hello",
         state={"busy_input_mode": "steer", "agent": {"has_steer": True}},
         io={"agent.steer": R(True), "t": R("steered!"), "is_seen": R(True)}),
    case("enter_while_busy", text="stop talking", images=[], payload="stop talking",
         state={"busy_input_mode": "steer", "agent": {"has_steer": True}},
         io={"agent.steer": R("voice_stopped")}),
    case("enter_while_busy", text="hi", images=[], payload="hi",
         state={"busy_input_mode": "steer", "agent": {"has_steer": True}},
         io={"agent.steer": R(False), "self._gateway_queue_store": R(None),
             "t": R("queued"), "is_seen": R(True)}),
    case("enter_while_busy", text="hi", images=[], payload="hi",
         state={"busy_input_mode": "steer", "agent": {"has_steer": True}},
         io={"agent.steer": RAISE("RuntimeError", "boom"),
             "t": [{"ret": "steer failed"}, {"ret": "queued next"}],
             "self._gateway_queue_store": R({"id": "q1"}),
             "self._queue_enqueue": R(None),
             "is_seen": R(False), "busy_input_hint_cli": R("tip: /busy"),
             "mark_seen": R(True)}),
    case("enter_while_busy", text="hi", images=["a.png"], payload=["hi", ["a.png"]],
         mode="steer", state={"agent": {"has_steer": True}},
         io={"t": R("queued"), "is_seen": R(True)}),
    case("enter_while_busy", text="task", images=[], payload="task", mode="queue",
         io={"self._gateway_queue_store": R({}), "self._queue_enqueue": R(None),
             "is_seen": R(True)}),
    case("enter_while_busy", text="", images=["a", "b"], payload=["", ["a", "b"]],
         mode="queue",
         io={"t": [{"ret": "2 images attached"}, {"ret": "queued"}],
             "is_seen": R(True)}),
    case("enter_while_busy", text="new direction", images=[], payload="new direction",
         mode="interrupt",
         state={"agent": {"has_redirect": True, "_supports_active_turn_redirect": True}},
         io={"agent.redirect": R(True), "t": R("redirected"),
             "is_seen": R(False), "busy_input_hint_cli": R("tip"),
             "mark_seen": R(True)}),
    case("enter_while_busy", text="legacy", images=[], payload="legacy",
         mode="interrupt",
         state={"agent": {"has_redirect": True, "_supports_active_turn_redirect": True}},
         io={"agent.redirect": R(False), "is_seen": R(True)}),
    case("enter_while_busy", text="stop talking", images=[], payload="stop talking",
         mode="interrupt",
         state={"agent": {"has_redirect": True, "_supports_active_turn_redirect": True}},
         io={"agent.redirect": R("voice_stopped")}),
    case("enter_while_busy", text="no support", images=[], payload="no support",
         mode="interrupt",
         state={"agent": {"has_redirect": True, "_supports_active_turn_redirect": False}},
         io={"is_seen": R(True)}),
    case("enter_while_busy", text="x", images=[], payload="x", mode="queue",
         state={"busy_input_mode": "steer", "agent": {"has_steer": True}},
         io={"self._gateway_queue_store": R(None), "t": R("q"), "is_seen": R(True)}),
    case("enter_while_busy", text="x" * 100, images=[], payload="x" * 100,
         state={"busy_input_mode": "steer", "agent": {"has_steer": True}},
         io={"agent.steer": R(True), "t": R("s"), "is_seen": R(True)}),
    case("enter_while_busy", text="x", images=[], payload="x", mode="queue",
         io={"self._gateway_queue_store": R(None), "t": R("q"),
             "import:agent.onboarding": IMP("ImportError", "no onboarding")}),
    case("enter_while_busy", text="", images=[], payload="", mode="steer",
         io={"t": [{"ret": "0 images"}, {"ret": "q"}], "is_seen": R(True)}),
]

# ── _tui_enter_overlay ───────────────────────────────────────────────────
CASES += [
    case("enter_overlay"),
    case("enter_overlay", state={**buf("val"), "_connection_state": copy.deepcopy(CONN)},
         io={"self._connection_set_field": R(None)}),
    case("enter_overlay",
         state={**buf("x"), "_connection_state": {**copy.deepcopy(CONN), "field_index": 2}},
         io={"self._connection_submit": R(None)}),
    case("enter_overlay",
         state={**buf("x"), "_connection_state": {**copy.deepcopy(CONN), "phase": "authorized"}},
         io={"self._connection_submit": R(None)}),
    case("enter_overlay", state={**buf("pw"), "_sudo_state": {"response_queue": []}}),
    case("enter_overlay", state={**buf("s3cret"), "_secret_state": {"var_name": "KEY"}},
         io={"self._submit_secret_response": R(None)}),
    case("enter_overlay",
         state={"_approval_state": {"choices": ["once", "deny"], "selected": 0}},
         io={"self._handle_approval_selection": R(None)}),
    case("enter_overlay",
         state={**buf(" y "), "_slash_confirm_state": {
             "choices": [["yes", "Yes", "proceed"], ["no", "No", "abort"]], "selected": 0}},
         io={"self._normalize_slash_confirm_choice": R("yes"),
             "self._submit_slash_confirm_response": R(None)}),
    case("enter_overlay",
         state={**buf(""), "_slash_confirm_state": {
             "choices": [["yes", "Yes", "proceed"], ["no", "No", "abort"]], "selected": 1}},
         io={"self._submit_slash_confirm_response": R(None)}),
    case("enter_overlay",
         state={**buf("zzz"), "_slash_confirm_state": {
             "choices": [["yes", "Yes", "proceed"]], "selected": 5}},
         io={"self._normalize_slash_confirm_choice": R(None),
             "self._submit_slash_confirm_response": R(None)}),
    case("enter_overlay", state={"_model_picker_state": {"stage": "provider"}},
         io={"resolve_persist_behavior": R(False),
             "self._handle_model_picker_selection": R(None)}),
    case("enter_overlay", state={"_model_picker_state": {"stage": "provider"}},
         io={"resolve_persist_behavior": R(False),
             "self._handle_model_picker_selection": RAISE("ValueError", "bad model"),
             "t": R("selection failed"), "self._close_model_picker": R(None)}),
    case("enter_overlay", state={"_model_picker_state": {"stage": "provider"}},
         io={"import:hermes_cli.model_switch": IMP("ImportError", "no model_switch"),
             "t": R("fail"), "self._close_model_picker": R(None)}),
    case("enter_overlay",
         state={"_clarify_state": copy.deepcopy(CLARIFY), "_clarify_freetext": True},
         io={"self._tui_enter_clarify_freetext": R(None)}),
    case("enter_overlay",
         state={"_clarify_state": copy.deepcopy(CLARIFY), "_clarify_freetext": False},
         io={"self._tui_enter_clarify_choice": R(None)}),
    case("enter_overlay",
         state={**buf("v"), "_connection_state": copy.deepcopy(CONN),
                "_sudo_state": {"response_queue": []},
                "_approval_state": {"choices": ["once"]}},
         io={"self._connection_set_field": R(None)}),
]

# ── _tui_enter_clarify_freetext ──────────────────────────────────────────
CASES += [
    case("enter_clarify_freetext", state={**buf("  "), "_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_lock": R(None)}),
    case("enter_clarify_freetext",
         state={**buf(""), "_clarify_state": copy.deepcopy(CLARIFY),
                "_clarify_multi_base": ["a", "b"]},
         io={"self._clarify_batch_lock": R(None)}),
    case("enter_clarify_freetext", state={**buf("blue"), "_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_lock": R(None)}),
    case("enter_clarify_freetext",
         state={**buf("b"), "_clarify_state": copy.deepcopy(CLARIFY),
                "_clarify_multi_base": ["a"]},
         io={"self._clarify_batch_lock": R(None)}),
    case("enter_clarify_freetext",
         state={**buf("x"), "_clarify_state": copy.deepcopy(CLARIFY), "_clarify_multi_base": []},
         io={"self._clarify_batch_lock": R(None)}),
    case("enter_clarify_freetext",
         state={**buf("café ☕"), "_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_lock": R(None)}),
    case("enter_clarify_freetext",
         state={**buf("ans"), "_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_lock": RAISE("RuntimeError", "lock fail")}),
]

# ── _tui_enter_clarify_choice ────────────────────────────────────────────
CASES += [
    case("enter_clarify_choice", state={"_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_enter": R(None)}),
    case("enter_clarify_choice", state={"_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_enter": EFF(None, _clarify_freetext=True,
                                              _clarify_prefill="prev")}),
    case("enter_clarify_choice", state={"_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_enter": EFF(None, _clarify_freetext=True,
                                              _clarify_prefill="café")}),
    case("enter_clarify_choice", state={"_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clarify_batch_enter": EFF(None, _clarify_freetext=True,
                                              _clarify_prefill="")}),
]

# ── _tui_handle_ctrl_c ───────────────────────────────────────────────────
CASES += [
    case("handle_ctrl_c", io={"time.time": R(1000),
                              "self._tui_cancel_voice_recording": R(True)}),
    case("handle_ctrl_c", io={"time.time": R(1000),
                              "self._tui_cancel_voice_recording": R(False),
                              "self._tui_cancel_foreground_ui": R(True)}),
    case("handle_ctrl_c", io={"time.time": R(1000),
                              "self._tui_cancel_voice_recording": R(False),
                              "self._tui_cancel_foreground_ui": R(False),
                              "self._tui_clear_blocking_overlays": R(True)}),
    case("handle_ctrl_c",
         state={"_agent_running": True, "agent": {"has_steer": True}},
         io={"time.time": R(1000),
             "self._tui_cancel_voice_recording": R(False),
             "self._tui_cancel_foreground_ui": R(False),
             "self._tui_clear_blocking_overlays": R(True),
             "t": R("press again to exit"), "request_hard_interrupt": R(None)}),
    case("handle_ctrl_c",
         state={"_agent_running": True, "agent": {"has_steer": True},
                "_last_ctrl_c_time": 999},
         io={"time.time": R(1000),
             "self._tui_cancel_voice_recording": R(False),
             "self._tui_cancel_foreground_ui": R(False),
             "self._tui_clear_blocking_overlays": R(False),
             "t": R("force exiting")}),
    case("handle_ctrl_c",
         state={**buf("draft"), "_agent_running": True, "agent": {"has_steer": True}},
         io={"time.time": R(1000),
             "self._tui_cancel_voice_recording": R(False),
             "self._tui_cancel_foreground_ui": R(False),
             "self._tui_clear_blocking_overlays": R(False),
             "self._tui_clear_or_exit": R(None)}),
    case("handle_ctrl_c", io={"time.time": R(1000),
                              "self._tui_cancel_voice_recording": R(False),
                              "self._tui_cancel_foreground_ui": R(False),
                              "self._tui_clear_blocking_overlays": R(False),
                              "self._tui_clear_or_exit": R(None)}),
    case("handle_ctrl_c",
         state={"_agent_running": True, "agent": None},
         io={"time.time": R(1000),
             "self._tui_cancel_voice_recording": R(False),
             "self._tui_cancel_foreground_ui": R(False),
             "self._tui_clear_blocking_overlays": R(True)}),
]

# ── _tui_handle_ctrl_q ───────────────────────────────────────────────────
CASES += [
    case("handle_ctrl_q", io={"self._tui_cancel_voice_recording": R(True)}),
    case("handle_ctrl_q", io={"self._tui_cancel_voice_recording": R(False),
                              "self._tui_cancel_foreground_ui": R(True)}),
    case("handle_ctrl_q",
         state={"_agent_running": True, "agent": {"has_steer": True},
                "_last_ctrl_c_time": 999},
         io={"self._tui_cancel_voice_recording": R(False),
             "self._tui_cancel_foreground_ui": R(False),
             "self._tui_clear_blocking_overlays": R(False),
             "t": R("interrupting"), "request_hard_interrupt": R(None)}),
    case("handle_ctrl_q", io={"self._tui_cancel_voice_recording": R(False),
                              "self._tui_cancel_foreground_ui": R(False),
                              "self._tui_clear_blocking_overlays": R(False),
                              "self._tui_clear_or_exit": R(None)}),
]

# ── _tui_handle_escape_modal ─────────────────────────────────────────────
CASES += [
    case("handle_escape_modal", state={"_connection_state": copy.deepcopy(CONN)},
         io={"self._connection_cancel": R(None)}),
    case("handle_escape_modal", state={"_secret_state": {"var_name": "KEY"}},
         io={"self._cancel_secret_capture": R(None)}),
    case("handle_escape_modal", state={**buf("pw"), "_sudo_state": {"response_queue": []}}),
    case("handle_escape_modal", state={"_slash_confirm_state": {"choices": []}},
         io={"self._submit_slash_confirm_response": R(None)}),
    case("handle_escape_modal"),
    case("handle_escape_modal",
         state={"_connection_state": copy.deepcopy(CONN),
                "_sudo_state": {"response_queue": []}},
         io={"self._connection_cancel": R(None)}),
]

# ── _tui_handle_ctrl_d ───────────────────────────────────────────────────
CASES += [
    case("handle_ctrl_d", state=buf("abc", 1)),
    case("handle_ctrl_d", state=buf("")),
    case("handle_ctrl_d", state={**buf(""), "_agent_running": True}),
    case("handle_ctrl_d", state={**buf(""), "_attached_images": ["a.png"]}),
    case("handle_ctrl_d", state=buf("ab", 2)),
    case("handle_ctrl_d", state=buf("aébc", 1)),
]

# ── _tui_clear_or_exit ───────────────────────────────────────────────────
CASES += [
    case("clear_or_exit", state=buf("x")),
    case("clear_or_exit", state={**buf(""), "_attached_images": ["a.png"]}),
    case("clear_or_exit", state=buf("")),
    case("clear_or_exit", state={**buf("x"), "_attached_images": ["a.png", "b.png"]}),
]

# ── _tui_clear_blocking_overlays ─────────────────────────────────────────
CASES += [
    case("clear_blocking_overlays"),
    case("clear_blocking_overlays", state={"_approval_state": {"choices": ["once"]}},
         io={"self._clear_active_overlays_for_interrupt": R(None)}),
    case("clear_blocking_overlays", state={"_clarify_state": copy.deepcopy(CLARIFY)},
         io={"self._clear_active_overlays_for_interrupt": R(None)}),
    case("clear_blocking_overlays", state={"_connection_state": copy.deepcopy(CONN)},
         io={"self._clear_active_overlays_for_interrupt": R(None)}),
    case("clear_blocking_overlays",
         state={"_sudo_state": {"response_queue": []}, "_secret_state": {},
                "_approval_state": {"choices": ["once"]}},
         io={"self._clear_active_overlays_for_interrupt": R(None)}),
]

# ── _tui_cancel_foreground_ui ────────────────────────────────────────────
CASES += [
    case("cancel_foreground_ui", closers=copy.deepcopy(CLOSERS)),
    case("cancel_foreground_ui", closers=copy.deepcopy(CLOSERS),
         state={"_slash_confirm_state": {"choices": []}},
         io={"self._submit_slash_confirm_response": R(None)}),
    case("cancel_foreground_ui", closers=copy.deepcopy(CLOSERS),
         state={"_model_picker_state": {"stage": "provider"}},
         io={"self._close_model_picker": R(None)}),
    case("cancel_foreground_ui", closers=copy.deepcopy(CLOSERS),
         state={"_gateway_command_wait": {"id": "gw1", "set": False}}),
    case("cancel_foreground_ui", closers=copy.deepcopy(CLOSERS),
         state={"_slash_confirm_state": {}}),
    case("cancel_foreground_ui", closers=copy.deepcopy(CLOSERS),
         state={"_command_palette_state": {"entries": []}},
         io={"self._close_command_palette": R(None)}),
]

# ── _tui_connection_move ─────────────────────────────────────────────────
CASES += [
    case("connection_move", delta=1),
    case("connection_move", delta=1,
         state={**buf("v"), "_connection_state": copy.deepcopy(CONN)},
         io={"self._connection_sync_input_buffer": R(None), "self._paint_now": R(None)}),
    case("connection_move", delta=-1,
         state={**buf("v"), "_connection_state": copy.deepcopy(CONN)},
         io={"self._connection_sync_input_buffer": R(None), "self._paint_now": R(None)}),
    case("connection_move", delta=1,
         state={"_connection_state": {**copy.deepcopy(CONN), "field_index": 2}},
         io={"self._connection_sync_input_buffer": R(None), "self._paint_now": R(None)}),
    case("connection_move", delta=-1,
         state={"_connection_state": {**copy.deepcopy(CONN), "field_index": 2}},
         io={"self._connection_sync_input_buffer": R(None), "self._paint_now": R(None)}),
    case("connection_move", delta=-1,
         state={"_connection_state": {**copy.deepcopy(CONN), "phase": "authorized"}},
         io={"self._paint_now": R(None)}),
    case("connection_move", delta=1,
         state={"_connection_state": {**copy.deepcopy(CONN), "phase": "waiting"}},
         io={"self._paint_now": R(None)}),
    case("connection_move", delta=1,
         state={**buf("w"), "_connection_state": {**copy.deepcopy(CONN), "phase": "failed"}},
         io={"self._connection_sync_input_buffer": R(None), "self._paint_now": R(None)}),
]

# ── _tui_model_picker_escape ─────────────────────────────────────────────
CASES += [
    case("model_picker_escape",
         state={"_model_picker_state": {"stage": "model", "filter": "abc",
                                        "selected": 3, "_scroll_offset": 2}}),
    case("model_picker_escape",
         state={"_model_picker_state": {"stage": "reasoning", "selected": 2,
                                        "switch_result": {"new_model": "m"}}}),
    case("model_picker_escape", state={"_model_picker_state": {"stage": "provider"}},
         io={"self._close_model_picker": R(None)}),
    case("model_picker_escape", io={"self._close_model_picker": R(None)}),
    case("model_picker_escape",
         state={"_model_picker_state": {"stage": "model", "filter": ""}},
         io={"self._close_model_picker": R(None)}),
]

# ── _tui_paste_over_threshold ────────────────────────────────────────────
CASES += [
    case("paste_over_threshold", text="abc", line_count=1,
         threshold_key="paste_collapse_threshold"),
    case("paste_over_threshold", text="a\nb\nc\nd\ne", line_count=5,
         threshold_key="paste_collapse_threshold"),
    case("paste_over_threshold", text="x" * 2000, line_count=0,
         threshold_key="paste_collapse_threshold"),
    case("paste_over_threshold", text="x" * 3000, line_count=100,
         threshold_key="paste_collapse_threshold",
         state={"config": {"paste_collapse_threshold": 0,
                           "paste_collapse_char_threshold": 0}}),
    case("paste_over_threshold", text="x" * 1999, line_count=0,
         threshold_key="paste_collapse_threshold"),
    case("paste_over_threshold", text="é" * 1999, line_count=0,
         threshold_key="paste_collapse_threshold"),
    case("paste_over_threshold", text="short", line_count=3,
         threshold_key="my_thresh", state={"config": {"my_thresh": 3}}),
]

# ── _tui_suppress_closed_loop_errors ─────────────────────────────────────
CASES += [
    case("suppress_closed_loop_errors",
         context={"exception": {"type": "RuntimeError", "message": "Event loop is closed"}}),
    case("suppress_closed_loop_errors",
         context={"exception": {"type": "RuntimeError", "message": "boom"}},
         io={"loop.default_exception_handler": R(None)}),
    case("suppress_closed_loop_errors",
         context={"exception": {"type": "KeyError", "message": "selector is not registered"}}),
    case("suppress_closed_loop_errors",
         context={"exception": {"type": "OSError", "message": "I/O error", "errno": 5}}),
    case("suppress_closed_loop_errors",
         context={"exception": {"type": "OSError", "message": "denied", "errno": 1}},
         io={"loop.default_exception_handler": R(None)}),
    case("suppress_closed_loop_errors",
         context={"exception": {"type": "ValueError", "message": "x"}},
         io={"loop.default_exception_handler": R(None)}),
    case("suppress_closed_loop_errors",
         context={"message": "Task exception was never retrieved"},
         io={"loop.default_exception_handler": R(None)}),
    case("suppress_closed_loop_errors",
         context={"exception": {"type": "KeyError", "message": "something else"}},
         io={"loop.default_exception_handler": R(None)}),
]


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 4}
    print(f"wrote {OUT} ({len(CASES)} cases, {len(counts)} funcs)")
    if thin:
        raise SystemExit(f"FATAL: funcs with <4 cases: {thin}")


if __name__ == "__main__":
    main()
