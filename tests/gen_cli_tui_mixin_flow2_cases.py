#!/usr/bin/env python3
"""Generate tests/fixtures/cli_tui_mixin_flow2.jsonl for the cli_tui_mixin.py
unclaimed-remainder FLOW port (src/cli/cli_tui_mixin_flow2.c).

Each case carries the FULL state schema (defaults merged with overrides) so
the C harness and the oracle build identical mirrors without shared default
tables. Scripted seam results live in in.io; "import:<module>" keys must be
single entries. Run: python tests/gen_cli_tui_mixin_flow2_cases.py
"""
import copy
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "cli_tui_mixin_flow2.jsonl"

DEFAULT_STATE = {
    "buf": {"text": "", "cursor_position": 0, "complete_state": None, "suggestion": None},
    "_voice_recording": False,
    "_voice_processing": False,
    "_voice_mode": False,
    "_voice_live_text": "",
    "_sudo_state": None,
    "_secret_state": None,
    "_connection_state": None,
    "_approval_state": None,
    "_slash_confirm_state": None,
    "_clarify_state": None,
    "_clarify_freetext": False,
    "_clarify_deadline": None,
    "_sudo_deadline": 0,
    "_secret_deadline": 0,
    "_approval_deadline": 0,
    "_slash_confirm_deadline": 0,
    "_command_running": False,
    "_command_blocks_input": False,
    "_command_status": "",
    "_agent_running": False,
    "_modern_layout": False,
    "busy_input_mode": "queue",
    "_attached_images": [],
    "_image_counter": 0,
    "_tui_style_base": None,
    "_app": None,
    "_prompt_stash": None,
    "_composer_placeholder": "",
    "_model_picker_state": None,
    "_command_palette_state": None,
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


CASES = []


def add(func, state=None, io=None, **extra):
    CASES.append(case(func, state, io, **extra))


def buf(text, cursor=None, complete_state=None, suggestion=None):
    return {"buf": {"text": text,
                    "cursor_position": len(text) if cursor is None else cursor,
                    "complete_state": complete_state,
                    "suggestion": suggestion}}


# ── prompt symbols ───────────────────────────────────────────────────────
SYM_IO = {"get_active_prompt_symbol": R("❯ "), "get_active_profile_name": R("default")}
add("get_tui_prompt_symbols", io=SYM_IO)
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("$ "),
                                 "get_active_profile_name": R("default")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("λ "),
                                 "get_active_profile_name": R("default")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("❯ "),
                                 "get_active_profile_name": R("coder")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("❯ "),
                                 "get_active_profile_name": R(None)})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R(None),
                                 "get_active_profile_name": R("custom")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("  \t "),
                                 "get_active_profile_name": R("default")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("❯ foo bar"),
                                 "get_active_profile_name": R("default")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("❯\t\n"),
                                 "get_active_profile_name": R("default")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R(5)})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("❯ "),
                                 "get_active_profile_name": R([])})
add("get_tui_prompt_symbols", io={"import:hermes_cli.skin_engine": IMP("ImportError", "no skin"),
                                 "get_active_profile_name": R("default")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("❯ "),
                                 "import:hermes_cli.profiles": IMP("ImportError", "no profiles")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": RAISE("ValueError", "sym fail"),
                                 "get_active_profile_name": R("default")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": RAISE("SystemExit", "bye")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("❯ "),
                                 "get_active_profile_name": RAISE("KeyboardInterrupt", "")})
add("get_tui_prompt_symbols", io={"get_active_prompt_symbol": R("› "),
                                 "get_active_profile_name": R("default")})

# ── prompt fragments ─────────────────────────────────────────────────────
FRAG_BASE = {"get_active_prompt_symbol": R("❯ "), "get_active_profile_name": R("default"),
             "self._get_tui_terminal_width": R(80), "self._use_minimal_tui_chrome": R(False)}
add("get_tui_prompt_fragments", io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_voice_recording": True},
    io=dict(FRAG_BASE, **{"self._audio_level_bar": R("▃")}))
