#!/usr/bin/env python3
"""Generate tests/fixtures/cli_tui_mixin_flow.jsonl for the cli_tui_mixin.py
navigation/editor/stash FLOW port (src/cli/cli_tui_mixin_flow.c).

Each case carries the FULL state schema (defaults merged with overrides) so
the C harness and the oracle build identical mirrors without shared default
tables. Scripted seam results live in in.io; "import:<module>" keys must be
single entries. Run: python tests/gen_cli_tui_mixin_flow_cases.py
"""
import copy
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "cli_tui_mixin_flow.jsonl"

DEFAULT_STATE = {
    "buf": {"text": "", "cursor_position": 0},
    "_connection_state": None,
    "_model_picker_state": None,
    "_clarify_state": None,
    "_clarify_freetext": False,
    "_command_palette_state": None,
    "_slash_confirm_state": None,
    "_approval_state": None,
    "_attached_images": [],
    "_skip_paste_collapse": False,
    "_voice_recorder": None,
    "_prompt_stash": None,
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


def clarify(choices=None, selected=0, active=0, indices=None, questions=None):
    d = {"choices": choices if choices is not None else ["red", "blue"],
         "selected": selected, "active": active,
         "questions": questions if questions is not None else [{"qid": "q1"}, {"qid": "q2"}],
         "answers": {}}
    if indices is not None:
        d["selected_indices"] = indices
    return {"_clarify_state": d}


CASES = []


def add(func, state=None, io=None, **extra):
    CASES.append(case(func, state, io, **extra))


# ── module leaves ────────────────────────────────────────────────────────
for ch in ("once", "session", "always", "deny", "view", "unknown"):
    add("approval_choice_label", io={"t": R("LABEL-" + ch)}, choice=ch)
add("approval_choice_label", io={"t": RAISE("ValueError", "boom")}, choice="once")

add("agent_name", io={"get_active_skin": R({}), "get_branding": R("Jarvis")})
add("agent_name", io={"import:hermes_cli.skin_engine": IMP("ImportError", "no skin")})
add("agent_name", io={"get_active_skin": RAISE("RuntimeError", "skin fail")})
add("agent_name", io={"get_active_skin": R(None)})
add("agent_name", io={"get_active_skin": R({}), "get_branding": RAISE("ValueError", "brand fail")})
add("agent_name", io={"get_active_skin": R({}), "get_branding": RAISE("SystemExit", "bye")})

add("tn", io={"t": R("ONE")}, key="cli.tui.x", count=1)
add("tn", io={"t": R("OTHER")}, key="cli.tui.x", count=0)
add("tn", io={"t": R("OTHER")}, key="cli.tui.x", count=2)
add("tn", io={"t": R("KW")}, key="cli.tui.y", count=3, kwargs={"name": "n"})
add("tn", io={"t": RAISE("ValueError", "tn fail")}, key="cli.tui.x", count=1)

for i in (0, 8, 9, 10, -1, 100):
    add("num_prefix", i=i)

add("term_rows", io={"shutil.get_terminal_size": R({"lines": 40, "columns": 100})})
add("term_rows", io={"shutil.get_terminal_size": R({"lines": 24, "columns": 80})})
add("term_rows", io={"shutil.get_terminal_size": R(None)})
add("term_rows", io={"shutil.get_terminal_size": RAISE("OSError", "no tty")})
add("term_rows", io={"shutil.get_terminal_size": R({"columns": 80})})

add("set_filter", st={"filter": "abc", "selected": 5, "_scroll_offset": 3}, value="")
add("set_filter", st={"filter": "", "selected": 0, "_scroll_offset": 0}, value="xy")
add("set_filter", st={"selected": 2}, value="q")
add("set_filter", st={"filter": "old", "selected": 1, "_scroll_offset": 9, "extra": 1}, value="new")
add("set_filter", st={}, value="z")

add("audio_level_bar", state={"_voice_recorder": None})
add("audio_level_bar", state={"_voice_recorder": {"current_rms": 0}})
add("audio_level_bar", state={"_voice_recorder": {"current_rms": 4000}})
add("audio_level_bar", state={"_voice_recorder": {"current_rms": 8000}})
add("audio_level_bar", state={"_voice_recorder": {"current_rms": 20000}})
add("audio_level_bar", state={"_voice_recorder": {}})

# ── connection navigation ────────────────────────────────────────────────
add("connection_up", io={"self._tui_connection_move": R(None)})
add("connection_up", io={"self._tui_connection_move": RAISE("ValueError", "mv")})
add("connection_up", io={"self._tui_connection_move": RAISE("SystemExit", "bye")})
add("connection_up", io={"self._tui_connection_move": R(True)})
add("connection_up", io={})
add("connection_down", io={"self._tui_connection_move": R(None)})
add("connection_down", io={"self._tui_connection_move": RAISE("RuntimeError", "mv")})
add("connection_down", io={"self._tui_connection_move": RAISE("KeyboardInterrupt", "")})
add("connection_down", io={"self._tui_connection_move": R(True)})
add("connection_down", io={})

CONN_FORM = {"phase": "form", "fields": [{"name": "a"}, {"name": "b"}],
             "field_index": 0, "selected": 0}
add("connection_side", state={"_connection_state": None})
add("connection_side", state={"_connection_state": {"phase": "authorized", "selected": 0}},
    io={"self._paint_now": R(None)})
add("connection_side", state={"_connection_state": {"phase": "authorized", "selected": 1}},
    io={"self._paint_now": R(None)})
add("connection_side", state={"_connection_state": dict(CONN_FORM, field_index=2, selected=0)},
    io={"self._paint_now": R(None)})
add("connection_side", state={"_connection_state": dict(CONN_FORM, field_index=0)},
    io={"self._paint_now": R(None)})
add("connection_side", state={"_connection_state": {"phase": "authorized", "selected": 0}},
    io={"self._paint_now": RAISE("SystemExit", "bye")})

# ── model picker navigation ──────────────────────────────────────────────
add("model_picker_down", state={"_model_picker_state": None})
add("model_picker_down", state={"_model_picker_state": {"stage": "provider", "providers": [1, 2, 3], "selected": 2}})
add("model_picker_down", state={"_model_picker_state": {"stage": "provider", "providers": [1, 2, 3], "selected": 3}})
add("model_picker_down", state={"_model_picker_state": {"stage": "reasoning", "selected": 0}},
    io={"_picker_reasoning_rows": R([["low", "Low"], ["medium", "Medium"]])})
add("model_picker_down", state={"_model_picker_state": {"stage": "reasoning", "selected": 0}},
    io={"import:hermes_cli.cli_model_switch_mixin": IMP("ImportError", "no msm")})
add("model_picker_down", state={"_model_picker_state": {"stage": "model", "model_list": ["m1", "m2"], "selected": 1}})
add("model_picker_down", state={"_model_picker_state": {"stage": "model", "_filtered_pairs": [[0, "m1"]], "model_list": ["m1", "m2"], "selected": 0}})
add("model_picker_down", state={"_model_picker_state": {"stage": "model", "_filtered_pairs": None, "model_list": ["m1"], "selected": 5}})

add("model_picker_up", state={"_model_picker_state": None})
add("model_picker_up", state={"_model_picker_state": {"selected": 3}})
add("model_picker_up", state={"_model_picker_state": {"selected": 0}})
add("model_picker_up", state={"_model_picker_state": {"selected": 1}})
add("model_picker_up", state={"_model_picker_state": {}})

add("model_picker_filter_backspace", state={"_model_picker_state": None})
add("model_picker_filter_backspace", state={"_model_picker_state": {"filter": "abc", "selected": 4, "_scroll_offset": 2}})
add("model_picker_filter_backspace", state={"_model_picker_state": {"filter": "", "selected": 1}})
add("model_picker_filter_backspace", state={"_model_picker_state": {"filter": "x", "selected": 2}})
add("model_picker_filter_backspace", state={"_model_picker_state": {"filter": "héllo", "selected": 1}})

# ── clarify navigation ───────────────────────────────────────────────────
add("clarify_toggle", state={"_clarify_state": None})
add("clarify_toggle", state=clarify(selected=1, indices=[0, 2]))
add("clarify_toggle", state=clarify(selected=2, indices=[0, 2]))
add("clarify_toggle", state=clarify(selected=0, indices=[]))
add("clarify_toggle", state=clarify(selected=1))   # indices absent -> no write-back
add("clarify_toggle", state={"_clarify_state": {"choices": ["a"]}})  # selected absent -> KeyError

add("clarify_down", state={"_clarify_state": None})
add("clarify_down", state=clarify(choices=["a", "b"], selected=0))
add("clarify_down", state=clarify(choices=["a", "b"], selected=2))
add("clarify_down", state=clarify(choices=["a", "b"], selected=1))
add("clarify_down", state=clarify(choices=[], selected=0))
add("clarify_down", state={"_clarify_state": {"choices": ["a"]}})  # selected absent -> KeyError

add("clarify_up", state={"_clarify_state": None})
add("clarify_up", state=clarify(selected=2))
add("clarify_up", state=clarify(selected=0))
add("clarify_up", state=clarify(selected=1))
add("clarify_up", state={"_clarify_state": {"choices": ["a"]}})  # KeyError

add("clarify_batch_step", state={"_clarify_state": None}, delta=1)
add("clarify_batch_step", state=clarify(active=0, questions=[{"qid": "q1"}, {"qid": "q2"}]),
    io={"self._clarify_batch_set_active": R(None)}, delta=1)
add("clarify_batch_step", state=clarify(active=0, questions=[{"qid": "q1"}, {"qid": "q2"}]),
    io={"self._clarify_batch_set_active": R(None)}, delta=-1)
add("clarify_batch_step", state=clarify(active=1, questions=[{"qid": "q1"}, {"qid": "q2"}, {"qid": "q3"}]),
    io={"self._clarify_batch_set_active": R(None)}, delta=1)
add("clarify_batch_step", state=clarify(active=0, questions=[]), delta=1)  # ZeroDivisionError
add("clarify_batch_step", state=clarify(active=0),
    io={"self._clarify_batch_set_active": RAISE("SystemExit", "bye")}, delta=1)

add("clarify_batch_tab", state={"_clarify_state": None})
add("clarify_batch_tab", state=clarify(active=0), io={"self._clarify_batch_set_active": R(None)})
add("clarify_batch_tab", state=clarify(active=1), io={"self._clarify_batch_set_active": R(None)})
add("clarify_batch_tab", state=clarify(active=0, questions=[]))
add("clarify_batch_tab", state=clarify(active=0), io={"self._clarify_batch_set_active": RAISE("ValueError", "x")})

add("clarify_batch_backtab", state={"_clarify_state": None})
add("clarify_batch_backtab", state=clarify(active=0), io={"self._clarify_batch_set_active": R(None)})
add("clarify_batch_backtab", state=clarify(active=1), io={"self._clarify_batch_set_active": R(None)})
add("clarify_batch_backtab", state=clarify(active=0, questions=[]))
add("clarify_batch_backtab", state=clarify(active=2, questions=[{"qid": "a"}, {"qid": "b"}, {"qid": "c"}]),
    io={"self._clarify_batch_set_active": R(None)})

# ── command palette navigation ───────────────────────────────────────────
add("command_palette_backspace", state={"_command_palette_state": None})
add("command_palette_backspace", state={"_command_palette_state": {"filter": "abc", "selected": 3, "_scroll_offset": 1}})
add("command_palette_backspace", state={"_command_palette_state": {"filter": "", "selected": 0}})
add("command_palette_backspace", state={"_command_palette_state": {"filter": "z", "selected": 2}})
add("command_palette_backspace", state={"_command_palette_state": {"filter": "日本語", "selected": 1}})

add("command_palette_down", state={"_command_palette_state": None})
add("command_palette_down", state={"_command_palette_state": {"selected": 0}},
    io={"self._command_palette_visible_entries": R([["c1", "cat", "d1"], ["c2", "cat", "d2"]])})
add("command_palette_down", state={"_command_palette_state": {"selected": 1, "_visible_count": 5}},
    io={"self._command_palette_visible_entries": R([["c1", "cat", "d1"]])})
add("command_palette_down", state={"_command_palette_state": {"selected": 4, "_visible_count": 5}},
    io={"self._command_palette_visible_entries": R([])})
add("command_palette_down", state={"_command_palette_state": {"selected": 0}},
    io={"self._command_palette_visible_entries": R([])})
add("command_palette_down", state={"_command_palette_state": {"selected": 0}},
    io={"self._command_palette_visible_entries": RAISE("SystemExit", "bye")})

add("command_palette_up", state={"_command_palette_state": None})
add("command_palette_up", state={"_command_palette_state": {"selected": 3}})
add("command_palette_up", state={"_command_palette_state": {"selected": 0}})
add("command_palette_up", state={"_command_palette_state": {"selected": 1}})
add("command_palette_up", state={"_command_palette_state": {}})

add("command_palette_enter", io={"self._handle_command_palette_selection": R(None)})
add("command_palette_enter", io={"self._handle_command_palette_selection": RAISE("ValueError", "sel")})
add("command_palette_enter", io={"self._handle_command_palette_selection": RAISE("SystemExit", "bye")})
add("command_palette_enter", io={"self._handle_command_palette_selection": R(True)})
add("command_palette_enter", io={})
add("command_palette_escape", io={"self._close_command_palette": R(None)})
add("command_palette_escape", io={"self._close_command_palette": RAISE("RuntimeError", "close")})
add("command_palette_escape", io={"self._close_command_palette": RAISE("KeyboardInterrupt", "")})
add("command_palette_escape", io={"self._close_command_palette": R(True)})
add("command_palette_escape", io={})

# ── slash-confirm / approval navigation ──────────────────────────────────
add("slash_confirm_down", state={"_slash_confirm_state": None})
add("slash_confirm_down", state={"_slash_confirm_state": {"choices": [["a", "A"], ["b", "B"]], "selected": 0}})
add("slash_confirm_down", state={"_slash_confirm_state": {"choices": [["a", "A"], ["b", "B"]], "selected": 1}})
add("slash_confirm_down", state={"_slash_confirm_state": {"choices": [], "selected": 0}})
add("slash_confirm_down", state={"_slash_confirm_state": {"selected": 0}})
add("slash_confirm_up", state={"_slash_confirm_state": None})
add("slash_confirm_up", state={"_slash_confirm_state": {"selected": 2}})
add("slash_confirm_up", state={"_slash_confirm_state": {"selected": 0}})
add("slash_confirm_up", state={"_slash_confirm_state": {"selected": 1}})
add("slash_confirm_up", state={"_slash_confirm_state": {}})

add("approval_down", state={"_approval_state": None})
add("approval_down", state={"_approval_state": {"choices": ["once", "deny"], "selected": 0}})
add("approval_down", state={"_approval_state": {"choices": ["once", "deny"], "selected": 1}})
add("approval_down", state={"_approval_state": {"choices": ["once"], "selected": 0}})
add("approval_down", state={"_approval_state": {"selected": 0}})  # choices absent -> KeyError
add("approval_up", state={"_approval_state": None})
add("approval_up", state={"_approval_state": {"choices": ["once", "deny"], "selected": 1}})
add("approval_up", state={"_approval_state": {"choices": ["once", "deny"], "selected": 0}})
add("approval_up", state={"_approval_state": {"choices": ["once"], "selected": 0}})
add("approval_up", state={"_approval_state": {"choices": ["once"]}})  # selected absent -> KeyError

# ── editor gestures ──────────────────────────────────────────────────────
add("handle_double_escape", state=buf(""))
add("handle_double_escape", state=buf("hello"))
add("handle_double_escape", state={"_attached_images": ["img1"]})
add("handle_double_escape", state={**buf("draft"), "_attached_images": ["img1", "img2"]})
add("handle_double_escape", state={**buf(""), "_attached_images": []})

add("handle_alt_v", io={"self._try_attach_clipboard_image": R(True)})
add("handle_alt_v", io={"self._try_attach_clipboard_image": R(False)})
add("handle_alt_v", io={"self._try_attach_clipboard_image": R(None)})
add("handle_alt_v", io={"self._try_attach_clipboard_image": RAISE("ValueError", "clip")})
add("handle_alt_v", io={"self._try_attach_clipboard_image": RAISE("SystemExit", "bye")})
add("handle_ctrl_v", io={"self._try_attach_clipboard_image": R(True)})
add("handle_ctrl_v", io={"self._try_attach_clipboard_image": R(False)})
add("handle_ctrl_v", io={"self._try_attach_clipboard_image": R(None)})
add("handle_ctrl_v", io={"self._try_attach_clipboard_image": RAISE("RuntimeError", "clip")})
add("handle_ctrl_v", io={"self._try_attach_clipboard_image": RAISE("GeneratorExit", "")})

add("handle_ctrl_l", io={"self._force_full_redraw": R(None)})
add("handle_ctrl_l", io={"self._force_full_redraw": RAISE("ValueError", "redraw")})
add("handle_ctrl_l", io={"self._force_full_redraw": RAISE("SystemExit", "bye")})
add("handle_ctrl_l", io={"self._force_full_redraw": R(True)})
add("handle_ctrl_l", io={})

add("insert_newline", state=buf(""))
add("insert_newline", state=buf("abc"))
add("insert_newline", state=buf("abc", 1))
add("insert_newline", state=buf("abc", 0))
add("insert_newline", state=buf("héllo", 2))

add("handle_open_in_editor", state=buf("draft text"), io={"self._open_external_editor": R(None)})
add("handle_open_in_editor", state=buf(""), io={"self._open_external_editor": R(None)})
add("handle_open_in_editor", state=buf("x"), io={"self._open_external_editor": RAISE("ValueError", "ed")})
add("handle_open_in_editor", state=buf("x"), io={"self._open_external_editor": RAISE("SystemExit", "bye")})
add("handle_open_in_editor", state=buf("x"), io={})

add("recall_without_recollapse", state=buf("before"), io={"move": R(None)})
add("recall_without_recollapse", state=buf("before"),
    io={"move": EFF(None, buf={"text": "after", "cursor_position": 5})})
add("recall_without_recollapse", state={**buf("same"), "_skip_paste_collapse": False},
    io={"move": EFF(None, buf={"text": "same", "cursor_position": 4})})
add("recall_without_recollapse", state=buf("x"), io={"move": RAISE("ValueError", "mv")})
add("recall_without_recollapse", state=buf("x"), io={"move": RAISE("SystemExit", "bye")})

add("history_up", state=buf("before"), io={"buffer.auto_up": R(None)}, arg=1)
add("history_up", state=buf("before"),
    io={"buffer.auto_up": EFF(None, buf={"text": "recalled", "cursor_position": 8})}, arg=1)
add("history_up", state=buf("same"),
    io={"buffer.auto_up": EFF(None, buf={"text": "same", "cursor_position": 4})}, arg=2)
add("history_up", state=buf("x"), io={"buffer.auto_up": RAISE("ValueError", "up")}, arg=1)
add("history_up", state=buf("x"), io={"buffer.auto_up": RAISE("SystemExit", "bye")}, arg=1)
add("history_down", state=buf("before"), io={"buffer.auto_down": R(None)}, arg=1)
add("history_down", state=buf("before"),
    io={"buffer.auto_down": EFF(None, buf={"text": "next", "cursor_position": 4})}, arg=1)
add("history_down", state=buf("same"),
    io={"buffer.auto_down": EFF(None, buf={"text": "same", "cursor_position": 4})}, arg=3)
add("history_down", state=buf("x"), io={"buffer.auto_down": RAISE("RuntimeError", "down")}, arg=1)
add("history_down", state=buf("x"), io={"buffer.auto_down": RAISE("KeyboardInterrupt", "")}, arg=1)

# ── misc flows ───────────────────────────────────────────────────────────
add("wake_startup", io={"self._maybe_start_wake_word": R(None)})
add("wake_startup", io={"self._maybe_start_wake_word": RAISE("ValueError", "wake fail")})
add("wake_startup", io={"self._maybe_start_wake_word": RAISE("RuntimeError", "wake fail")})
add("wake_startup", io={"self._maybe_start_wake_word": RAISE("SystemExit", "bye")})
add("wake_startup", io={"import:cli": IMP("ImportError", "no cli")})
add("wake_startup", io={"self._maybe_start_wake_word": RAISE("KeyError", "reg")})
add("wake_startup", io={"self._maybe_start_wake_word": RAISE("ValueError", "wake"),
                        "logger.debug": RAISE("RuntimeError", "log fail")})
add("wake_startup", io={"self._maybe_start_wake_word": RAISE("ValueError", "wake"),
                        "logger.debug": R(None)})

add("handle_ignored_terminal_sequence", key_sequence=None)
add("handle_ignored_terminal_sequence", key_sequence=[{"data": "\x1b[I"}],
    io={"self._schedule_focus_regain_redraw": R(None)})
add("handle_ignored_terminal_sequence", key_sequence=[{"data": "x"}, {"data": "\x1b[I"}, {"data": "\x1b[I"}],
    io={"self._schedule_focus_regain_redraw": R(None)})
add("handle_ignored_terminal_sequence", key_sequence=[{"data": "x"}, {"data": "y"}])
add("handle_ignored_terminal_sequence", key_sequence=5)
add("handle_ignored_terminal_sequence", key_sequence="abc")
add("handle_ignored_terminal_sequence", key_sequence=[{"data": "\x1b[I"}],
    io={"self._schedule_focus_regain_redraw": RAISE("ValueError", "focus")})
add("handle_ignored_terminal_sequence", key_sequence=[{"data": "\x1b[I"}],
    io={"self._schedule_focus_regain_redraw": RAISE("SystemExit", "bye")})

# ── stash browse-panel + restore ─────────────────────────────────────────
add("stash_panel_up", state={"_prompt_stash": {}}, io={"prompt_stash.move_cursor": R(None)})
add("stash_panel_up", state={"_prompt_stash": {}}, io={"prompt_stash.move_cursor": RAISE("ValueError", "mv")})
add("stash_panel_up", state={"_prompt_stash": {}}, io={"prompt_stash.move_cursor": RAISE("SystemExit", "bye")})
add("stash_panel_up", state={"_prompt_stash": None})   # AttributeError
add("stash_panel_up", state={"_prompt_stash": {}}, io={})
add("stash_panel_down", state={"_prompt_stash": {}}, io={"prompt_stash.move_cursor": R(None)})
add("stash_panel_down", state={"_prompt_stash": {}}, io={"prompt_stash.move_cursor": RAISE("RuntimeError", "mv")})
add("stash_panel_down", state={"_prompt_stash": {}}, io={"prompt_stash.move_cursor": RAISE("KeyboardInterrupt", "")})
add("stash_panel_down", state={"_prompt_stash": None})
add("stash_panel_down", state={"_prompt_stash": {}}, io={})
add("stash_panel_delete", state={"_prompt_stash": {}}, io={"prompt_stash.delete_at_cursor": R(None)})
add("stash_panel_delete", state={"_prompt_stash": {}}, io={"prompt_stash.delete_at_cursor": RAISE("ValueError", "del")})
add("stash_panel_delete", state={"_prompt_stash": {}}, io={"prompt_stash.delete_at_cursor": RAISE("SystemExit", "bye")})
add("stash_panel_delete", state={"_prompt_stash": None})
add("stash_panel_delete", state={"_prompt_stash": {}}, io={})
add("stash_panel_close", state={"_prompt_stash": {}}, io={"prompt_stash.close_panel": R(None)})
add("stash_panel_close", state={"_prompt_stash": {}}, io={"prompt_stash.close_panel": RAISE("RuntimeError", "close")})
add("stash_panel_close", state={"_prompt_stash": {}}, io={"prompt_stash.close_panel": RAISE("GeneratorExit", "")})
add("stash_panel_close", state={"_prompt_stash": None})
add("stash_panel_close", state={"_prompt_stash": {}}, io={})

add("restore_stash_payload", payload=None)
add("restore_stash_payload", payload=[])
add("restore_stash_payload", payload=["restored text", []], state=buf("old"))
add("restore_stash_payload", payload=["txt", ["img1", "img2"]], state={"_attached_images": ["img0"]})
add("restore_stash_payload", payload=["txt", ["img1", "img1", "img0"]], state={"_attached_images": ["img0"]})
add("restore_stash_payload", payload=["only"])           # ValueError unpack
add("restore_stash_payload", payload=["a", "b", "c"])    # ValueError unpack
add("restore_stash_payload", payload=["txt", None], state=buf("old"))


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function counts (>=5 required)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    low = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    if low:
        print(f"WARNING funcs with <5 cases: {low}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()