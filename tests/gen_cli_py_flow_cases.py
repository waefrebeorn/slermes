#!/usr/bin/env python3
"""Generate tests/fixtures/cli_py_flow.jsonl — differential cases for the
unclaimed quarry ROOT cli.py FLOW ports (src/cli/cli_py_flow.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, MISSING script is
fail-closed RuntimeError on both sides). Output seams default to null; _t
defaults to the rendered string "T".

Domain (see the port header): ASCII inputs; painted history texts unique per
case; str()-computed positions carry integral ints only; line/seam list
results are wrapped one level deeper than per-call queues; bundle/skill info
objects carry "name"; auto/preload results are 3-arrays; split results are
2-arrays; fit is a 4-array or null; subprocess results carry stdout/stderr.

Run:  python tests/gen_cli_py_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "cli_py_flow.jsonl")

OUT_SEAMS = {"print": None, "cprint": None, "console_print": None,
             "ChatConsole.print": None, "t": "T"}

# shared scripts for the slash/skill dispatch cluster
DISPATCH_SEAMS = {
    "get_skill_bundles": {},
    "gateway_plugin_commands.lookup": None,
    "gateway_plugin_commands.skills": None,
    "get_interactive_skill_commands": {},
    "get_plugin_cmd_handler_names": [[]],
    "gateway_plugin_commands.run": None,
    "gateway_plugin_commands.run_skill": None,
    "gateway_plugin_commands.report_gone": False,
    "process_command": True,
    "COMMANDS": [["/help", "/quit", "/model", "/skills"]],
    "unknown_command_lines": [["Unknown command.", "Did you mean /help?"]],
    "split_stacked_skill_commands": [[[], "do it"]],
    "build_skill_invocation_message": "SKILLMSG",
    "build_bundle_invocation_message": [["BUNDLEMSG", ["alpha"], []]],
    "build_stacked_skill_invocation_message": [["STACKMSG", ["alpha", "beta"], []]],
    "format_missing_skills": "missing skills: x",
    "logger.warning": None,
    "pending_input.put": None,
    "build_subprocess_env": {"PATH": "/bin"},
    "windows_hide_flags": 0,
    "subprocess.run": {"stdout": "out", "stderr": ""},
    "redact_sensitive_text": "redacted",
    "rich_text_from_ansi": {"__rich__": True, "text": "redacted"},
    "get_plugin_command_handler": None,
    "invoke_plugin_command": "raw",
    "resolve_plugin_command_result": "result",
    "clarify_asker": {"asker": 1},
    "bind_asker.enter": None,
    "bind_asker.exit": None,
    "preload_thread.join": None,
    "set_sudo_password_callback": None,
    "set_approval_callback": None,
    "set_secret_capture_callback": None,
    "set_unlock_prompt_callback": None,
    "set_save_login_prompt_callback": None,
    "set_code_prompt_callback": None,
}

HIST_SEAMS = {
    "coerce_output_history_limit": 200,
    "output_history_lines": [["line one", "line two"]],
    "output_tail_fitting": [["line two"]],
    "pt_print": None,
    "paint_formatted_text": None,
    "output.get_size": {"rows": 24, "columns": 80},
    "line_rows": 1,
    "set_chrome_floor": None,
    "add_suspect_rows": None,
    "painted_columns": 80,
    "suspend_body": None,
    "get_interactive_skill_commands": {},
    "gateway_plugin_commands.skills": None,
}

SELF_BASE = {
    "session_id": "sess-1",
    "_pending_input": {"__queue__": True},
    "config": {"quick_commands": {}},
}


def case(func, seams=None, **inp):
    s = dict(OUT_SEAMS)
    s.update(seams or {})
    d = dict(inp)
    d["seams"] = s
    return {"func": func, "in": d}


def dcase(func, seams=None, self=None, **inp):
    s = dict(OUT_SEAMS)
    s.update(DISPATCH_SEAMS)
    s.update(seams or {})
    d = dict(inp)
    d["self"] = dict(SELF_BASE) if self is None else self
    d["seams"] = s
    return {"func": func, "in": d}


def hcase(func, seams=None, state=None, **inp):
    s = dict(OUT_SEAMS)
    s.update(HIST_SEAMS)
    s.update(seams or {})
    d = dict(inp)
    d["state"] = {"enabled": True, "max_lines": 200, "replaying": False,
                  "suppressed": False, "history": ["old"], "skill_commands": None}
    if state is not None:
        d["state"].update(state)
    d["seams"] = s
    return {"func": func, "in": d}


def bare(func, seams=None, **inp):
    d = dict(inp)
    d["seams"] = dict(seams or {})
    return {"func": func, "in": d}


CASES = []

# ── _configure_output_history ──
CASES += [
    hcase("configure_output_history", enabled=True, max_lines=50),
    hcase("configure_output_history", enabled=False, max_lines=200),
    hcase("configure_output_history", {"coerce_output_history_limit": 10}, enabled=1, max_lines=3),
    hcase("configure_output_history", {"coerce_output_history_limit": {"__raise__": "ValueError", "msg": "bad"}},
          enabled=True, max_lines="x"),
    hcase("configure_output_history", {"coerce_output_history_limit": {"__raise__": "SystemExit", "msg": "1"}},
          enabled=True, max_lines=5),
    bare("configure_output_history", enabled=True, max_lines=5,
         state={"enabled": False, "max_lines": 9, "history": ["h"]}),   # fail-closed coerce
]

# ── _suspend_output_history ──
CASES += [
    hcase("suspend_output_history", state={"suppressed": False}),
    hcase("suspend_output_history", state={"suppressed": True}),
    hcase("suspend_output_history", {"suspend_body": {"__raise__": "ValueError", "msg": "body"}},
          state={"suppressed": False}),
    hcase("suspend_output_history", {"suspend_body": {"__raise__": "KeyboardInterrupt", "msg": ""}},
          state={"suppressed": False}),
    hcase("suspend_output_history", {"suspend_body": "val"}, state={"suppressed": False}),
    bare("suspend_output_history", state={"suppressed": False}),   # fail-closed body
]

# ── _replay_output_history ──
PL = {"__painted__": True, "text": "painted line", "width": 40}
CASES += [
    hcase("replay_output_history"),
    hcase("replay_output_history", state={"enabled": False}),
    hcase("replay_output_history", state={"history": []}),
    hcase("replay_output_history", fit=[10, 80, True, 2]),
    hcase("replay_output_history", fit=[10, 80, True, None]),
    hcase("replay_output_history", fit=[10, 80, True, 2], output={"rows": 24, "columns": 80}),
    hcase("replay_output_history", fit=[10, 60, True, 2], output={"rows": 24, "columns": 80},
          seams={"line_rows": [2, 3]}),
    hcase("replay_output_history", output={"rows": 24, "columns": 80}),
    hcase("replay_output_history", state={"history": [PL]},
          seams={"output_history_lines": [[PL]], "painted_columns": 100}),
    hcase("replay_output_history", fit=[10, 80, True, 2], state={"history": [PL]},
          seams={"output_history_lines": [[PL]], "output_tail_fitting": [[PL]]}),
    hcase("replay_output_history", seams={"output_history_lines": [[]]}),
    hcase("replay_output_history", seams={"output_history_lines": {"__raise__": "ValueError", "msg": "x"}}),
    hcase("replay_output_history", seams={"output_history_lines": {"__raise__": "SystemExit", "msg": "1"}}),
    hcase("replay_output_history", fit=[10, 80], seams={"output_history_lines": [["a"]]}),  # unpack err swallowed
    hcase("replay_output_history", output={"rows": 24, "columns": 80},
          seams={"output.get_size": {"__raise__": "RuntimeError", "msg": "size"}}),
    hcase("replay_output_history", output={"rows": 24, "columns": 80}, fit=[10, 80, True, 2],
          seams={"output.get_size": {"columns": 80}}),   # missing rows -> swallowed attr err
    bare("replay_output_history"),   # fail-closed output_history_lines
]

# ── _looks_like_slash_command ──
CASES += [
    case("looks_like_slash_command", text="/help"),
    case("looks_like_slash_command", text="/help me now"),
    case("looks_like_slash_command", text="/Users/x/file.md"),
    case("looks_like_slash_command", text="hello"),
    case("looks_like_slash_command", text=""),
    case("looks_like_slash_command", text=None),
    case("looks_like_slash_command", text="/ "),          # IndexError
    case("looks_like_slash_command", text="/a/b rest"),
    case("looks_like_slash_command", text=5),             # AttributeError startswith
]

# ── _slash_args ──
CASES += [
    case("slash_args", cmd="/help"),
    case("slash_args", cmd="/help   me  now "),
    case("slash_args", cmd="  /help  arg arg  "),
    case("slash_args", cmd=""),
    case("slash_args", cmd="   "),
    case("slash_args", cmd="/help\t\nrest"),
    case("slash_args", cmd=None),
]

# ── _ensure_skill_commands ──
CASES += [
    hcase("ensure_skill_commands", state={"skill_commands": {"/a": {"name": "a"}}}),
    hcase("ensure_skill_commands", state={"skill_commands": None}),
    hcase("ensure_skill_commands", {"get_interactive_skill_commands": {"/b": {"name": "b"}}},
          state={"skill_commands": None}),
    hcase("ensure_skill_commands", {"get_interactive_skill_commands": {"__raise__": "ValueError", "msg": "x"}},
          state={"skill_commands": None}),
    hcase("ensure_skill_commands", {"get_interactive_skill_commands": {"__raise__": "SystemExit", "msg": "1"}},
          state={"skill_commands": None}),
    bare("ensure_skill_commands", state={"skill_commands": None}),   # fail-closed
]

# ── _session_skill_commands ──
CASES += [
    hcase("session_skill_commands", {"gateway_plugin_commands.skills": {"/g": {"name": "g"}}}),
    hcase("session_skill_commands", {"gateway_plugin_commands.skills": None}),
    hcase("session_skill_commands", {"gateway_plugin_commands.skills": None,
                                     "get_interactive_skill_commands": {"/l": {"name": "l"}}},
          state={"skill_commands": None}),
    hcase("session_skill_commands", state={"skill_commands": {"/c": {"name": "c"}}}),
    hcase("session_skill_commands", {"gateway_plugin_commands.skills": {"__raise__": "SystemExit", "msg": "1"}}),
    bare("session_skill_commands"),   # fail-closed skills
]

# ── _should_seed_interactive ──
CASES += [
    case("should_seed_interactive", {"stdin.isatty": True, "stdout.isatty": True}, query="q", image=None),
    case("should_seed_interactive", {"stdin.isatty": False, "stdout.isatty": True}, query="q"),
    case("should_seed_interactive", {"stdin.isatty": True, "stdout.isatty": False}, query="q"),
    case("should_seed_interactive", query=None, image=None),
    case("should_seed_interactive", query="q", oneshot=True),
    case("should_seed_interactive", query="q", quiet=True),
    case("should_seed_interactive", {"stdin.isatty": {"__raise__": "ValueError", "msg": "x"}}, query="q"),
    case("should_seed_interactive", {"stdin.isatty": {"__raise__": "SystemExit", "msg": "1"}}, query="q"),
    case("should_seed_interactive", {"stdin.isatty": True, "stdout.isatty": {"__raise__": "OSError", "msg": "x"}},
         query="q"),
    bare("should_seed_interactive", query="q"),   # fail-closed stdin
]

# ── sentinel __str__ leaves ──
CASES += [
    case("voice_input_message_str", self={"text": "hello"}),
    case("voice_input_message_str", self={"text": ""}),
    case("voice_input_message_str", self={"text": "a b c"}),
    case("voice_input_message_str", self={}),                 # ctor TypeError
    case("voice_input_message_str", self={"text": None}),     # __str__ non-string
    case("seeded_query_message_str", self={"text": "hello"}),
    case("seeded_query_message_str", self={"text": None}),    # text or "" -> ""
    case("seeded_query_message_str", self={"text": ""}),
    case("seeded_query_message_str", self={"text": "q", "images": ["i"], "admission_id": "a"}),
    case("seeded_query_message_str", self={}),                # ctor TypeError
]

# ── _install_tool_callbacks ──
CB_SELF = {
    "_tool_callbacks_installed": False,
    "_sudo_password_callback": "cb:sudo", "_approval_callback": "cb:approval",
    "_secret_capture_callback": "cb:secret", "_vault_unlock_callback": "cb:unlock",
    "_vault_save_login_callback": "cb:save", "_vault_code_callback": "cb:code",
}
CASES += [
    dcase("install_tool_callbacks", self=dict(CB_SELF)),
    dcase("install_tool_callbacks", self=dict(CB_SELF, _tool_callbacks_installed=True)),
    dcase("install_tool_callbacks", {"set_approval_callback": {"__raise__": "ValueError", "msg": "x"}},
          self=dict(CB_SELF)),
    dcase("install_tool_callbacks", {"set_sudo_password_callback": {"__raise__": "KeyboardInterrupt", "msg": ""}},
          self=dict(CB_SELF)),
    dcase("install_tool_callbacks", self={k: v for k, v in CB_SELF.items() if k != "_vault_code_callback"}),
    dcase("install_tool_callbacks", self={"_tool_callbacks_installed": False}),   # missing cb attr
    bare("install_tool_callbacks", self=dict(CB_SELF)),   # fail-closed setter
]

# ── _show_security_advisories ──
SEC_SEAMS = {"detect_compromised": [], "startup_banner": "BANNER"}
CASES += [
    case("show_security_advisories", SEC_SEAMS),
    case("show_security_advisories", dict(SEC_SEAMS, startup_banner=None)),
    case("show_security_advisories", dict(SEC_SEAMS, startup_banner="")),
    case("show_security_advisories", dict(SEC_SEAMS, detect_compromised={"__raise__": "ValueError", "msg": "x"})),
    case("show_security_advisories", dict(SEC_SEAMS, startup_banner={"__raise__": "OSError", "msg": "x"})),
    case("show_security_advisories", dict(SEC_SEAMS, detect_compromised={"__raise__": "SystemExit", "msg": "1"})),
    case("show_security_advisories", dict(SEC_SEAMS, print={"__raise__": "ValueError", "msg": "x"})),
    case("show_security_advisories", SEC_SEAMS, env={"security_import_ok": False}),
    bare("show_security_advisories"),   # fail-closed detect
]

# ── finalize_preloaded_skills ──
FIN_SELF = {
    "_preload_skills_finalized": False,
    "_preload_skills_thread": {"__thread__": True},
    "_preload_skills_error": None,
    "_auto_load_skills_result": ["prompt", ["auto1"], []],
    "_preload_skills_result": ["SKILLS", ["s1", "s2"], []],
    "system_prompt": "base",
    "preloaded_skills": [],
}
CASES += [
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF)),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _preload_skills_finalized=True)),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _preload_skills_thread=None)),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _preload_skills_error={"__raise__": "ValueError", "msg": "all unknown"})),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _auto_load_skills_result=["p", ["a"], ["miss1", "miss2"]])),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _preload_skills_result=["S", ["s1"], ["m1"]])),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _preload_skills_result=["S", [], ["m1"]])),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _preload_skills_result=None)),
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, _preload_skills_result=["S", ["s1"]]), ),  # unpack err
    dcase("finalize_preloaded_skills", self=dict(FIN_SELF, system_prompt=None)),
    dcase("finalize_preloaded_skills", {"preload_thread.join": {"__raise__": "SystemExit", "msg": "1"}},
          self=dict(FIN_SELF)),
    dcase("finalize_preloaded_skills", {"format_missing_skills": {"__raise__": "RuntimeError", "msg": "fm"}},
          self=dict(FIN_SELF, _preload_skills_result=["S", [], ["m"]])),
    bare("finalize_preloaded_skills", self=dict(FIN_SELF)),   # fail-closed join
]

# ── _slash_handler ──
DISPATCH = {"exit": ["_cmd_exit", True], "help": ["_cmd_help", True],
            "palette": ["_open_command_palette", False]}
CASES += [
    case("slash_handler", dispatch=DISPATCH, handlers=[], canonical="exit"),
    case("slash_handler", dispatch=DISPATCH, handlers=[], canonical="palette"),
    case("slash_handler", dispatch=DISPATCH, handlers=["_handle_foo_command"], canonical="foo"),
    case("slash_handler", dispatch=DISPATCH, handlers=["_handle_foo_bar_command"], canonical="foo-bar"),
    case("slash_handler", dispatch=DISPATCH, handlers=[], canonical="foo"),
    case("slash_handler", dispatch={}, handlers=[], canonical="exit"),
]

# ── _queue_skill_message ──
CASES += [
    dcase("queue_skill_message", msg={"text": "m"}),
    dcase("queue_skill_message", msg="plain"),
    dcase("queue_skill_message", self={"session_id": "s"}, msg="m"),          # absent _pending_input
    dcase("queue_skill_message", self=dict(SELF_BASE, _pending_input=None), msg="m"),  # NoneType.put
    dcase("queue_skill_message", {"pending_input.put": {"__raise__": "ValueError", "msg": "x"}}, msg="m"),
    dcase("queue_skill_message", {"pending_input.put": {"__raise__": "SystemExit", "msg": "1"}}, msg="m"),
    bare("queue_skill_message", msg="m"),   # fail-closed put
]

# ── _queue_loaded_skills ──
CASES += [
    dcase("queue_loaded_skills", msg="M", label="loading", missing=None),
    dcase("queue_loaded_skills", msg="M", label="loading", missing=["m1", "m2"]),
    dcase("queue_loaded_skills", msg="M", label="loading", missing=[]),
    dcase("queue_loaded_skills", {"print": {"__raise__": "SystemExit", "msg": "1"}}, msg="M", label="L"),
    dcase("queue_loaded_skills", {"t": {"__raise__": "ValueError", "msg": "t"}}, msg="M", label="L", missing=["m"]),
    bare("queue_loaded_skills", msg="M", label="L"),   # fail-closed print
]

# ── _run_skill_bundle_command ──
CASES += [
    dcase("run_skill_bundle_command", base_cmd="/bun", bundle_info={"name": "Bun"}, user_instruction="go"),
    dcase("run_skill_bundle_command", {"build_bundle_invocation_message": None},
          base_cmd="/bun", bundle_info={"name": "Bun"}, user_instruction="go"),
    dcase("run_skill_bundle_command", {"build_bundle_invocation_message": [["M", ["a", "b"], ["miss"]]]},
          base_cmd="/bun", bundle_info={"name": "Bun"}, user_instruction="go"),
    dcase("run_skill_bundle_command", base_cmd="/bun", bundle_info={}, user_instruction="go"),  # KeyError name
    dcase("run_skill_bundle_command", {"build_bundle_invocation_message": {"__raise__": "SystemExit", "msg": "1"}},
          base_cmd="/bun", bundle_info={"name": "B"}, user_instruction=""),
    dcase("run_skill_bundle_command", self={"_pending_input": {"__queue__": True}},
          base_cmd="/bun", bundle_info={"name": "B"}, user_instruction=""),   # missing session_id
    bare("run_skill_bundle_command", base_cmd="/bun", bundle_info={"name": "B"}, user_instruction=""),
]

# ── _run_skill_slash_command ──
CASES += [
    dcase("run_skill_slash_command", base_cmd="/sk", skill_info={"name": "Sk"}, rest="do it"),
    dcase("run_skill_slash_command", {"gateway_plugin_commands.run_skill": True},
          base_cmd="/sk", skill_info={"name": "Sk"}, rest=""),
    dcase("run_skill_slash_command", {"split_stacked_skill_commands": [[["/sk2"], "stack it"]]},
          base_cmd="/sk", skill_info={"name": "Sk"}, rest="/sk2 stack it"),
    dcase("run_skill_slash_command", {"split_stacked_skill_commands": [[["/sk2"], "x"]],
                                      "build_stacked_skill_invocation_message": None},
          base_cmd="/sk", skill_info={"name": "Sk"}, rest="/sk2 x"),
    dcase("run_skill_slash_command", {"build_skill_invocation_message": None},
          base_cmd="/sk", skill_info={"name": "Sk"}, rest=""),
    dcase("run_skill_slash_command", base_cmd="/sk", skill_info={}, rest=""),   # KeyError name
    dcase("run_skill_slash_command", {"gateway_plugin_commands.run_skill": {"__raise__": "SystemExit", "msg": "1"}},
          base_cmd="/sk", skill_info={"name": "S"}, rest=""),
    bare("run_skill_slash_command", base_cmd="/sk", skill_info={"name": "S"}, rest=""),
]

# ── _run_quick_command ──
CASES += [
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "alias", "target": "help"}, user_args="me"),
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "alias", "target": "/help"}, user_args=""),
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "alias", "target": "  "}, user_args=""),
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "alias"}, user_args=""),
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "bogus"}, user_args=""),
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "exec", "command": "echo hi"}, user_args=""),
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "exec", "command": ""}, user_args=""),
    dcase("run_quick_command", base_cmd="/q", qcmd={"type": "exec"}, user_args=""),
    dcase("run_quick_command", {"subprocess.run": {"stdout": "", "stderr": "err"}},
          base_cmd="/q", qcmd={"type": "exec", "command": "x"}, user_args=""),
    dcase("run_quick_command", {"subprocess.run": {"stdout": "  ", "stderr": ""}},
          base_cmd="/q", qcmd={"type": "exec", "command": "x"}, user_args=""),
    dcase("run_quick_command", {"subprocess.run": {"__raise__": "TimeoutExpired", "msg": "timed out"}},
          base_cmd="/q", qcmd={"type": "exec", "command": "x"}, user_args=""),
    dcase("run_quick_command", {"subprocess.run": {"__raise__": "OSError", "msg": "boom"}},
          base_cmd="/q", qcmd={"type": "exec", "command": "x"}, user_args=""),
    dcase("run_quick_command", {"subprocess.run": {"__raise__": "SystemExit", "msg": "1"}},
          base_cmd="/q", qcmd={"type": "exec", "command": "x"}, user_args=""),
    dcase("run_quick_command", {"subprocess.run": {}},
          base_cmd="/q", qcmd={"type": "exec", "command": "x"}, user_args=""),   # missing stdout
    dcase("run_quick_command", {"process_command": {"__raise__": "ValueError", "msg": "pc"}},
          base_cmd="/q", qcmd={"type": "alias", "target": "help"}, user_args=""),
    bare("run_quick_command", base_cmd="/q", qcmd={"type": "exec", "command": "x"}, user_args=""),
]

# ── _run_plugin_slash_command ──
PL_SELF = dict(SELF_BASE, _clarify_callback="cb:clarify")
CASES += [
    dcase("run_plugin_slash_command", base_cmd="/plug", user_args="a", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"gateway_plugin_commands.run": "ANSWERED"},
          base_cmd="/plug", user_args="", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"gateway_plugin_commands.run": {"__gone__": True}},
          base_cmd="/plug", user_args="", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"get_plugin_command_handler": {"h": 1},
                                       "resolve_plugin_command_result": "RES"},
          base_cmd="/plug", user_args="a", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"get_plugin_command_handler": {"h": 1},
                                       "resolve_plugin_command_result": None},
          base_cmd="/plug", user_args="", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"get_plugin_command_handler": {"h": 1},
                                       "invoke_plugin_command": {"__raise__": "ValueError", "msg": "inv"}},
          base_cmd="/plug", user_args="", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"get_plugin_command_handler": {"h": 1},
                                       "bind_asker.exit": {"__raise__": "OSError", "msg": "exit"}},
          base_cmd="/plug", user_args="", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"get_plugin_command_handler": {"h": 1},
                                       "invoke_plugin_command": {"__raise__": "SystemExit", "msg": "1"}},
          base_cmd="/plug", user_args="", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", {"get_plugin_command_handler": {"h": 1},
                                       "clarify_asker": {"__raise__": "ValueError", "msg": "ca"}},
          base_cmd="/plug", user_args="", self=dict(PL_SELF)),
    dcase("run_plugin_slash_command", base_cmd="/plug", user_args="",
          self={k: v for k, v in PL_SELF.items() if k != "_clarify_callback"}),
    bare("run_plugin_slash_command", base_cmd="/plug", user_args="", self=dict(PL_SELF)),
]

# ── _process_unregistered_slash ──
CASES += [
    dcase("process_unregistered_slash", cmd_original="/help me", cmd_lower="/help me"),
    dcase("process_unregistered_slash", {"get_skill_bundles": {"/bun": {"name": "B"}}},
          cmd_original="/bun go", cmd_lower="/bun go"),
    dcase("process_unregistered_slash", {"gateway_plugin_commands.skills": {"/sk": {"name": "S"}}},
          cmd_original="/sk do", cmd_lower="/sk do"),
    dcase("process_unregistered_slash", {},
          cmd_original="/q me", cmd_lower="/q me",
          self=dict(SELF_BASE, config={"quick_commands": {"q": {"type": "alias", "target": "help"}}})),
    dcase("process_unregistered_slash",
          {"gateway_plugin_commands.lookup": {"__raise__": "CatalogUnavailable", "msg": "catalog"}},
          cmd_original="/x", cmd_lower="/x"),
    dcase("process_unregistered_slash",
          {"get_plugin_cmd_handler_names": [["x"]], "gateway_plugin_commands.run": "ANSWERED"},
          cmd_original="/x a", cmd_lower="/x a"),
    dcase("process_unregistered_slash",
          {"get_plugin_cmd_handler_names": [["x"]], "gateway_plugin_commands.run": {"__gone__": True},
           "gateway_plugin_commands.report_gone": True},
          cmd_original="/x", cmd_lower="/x"),
    dcase("process_unregistered_slash",
          {"get_plugin_cmd_handler_names": [["x"]], "gateway_plugin_commands.run": {"__gone__": True},
           "gateway_plugin_commands.report_gone": False},
          cmd_original="/x", cmd_lower="/x"),
    dcase("process_unregistered_slash", {"gateway_plugin_commands.lookup": {"plugin": 1},
                                         "gateway_plugin_commands.run": None},
          cmd_original="/x", cmd_lower="/x"),
    dcase("process_unregistered_slash", {"get_skill_bundles": {"__raise__": "SystemExit", "msg": "1"}},
          cmd_original="/x", cmd_lower="/x"),
    dcase("process_unregistered_slash", cmd_original="/zzz", cmd_lower="/zzz"),   # expand path
    dcase("process_unregistered_slash", self=dict(SELF_BASE, config=None),
          cmd_original="/x", cmd_lower="/x"),   # None.get
    dcase("process_unregistered_slash", cmd_original="   ", cmd_lower="   "),   # IndexError
    bare("process_unregistered_slash", cmd_original="/x", cmd_lower="/x"),
]

# ── _expand_slash_prefix ──
CASES += [
    dcase("expand_slash_prefix", cmd_original="/qui me", cmd_lower="/qui me",
          skill_commands={}, skill_bundles={}),
    dcase("expand_slash_prefix", cmd_original="/help", cmd_lower="/help",
          skill_commands={}, skill_bundles={}),
    dcase("expand_slash_prefix", cmd_original="/q", cmd_lower="/q",
          skill_commands={}, skill_bundles={}),   # ambiguous quit/queue? COMMANDS has quit only -> expand
    dcase("expand_slash_prefix", {"COMMANDS": [["/quit", "/quint"]]},
          cmd_original="/qui", cmd_lower="/qui", skill_commands={}, skill_bundles={}),
    dcase("expand_slash_prefix", {"COMMANDS": [["/quit", "/quint", "/quick"]]},
          cmd_original="/qui", cmd_lower="/qui", skill_commands={}, skill_bundles={}),
    dcase("expand_slash_prefix", cmd_original="/sk", cmd_lower="/sk",
          skill_commands={"/skill-a": {}}, skill_bundles={}),
    dcase("expand_slash_prefix", cmd_original="/zzz", cmd_lower="/zzz",
          skill_commands={}, skill_bundles={}),   # unknown_command_lines
    dcase("expand_slash_prefix", {"COMMANDS": {"__raise__": "SystemExit", "msg": "1"}},
          cmd_original="/q", cmd_lower="/q", skill_commands={}, skill_bundles={}),
    dcase("expand_slash_prefix", {"unknown_command_lines": [["lead"]]},
          cmd_original="/zzz", cmd_lower="/zzz", skill_commands={}, skill_bundles={}),  # unpack err
    bare("expand_slash_prefix", cmd_original="/q", cmd_lower="/q",
         skill_commands={}, skill_bundles={}),
]


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))
    thin = [k for k, v in counts.items() if v < 5]
    if thin:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % thin)


if __name__ == "__main__":
    main()