add("get_tui_prompt_fragments", state={"_voice_recording": True},
    io=dict(FRAG_BASE, **{"self._audio_level_bar": R("▃"),
                          "self._use_minimal_tui_chrome": R(True)}))
add("get_tui_prompt_fragments", state={"_voice_recording": True},
    io=dict(FRAG_BASE, **{"self._audio_level_bar": R("  "),
                          "self._use_minimal_tui_chrome": R(True)}))
add("get_tui_prompt_fragments", state={"_voice_processing": True}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_sudo_state": {"x": 1}}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_secret_state": {"x": 1}}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_connection_state": {"phase": "form"}}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_approval_state": {"choices": ["once"]}}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_slash_confirm_state": {"selected": 0}}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_clarify_freetext": True}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_clarify_state": {"choices": ["a"]}}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_command_running": True},
    io=dict(FRAG_BASE, **{"self._command_spinner_frame": R("|")}))
add("get_tui_prompt_fragments", state={"_agent_running": True}, io=FRAG_BASE)
add("get_tui_prompt_fragments", state={"_voice_mode": True}, io=FRAG_BASE)
add("get_tui_prompt_fragments", io=dict(FRAG_BASE, **{"self._get_tui_terminal_width": RAISE("ValueError", "w")}))
add("get_tui_prompt_fragments", io=dict(FRAG_BASE, **{"self._use_minimal_tui_chrome": RAISE("SystemExit", "bye")}))
add("get_tui_prompt_fragments", state={"_voice_recording": True},
    io=dict(FRAG_BASE, **{"self._audio_level_bar": RAISE("RuntimeError", "bar")}))
add("get_tui_prompt_fragments", state={"_voice_recording": True},
    io=dict(FRAG_BASE, **{"self._audio_level_bar": R(None)}))
add("get_tui_prompt_fragments", io=dict(FRAG_BASE, **{"import:hermes_cli.skin_engine": IMP("ImportError", "no skin")}))
add("get_tui_prompt_fragments", state={"_command_running": True},
    io=dict(FRAG_BASE, **{"self._command_spinner_frame": R(None),
                          "self._use_minimal_tui_chrome": R(True)}))
add("get_tui_prompt_fragments", state={"_command_running": True},
    io=dict(FRAG_BASE, **{"self._command_spinner_frame": R(None)}))
add("get_tui_prompt_fragments", state={"_command_running": True},
    io=dict(FRAG_BASE, **{"self._command_spinner_frame": R(5),
                          "self._use_minimal_tui_chrome": R(True)}))

# ── style dict ───────────────────────────────────────────────────────────
STYLE_BASE = {"prompt": "#888888 italic", "status-bar": "bg:#1a1a2e #C0C0C0", "hint": ""}
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"_detect_light_mode": R(False)})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"get_prompt_toolkit_style_overrides": R({"prompt": "#FF0000", "extra": "bold"}),
        "_detect_light_mode": R(False)})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"_detect_light_mode": R(True),
        "_maybe_remap_for_light_mode": [R("#000000"), R("#111111")]})
add("build_tui_style_dict", state={"_tui_style_base": {"prompt": "#888888 italic"}},
    io={"_detect_light_mode": R(True),
        "_maybe_remap_for_light_mode": RAISE("ValueError", "remap fail")})
add("build_tui_style_dict", state={"_tui_style_base": {"prompt": "#888888 italic"}},
    io={"_detect_light_mode": R(True),
        "_maybe_remap_for_light_mode": RAISE("SystemExit", "bye")})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"_detect_light_mode": RAISE("RuntimeError", "detect fail")})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"import:cli": IMP("ImportError", "no cli")})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"import:hermes_cli.skin_engine": IMP("ImportError", "no skin"),
        "_detect_light_mode": R(False)})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"get_prompt_toolkit_style_overrides": RAISE("ValueError", "ov fail"),
        "_detect_light_mode": R(False)})
add("build_tui_style_dict", state={"_tui_style_base": None},
    io={"_detect_light_mode": R(True)})
add("build_tui_style_dict", state={"_tui_style_base": {"x": 5}},
    io={"_detect_light_mode": R(True)})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"get_prompt_toolkit_style_overrides": R(None), "_detect_light_mode": R(False)})
add("build_tui_style_dict", state={"_tui_style_base": STYLE_BASE},
    io={"_detect_light_mode": RAISE("GeneratorExit", "")})

# ── apply skin style ─────────────────────────────────────────────────────
add("apply_tui_skin_style", state={"_app": None, "_tui_style_base": {"a": "b"}})
add("apply_tui_skin_style", state={"_app": {"style": None}, "_tui_style_base": None})
add("apply_tui_skin_style", state={"_app": {}, "_tui_style_base": {"a": "b"}})
add("apply_tui_skin_style", state={"_app": {"style": None}, "_tui_style_base": {"prompt": ""}},
    io={"_detect_light_mode": R(False), "PTStyle.from_dict": R("STYLE-OBJ"),
        "self._invalidate": R(None)})
add("apply_tui_skin_style", state={"_app": {"style": "old"}, "_tui_style_base": {"prompt": ""}},
    io={"_detect_light_mode": R(False), "PTStyle.from_dict": RAISE("ValueError", "style fail")})
add("apply_tui_skin_style", state={"_app": {"style": None}, "_tui_style_base": {"prompt": ""}},
    io={"import:cli": IMP("ImportError", "no cli")})
add("apply_tui_skin_style", state={"_app": {"style": None}, "_tui_style_base": {"prompt": ""}},
    io={"_detect_light_mode": R(False), "PTStyle.from_dict": R("S"),
        "self._invalidate": RAISE("SystemExit", "bye")})

# ── extra widgets hook ───────────────────────────────────────────────────
add("get_extra_tui_widgets")
add("get_extra_tui_widgets", state={"_agent_running": True})
add("get_extra_tui_widgets", state={"_attached_images": ["a"]})
add("get_extra_tui_widgets", state={"_modern_layout": True})
add("get_extra_tui_widgets", state={"_voice_mode": True})

# ── hint text ────────────────────────────────────────────────────────────
add("tui_hint_text")
add("tui_hint_text", state={"_connection_state": {"phase": "form", "payload": {"deadline_at": 100}}},
    io={"t": R("HINT"), "time.time": R(50)})
add("tui_hint_text", state={"_connection_state": {"phase": None}},
    io={"t": R("HINT")})
add("tui_hint_text", state={"_connection_state": {"phase": "authorized", "payload": {"deadline_at": 0}}},
    io={"t": R("HINT")})
add("tui_hint_text", state={"_connection_state": {"phase": "form", "payload": None}},
    io={"t": R("HINT")})
add("tui_hint_text", state={"_connection_state": {"phase": "form", "payload": {"deadline_at": "abc"}}},
    io={"t": R("HINT")})
add("tui_hint_text", state={"_connection_state": {"phase": "waiting", "payload": {"deadline_at": "100"}}},
    io={"t": R("HINT"), "time.time": R(30)})
add("tui_hint_text", state={"_sudo_state": {"x": 1}, "_sudo_deadline": 20},
    io={"t": R("HINT"), "time.monotonic": R(5)})
add("tui_hint_text", state={"_sudo_state": {"vault_save": {"step": "identifier"}}, "_sudo_deadline": 20},
    io={"t": R("HINT"), "time.monotonic": R(5)})
add("tui_hint_text", state={"_sudo_state": {"vault_code": {"site": "x"}}, "_sudo_deadline": 20},
    io={"t": R("HINT"), "time.monotonic": R(5)})
add("tui_hint_text", state={"_secret_state": {"x": 1}, "_secret_deadline": 3},
    io={"t": R("HINT"), "time.monotonic": R(1)})
add("tui_hint_text", state={"_approval_state": {"x": 1}, "_approval_deadline": 3,
                             "_sudo_state": {"x": 1}, "_sudo_deadline": 9},
    io={"t": R("HINT"), "time.monotonic": R(1)})
add("tui_hint_text", state={"_slash_confirm_state": {"x": 1}, "_slash_confirm_deadline": 3},
    io={"t": R("HINT"), "time.monotonic": R(1)})
add("tui_hint_text", state={"_clarify_state": {"x": 1}, "_clarify_freetext": True,
                             "_clarify_deadline": None},
    io={"t": R("HINT")})
add("tui_hint_text", state={"_clarify_state": {"x": 1}, "_clarify_deadline": 10},
    io={"t": R("HINT"), "time.monotonic": R(20)})
add("tui_hint_text", state={"_command_running": True, "_command_blocks_input": True},
    io={"self._command_spinner_frame": R("|"), "t": R("HINT")})
add("tui_hint_text", state={"_command_running": True},
    io={"self._command_spinner_frame": R("|"), "t": R("HINT")})
add("tui_hint_text", state={"_command_running": True},
    io={"self._command_spinner_frame": R("|"), "t": R(None)})
add("tui_hint_text", state={"_sudo_state": {"x": 1}, "_sudo_deadline": None},
    io={"t": R("HINT"), "time.monotonic": R(5)})
add("tui_hint_text", state={"_sudo_state": {"x": 1}, "_sudo_deadline": 20},
    io={"t": R("HINT"), "time.monotonic": RAISE("SystemExit", "bye")})
add("tui_hint_text", state={"_connection_state": {"phase": "url", "payload": {"deadline_at": 5}}},
    io={"t": R("HINT"), "time.time": R(10)})

# ── placeholder text ─────────────────────────────────────────────────────
add("tui_placeholder_text", state={"_voice_recording": True, "_voice_live_text": "hello"})
add("tui_placeholder_text", state={"_voice_recording": True},
    io={"self._voice_record_key_label": R("Ctrl+B"), "t": R("REC")})
add("tui_placeholder_text", state={"_voice_processing": True}, io={"t": R("TRANS")})
add("tui_placeholder_text", state={"_voice_processing": True, "_voice_live_text": "partial"})
add("tui_placeholder_text", state={"_sudo_state": {"vault_save": {"step": "identifier"}}},
    io={"t": R("USER")})
add("tui_placeholder_text", state={"_sudo_state": {"vault_code": {"site": "x"}}},
    io={"t": R("CODE")})
add("tui_placeholder_text", state={"_sudo_state": {"x": 1}}, io={"t": R("PW")})
add("tui_placeholder_text", state={"_secret_state": {"x": 1}}, io={"t": R("SECRET")})
add("tui_placeholder_text", state={"_approval_state": {"x": 1}})
add("tui_placeholder_text", state={"_slash_confirm_state": {"x": 1}}, io={"t": R("CHOICE")})
add("tui_placeholder_text", state={"_clarify_freetext": True}, io={"t": R("ANSWER")})
add("tui_placeholder_text", state={"_clarify_state": {"x": 1}})
add("tui_placeholder_text", state={"_command_running": True, "_command_status": "working"},
    io={"self._command_spinner_frame": R("|")})
add("tui_placeholder_text", state={"_command_running": True},
    io={"self._command_spinner_frame": R("|"), "t": R("PROC")})
add("tui_placeholder_text", state={"_agent_running": True, "_modern_layout": True,
                                  "busy_input_mode": "steer"}, io={"t": R("MODERN")})
add("tui_placeholder_text", state={"_agent_running": True}, io={"t": R("BUSY")})
add("tui_placeholder_text", state={"_voice_mode": True},
    io={"self._voice_record_key_label": R("Ctrl+B"), "t": R("IDLE")})
add("tui_placeholder_text", state={"_prompt_stash": {}},
    io={"prompt_stash.placeholder_hint": R("parked draft")})
add("tui_placeholder_text", state={"_prompt_stash": {}},
    io={"prompt_stash.placeholder_hint": RAISE("ValueError", "hint fail")})
add("tui_placeholder_text", state={"_prompt_stash": None, "_composer_placeholder": "try /help"})
add("tui_placeholder_text", state={"_prompt_stash": {}},
    io={"prompt_stash.placeholder_hint": RAISE("SystemExit", "bye")})
add("tui_placeholder_text", state={"_composer_placeholder": ""})
add("tui_placeholder_text", state={"_prompt_stash": {}},
    io={"prompt_stash.placeholder_hint": R(None)})
add("tui_placeholder_text", state={"_sudo_state": {"vault_save": "notdict"}})

# ── stash panel display ──────────────────────────────────────────────────
add("get_stash_panel_display_fragments", state={"_prompt_stash": None})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 2}},
    io={"prompt_stash.panel_rows": R([["r1"], ["r2"]]),
        "self._get_tui_terminal_width": R(80),
        "self._render_stash_panel": R([["frag"]])})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 0}},
    io={"prompt_stash.panel_rows": RAISE("ValueError", "rows fail")})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 0}},
    io={"prompt_stash.panel_rows": RAISE("SystemExit", "bye")})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 0}},
    io={"prompt_stash.panel_rows": RAISE("CancelledError", "")})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {}},
    io={"prompt_stash.panel_rows": R([])})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 1}},
    io={"prompt_stash.panel_rows": R([]),
        "self._get_tui_terminal_width": RAISE("RuntimeError", "w fail")})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 1}},
    io={"prompt_stash.panel_rows": R([]), "self._get_tui_terminal_width": R(80),
        "self._render_stash_panel": RAISE("ValueError", "render fail")})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 1}},
    io={"prompt_stash.panel_rows": R([]), "self._get_tui_terminal_width": R(80),
        "self._render_stash_panel": RAISE("GeneratorExit", "")})
add("get_stash_panel_display_fragments", state={"_prompt_stash": {"panel_cursor": 1}},
    io={"prompt_stash.panel_rows": R([]), "self._get_tui_terminal_width": R(80)})

# ── image bar ────────────────────────────────────────────────────────────
add("tui_image_bar_fragments")
add("tui_image_bar_fragments", state={"_attached_images": ["a"], "_image_counter": 3},
    io={"_format_image_attachment_badges": R("📎1")})
add("tui_image_bar_fragments", state={"_attached_images": ["a", "b"], "_image_counter": 0},
    io={"_format_image_attachment_badges": R(None)})
add("tui_image_bar_fragments", state={"_attached_images": ["a"]},
    io={"_format_image_attachment_badges": RAISE("ValueError", "badge fail")})
add("tui_image_bar_fragments", state={"_attached_images": ["a"]},
    io={"import:cli": IMP("ImportError", "no cli")})
add("tui_image_bar_fragments", state={"_attached_images": ["a"], "_image_counter": 1},
    io={"_format_image_attachment_badges": R(5)})
add("tui_image_bar_fragments", state={"_attached_images": [], "_image_counter": 9})

# ── voice status / spinner / hint height delegators ──────────────────────
add("tui_voice_status_fragments", io={"self._get_voice_status_fragments": R([["class:voice-status", "ok"]])})
add("tui_voice_status_fragments", io={"self._get_voice_status_fragments": R(None)})
add("tui_voice_status_fragments", io={"self._get_voice_status_fragments": R([])})
add("tui_voice_status_fragments", io={"self._get_voice_status_fragments": RAISE("ValueError", "vs")})
add("tui_voice_status_fragments", io={"self._get_voice_status_fragments": RAISE("SystemExit", "bye")})
add("tui_voice_status_fragments", io={"self._get_voice_status_fragments": RAISE("CancelledError", "")})
add("tui_voice_status_fragments")

add("tui_spinner_text", io={"self._render_spinner_text": R("spin")})
add("tui_spinner_text", io={"self._render_spinner_text": R("")})
add("tui_spinner_text", io={"self._render_spinner_text": R(None)})
add("tui_spinner_text", io={"self._render_spinner_text": RAISE("RuntimeError", "sp")})
add("tui_spinner_text", io={"self._render_spinner_text": RAISE("KeyboardInterrupt", "")})
add("tui_spinner_text", io={"self._render_spinner_text": R(0)})

add("tui_spinner_height", io={"self._spinner_widget_height": R(2)})
add("tui_spinner_height", io={"self._spinner_widget_height": R(0)})
add("tui_spinner_height", io={"self._spinner_widget_height": R(None)})
add("tui_spinner_height", io={"self._spinner_widget_height": RAISE("ValueError", "h")})
add("tui_spinner_height", io={"self._spinner_widget_height": RAISE("SystemExit", "bye")})

add("tui_hint_height", io={"self._agent_spacer_height": R(1)})
add("tui_hint_height", state={"_sudo_state": {"x": 1}})
add("tui_hint_height", state={"_secret_state": {"x": 1}})
add("tui_hint_height", state={"_approval_state": {"x": 1}})
add("tui_hint_height", state={"_connection_state": {"x": 1}})
add("tui_hint_height", state={"_slash_confirm_state": {"x": 1}})
add("tui_hint_height", state={"_clarify_state": {"x": 1}})
add("tui_hint_height", state={"_command_running": True})
add("tui_hint_height", io={"self._agent_spacer_height": R(0)})
add("tui_hint_height", io={"self._agent_spacer_height": RAISE("SystemExit", "bye")})

# ── Tab completion ───────────────────────────────────────────────────────
add("handle_tab", state=buf("abc", complete_state={"current_completion": "cmp1"}),
    io={"buffer.apply_completion": R(None)})
add("handle_tab", state=buf("abc", complete_state={"current_completion": None}),
    io={"buffer.go_to_completion": EFF(None, buf={"complete_state": {"current_completion": "cmp2"}}),
        "buffer.apply_completion": R(None)})
add("handle_tab", state=buf("abc", complete_state={"current_completion": None}),
    io={"buffer.go_to_completion": R(None)})
add("handle_tab", state=buf("abc", complete_state={"current_completion": None}),
    io={"buffer.go_to_completion": EFF(None, buf={"complete_state": None})})
add("handle_tab", state=buf("abc", suggestion={"text": "ghost"}))
add("handle_tab", state=buf("abc", suggestion={"text": ""}),
    io={"buffer.start_completion": R(None)})
add("handle_tab", state=buf("abc"), io={"buffer.start_completion": R(None)})
add("handle_tab", state=buf("abc", complete_state={"current_completion": "cmp1"}),
    io={"buffer.apply_completion": RAISE("ValueError", "apply fail")})
add("handle_tab", state=buf("abc", complete_state={"current_completion": None}),
    io={"buffer.go_to_completion": RAISE("SystemExit", "bye")})
add("handle_tab", state=buf("abc"), io={"buffer.start_completion": RAISE("RuntimeError", "sc")})
add("handle_tab", state=buf("abc", complete_state={}))
add("handle_tab", state=buf("héllo", 2, suggestion={"text": " wörld"}))

# ── open palette ─────────────────────────────────────────────────────────
add("open_command_palette", io={"self._open_command_palette": R(None)})
add("open_command_palette", io={"self._open_command_palette": RAISE("ValueError", "open")})
add("open_command_palette", io={"self._open_command_palette": RAISE("SystemExit", "bye")})
add("open_command_palette", io={"self._open_command_palette": R(True)})
add("open_command_palette")

# ── stash panel restore ──────────────────────────────────────────────────
add("handle_stash_panel_restore", state={"_prompt_stash": None})
add("handle_stash_panel_restore", state={"_prompt_stash": {}},
    io={"prompt_stash.restore_at_cursor": R(["txt", []]),
        "self._tui_restore_stash_payload": R(None)})
add("handle_stash_panel_restore", state={"_prompt_stash": {}},
    io={"prompt_stash.restore_at_cursor": R(None),
        "self._tui_restore_stash_payload": R(None)})
add("handle_stash_panel_restore", state={"_prompt_stash": {}},
    io={"prompt_stash.restore_at_cursor": RAISE("ValueError", "restore fail")})
add("handle_stash_panel_restore", state={"_prompt_stash": {}},
    io={"prompt_stash.restore_at_cursor": R(["t", ["i"]]),
        "self._tui_restore_stash_payload": RAISE("SystemExit", "bye")})
add("handle_stash_panel_restore", state={"_prompt_stash": {}},
    io={"prompt_stash.restore_at_cursor": R(["t", []]),
        "self._tui_restore_stash_payload": RAISE("RuntimeError", "payload fail")})
add("handle_stash_panel_restore", state={"_prompt_stash": {}},
    io={"self._tui_restore_stash_payload": R(None)})

# ── clarify number handler ───────────────────────────────────────────────
CLAR = {"choices": ["red", "blue"], "selected": 0, "active": 0,
        "questions": [{"qid": "q1"}], "answers": {}}
add("make_clarify_number_handler", state={"_clarify_state": None}, idx=0)
add("make_clarify_number_handler", state={"_clarify_state": CLAR, "_clarify_freetext": True}, idx=0)
add("make_clarify_number_handler", state={"_clarify_state": CLAR}, idx=5)
add("make_clarify_number_handler", state={"_clarify_state": CLAR}, idx=2)
add("make_clarify_number_handler", state={"_clarify_state": CLAR}, idx=0,
    io={"self._clarify_batch_lock": R(None)})
add("make_clarify_number_handler",
    state={"_clarify_state": dict(CLAR, multi_select=True, selected_indices=[0, 2])}, idx=2)
add("make_clarify_number_handler",
    state={"_clarify_state": dict(CLAR, multi_select=True)}, idx=1)
add("make_clarify_number_handler",
    state={"_clarify_state": dict(CLAR, multi_select=True, selected_indices=[0])}, idx=2)
add("make_clarify_number_handler", state={"_clarify_state": CLAR}, idx=1,
    io={"self._clarify_batch_lock": RAISE("ValueError", "lock fail")})
add("make_clarify_number_handler", state={"_clarify_state": CLAR}, idx=0,
    io={"self._clarify_batch_lock": RAISE("SystemExit", "bye")})
add("make_clarify_number_handler", state={"_clarify_state": {"selected": 0}}, idx=0)

# ── model filter char handler ────────────────────────────────────────────
add("make_model_filter_char_handler", state={"_model_picker_state": None}, ch="a")
add("make_model_filter_char_handler", state={"_model_picker_state": {"stage": "provider"}}, ch="a")
add("make_model_filter_char_handler",
    state={"_model_picker_state": {"stage": "model", "filter": "ab", "selected": 3, "_scroll_offset": 2}}, ch="c")
add("make_model_filter_char_handler", state={"_model_picker_state": {"stage": "model"}}, ch="x")
add("make_model_filter_char_handler", state={"_model_picker_state": {"stage": "model", "filter": None}}, ch="y")
add("make_model_filter_char_handler", state={"_model_picker_state": {}}, ch="z")
add("make_model_filter_char_handler", state={"_model_picker_state": {"filter": "q"}}, ch="w")

# ── palette char handler ─────────────────────────────────────────────────
add("make_palette_char_handler", state={"_command_palette_state": None}, ch="a")
add("make_palette_char_handler",
    state={"_command_palette_state": {"filter": "ab", "selected": 3, "_scroll_offset": 2}}, ch="c")
add("make_palette_char_handler", state={"_command_palette_state": {}}, ch="x")
add("make_palette_char_handler", state={"_command_palette_state": {"filter": None}}, ch="y")
add("make_palette_char_handler", state={"_command_palette_state": {"filter": "héllo"}}, ch="!")
add("make_palette_char_handler", state={"_command_palette_state": {"selected": 1}}, ch="z")

# ── approval number handler ──────────────────────────────────────────────
add("make_approval_number_handler", state={"_approval_state": None}, idx=0)
add("make_approval_number_handler", state={"_approval_state": {"choices": ["once", "deny"], "selected": 1}},
    idx=0, io={"self._handle_approval_selection": R(None)})
add("make_approval_number_handler", state={"_approval_state": {"choices": ["once", "deny"], "selected": 0}},
    idx=1, io={"self._handle_approval_selection": R(None)})
add("make_approval_number_handler", state={"_approval_state": {"choices": ["once", "deny"], "selected": 0}},
    idx=2)
add("make_approval_number_handler", state={"_approval_state": {"selected": 0}}, idx=0)
add("make_approval_number_handler", state={"_approval_state": {"choices": ["once"], "selected": 0}},
    idx=0, io={"self._handle_approval_selection": RAISE("ValueError", "sel fail")})
add("make_approval_number_handler", state={"_approval_state": {"choices": ["once"], "selected": 0}},
    idx=0, io={"self._handle_approval_selection": RAISE("SystemExit", "bye")})

# ── slash-confirm number handler ─────────────────────────────────────────
SLASH = {"choices": [["a", "A", "desc"], ["b", "B", "d2"]], "selected": 0}
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": None}, idx=0)
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": SLASH}, idx=0,
    io={"self._submit_slash_confirm_response": R(None)})
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": SLASH}, idx=1,
    io={"self._submit_slash_confirm_response": R(None)})
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": SLASH}, idx=2)
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": {"selected": 0}}, idx=0)
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": {"choices": [[]]}}, idx=0)
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": {"choices": [""]}}, idx=0)
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": {"choices": ["xy"]}}, idx=0,
    io={"self._submit_slash_confirm_response": R(None)})
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": SLASH}, idx=0,
    io={"self._submit_slash_confirm_response": RAISE("ValueError", "submit fail")})
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": SLASH}, idx=0,
    io={"self._submit_slash_confirm_response": RAISE("SystemExit", "bye")})
add("make_slash_confirm_number_handler", state={"_slash_confirm_state": {"choices": [5]}}, idx=0)

# ── Ctrl+Z suspend ───────────────────────────────────────────────────────
add("handle_ctrl_z", platform="win32", io={"t": R("UNSUP"), "_cprint": R(None)})
add("handle_ctrl_z", platform="win32", io={"import:cli": IMP("ImportError", "no cli")})
add("handle_ctrl_z", platform="win32", io={"t": RAISE("SystemExit", "bye")})
add("handle_ctrl_z", platform="win32", io={"t": R("UNSUP"), "_cprint": RAISE("ValueError", "print fail")})
add("handle_ctrl_z", platform="win32", io={"t": R(None), "_cprint": R(None)})
LINUX_IO = {"get_active_skin": R({}), "get_branding": R("Jarvis"),
            "t": R("SUSP"), "run_in_terminal": R(None)}
add("handle_ctrl_z", platform="linux", io=LINUX_IO)
add("handle_ctrl_z", platform="linux", io={"import:signal": IMP("ImportError", "no signal")})
add("handle_ctrl_z", platform="linux",
    io={"import:prompt_toolkit.application": IMP("ImportError", "no pt")})
add("handle_ctrl_z", platform="linux",
    io={"import:hermes_cli.skin_engine": IMP("ImportError", "no skin")})
add("handle_ctrl_z", platform="linux", io=dict(LINUX_IO, get_active_skin=R(None)))
add("handle_ctrl_z", platform="linux", io=dict(LINUX_IO, get_branding=RAISE("ValueError", "brand fail")))
add("handle_ctrl_z", platform="linux", io=dict(LINUX_IO, t=R(None)))
add("handle_ctrl_z", platform="linux", io=dict(LINUX_IO, run_in_terminal=RAISE("SystemExit", "bye")))
add("handle_ctrl_z", platform="linux", io=dict(LINUX_IO, get_active_skin=RAISE("KeyboardInterrupt", "")))


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
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