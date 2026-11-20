#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_tools_flow2.jsonl.

Each case: {"func": <name>, "in": {"args": {...}, "io": {...}},
            ["fail_closed": true]}.
io mirrors the C universal seam: a plain value is returned every call; a bare
LIST is a per-call queue (exhaustion is fail-closed); {"__script__":true,
"ret":v} returns v (use for list-valued results); {"__script__":true,
"raise":{type,message}} raises. Every func gets >= 5 cases: happy paths,
branch variants, error paths, a fail-closed missing-script case and (where a
catch site exists) a BaseException propagation case. ASCII corpus, POSIX paths.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_tools_flow2.jsonl"
CASES = []


def case(func, args, io, fail_closed=False):
    c = {"func": func, "in": {"args": args, "io": io}}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def S(ret):
    return {"__script__": True, "ret": ret}


def R(kind, message):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def okio(**over):
    d = {
        "ok": S({"jsonrpc": "2.0", "id": "r1", "result": {"type": "send"}}),
        "err": S({"jsonrpc": "2.0", "id": "r1", "error": {"code": 4001, "message": "x"}}),
        "exec_out": S({"jsonrpc": "2.0", "id": "r1", "result": {"type": "exec"}}),
        "load_cfg": S({}),
        "tools_mod": S("mod"),
        "sessions.get": S(None),
        "session.get": S(None),
        "agent.attr": S(None),
        "session.write": S(None),
        "session.del": S(None),
        "dispatch": S({"jsonrpc": "2.0", "id": "r1", "result": {"type": "exec"}}),
        "busy_error": S(None),
        "rewind_prelude": S([[], [], None]),
        "rewind_or_err": S([[[], {"content": "x"}, 1], None]),
        "session_key_or_err": S(["sid", S("mod"), None]),
        "session_toolsets": S([["a"], []]),
        "profile_scope": S(None),
        "session_home_scope": S(None),
        "session_cwd": S("/home/u/ws"),
        "resolve_agent_platform": S("linux"),
        "load_enabled_toolsets": S(["a"]),
        "load_disabled_toolsets": S([]),
        "session_source": S({}),
        "tui_model_friction": S(None),
        "resolve_session_platform": S("linux"),
        "skill_scaffold_projection": S("body"),
        "resolve_model": S("m"),
        "load_tool_progress_mode": S("all"),
        "write_config_key": S(None),
        "logger.warning": S(None),
        "bind_module": S(None),
        "plugin_commands.register": S(None),
        "session_not_owned_err": S({"error": "not owned"}),
        "hermes_cli.goals.GoalManager": S({"id": 1}),
        "hermes_cli.goals.last_user_message_from_db": S(None),
        "hermes_cli.loops.LoopManager": S({"id": 1}),
        "hermes_cli.loops.dispatch_loop_command": S({"output": "", "created": False}),
        "set_session_context": S({"t": 1}),
        "clear_session_context": S(None),
        "mgr.unsupported_backend_reason": S(None),
        "hermes_cli.tools_config._apply_toolset_change": S(None),
        "hermes_cli.tools_config._apply_mcp_change": S([]),
        "hermes_cli.config.save_config": S(None),
        "reset_session_agent": S(None),
    }
    d.update(over)
    return d


RID = "r1"


# ── _session_toolsets (6) ──
case("_session_toolsets", {"session": {"agent": {"enabled_toolsets": ["a"]}}},
     okio(**{"agent.enabled_toolsets": S(["a"]), "agent.disabled_toolsets": S(["b"])}))
case("_session_toolsets", {"session": {"agent": {}}},
     okio(**{"agent.enabled_toolsets": S(None), "agent.disabled_toolsets": S(["x"])}))
case("_session_toolsets", {"session": None},
     okio(**{"load_enabled_toolsets": S(["on"]), "load_disabled_toolsets": S(["off"])}))
case("_session_toolsets", {"session": {}},
     okio(session_source=S(None), resolve_agent_platform=S(None),
          load_enabled_toolsets=S(None), load_disabled_toolsets=S(None)))
case("_session_toolsets", {"session": {"agent": {}}},
     okio(**{"agent.enabled_toolsets": R("RuntimeError", "boom")}),
     fail_closed=False)
case("_session_toolsets", {"session": None}, {}, fail_closed=True)

# ── _toolset_rows (6) ──
INFO = {"a": {"description": "da", "tool_count": 2, "resolved_tools": ["t1"]},
        "b": {"description": "db", "tool_count": 1, "resolved_tools": []}}
case("_toolset_rows", {"params": {"session_id": "s1"}, "with_tools": True},
     okio(**{"toolsets.get_all_toolsets": S(INFO),
             "toolsets.get_toolset_info": [S(INFO["a"]), S(INFO["b"])],
             "session_toolsets": S([["a"], []]),
             "sessions.get": S({"session_key": "s1"})}))
case("_toolset_rows", {"params": {}, "with_tools": False},
     okio(**{"toolsets.get_all_toolsets": S(INFO),
             "toolsets.get_toolset_info": [S(INFO["a"]), S(INFO["b"])],
             "session_toolsets": S([[None], []])}))
case("_toolset_rows", {"params": {}, "with_tools": True},
     okio(**{"toolsets.get_all_toolsets": S(INFO),
             "toolsets.get_toolset_info": [S(INFO["a"]), S(None)]}))
case("_toolset_rows", {"params": {}, "with_tools": False},
     okio(**{"toolsets.get_all_toolsets": S({})}))
case("_toolset_rows", {"params": {}},
     okio(**{"toolsets.get_all_toolsets": S(INFO),
             "toolsets.get_toolset_info": [R("ValueError", "bad")]}), fail_closed=False)
case("_toolset_rows", {"params": {}}, {}, fail_closed=True)

# ── _skill_dispatch_or_refuse (6) ──
case("_skill_dispatch_or_refuse", {"rid": RID, "sid": "s1", "base": "foo", "arg": "a"},
     okio(dispatch=S({"error": {"code": 1}})))
case("_skill_dispatch_or_refuse", {"rid": RID, "sid": "s1", "base": "foo", "arg": "a"},
     okio(dispatch=S({"result": {"type": "send"}})))
case("_skill_dispatch_or_refuse", {"rid": RID, "sid": "s1", "base": "foo", "arg": "a"},
     okio(dispatch=S({"result": {}}), err=S({"ok": 1})))
case("_skill_dispatch_or_refuse", {"rid": RID, "sid": "s1", "base": "bar", "arg": None},
     okio(dispatch=S({"result": None})))
case("_skill_dispatch_or_refuse", {"rid": RID, "sid": "s1", "base": "foo", "arg": "a"},
     okio(dispatch=R("SystemExit", "stop")), fail_closed=False)
case("_skill_dispatch_or_refuse", {"rid": RID, "sid": "s1", "base": "foo", "arg": "a"}, {}, fail_closed=True)

# ── _bundle_key_for (6) ──
case("_bundle_key_for", {"name": "foo"},
     okio(**{"hermes_cli.commands.resolve_command": S(None),
             "agent.skill_bundles.resolve_bundle_command_key": S("/foo")}))
case("_bundle_key_for", {"name": "foo"},
     okio(**{"hermes_cli.commands.resolve_command": S({"name": "foo"})}))
case("_bundle_key_for", {"name": "foo"},
     okio(**{"hermes_cli.commands.resolve_command": R("ValueError", "x")}))
case("_bundle_key_for", {"name": "bar"},
     okio(**{"hermes_cli.commands.resolve_command": S(None),
             "agent.skill_bundles.resolve_bundle_command_key": S(None)}))
case("_bundle_key_for", {"name": "baz"},
     okio(**{"hermes_cli.commands.resolve_command": S(None),
             "agent.skill_bundles.resolve_bundle_command_key": R("KeyboardInterrupt", "x")}))
case("_bundle_key_for", {"name": "foo"}, {}, fail_closed=True)

# ── _dispatch_bundle (7) ──
BDL = [["m"], ["s1", "s2"], []]
case("_dispatch_bundle", {"rid": RID, "name": "foo", "arg": "x", "session": {"session_key": "t1"}},
     okio(**{"bundle_key_for": S("/foo"),
             "agent.skill_bundles.build_bundle_invocation_message": S(BDL),
             "agent.skill_bundles.get_skill_bundles": S({"/foo": {"name": "Foo"}}),
             "skill_scaffold_projection": S("body")}))
case("_dispatch_bundle", {"rid": RID, "name": "foo", "arg": "x", "session": None},
     okio(**{"bundle_key_for": S("/foo"),
             "agent.skill_bundles.build_bundle_invocation_message": S([["m"], ["s"], ["miss"]]),
             "agent.skill_bundles.get_skill_bundles": S({}),
             "skill_scaffold_projection": S("body")}))
case("_dispatch_bundle", {"rid": RID, "name": "foo", "arg": "x", "session": None},
     okio(**{"bundle_key_for": S(None)}))
case("_dispatch_bundle", {"rid": RID, "name": "foo", "arg": "x", "session": None},
     okio(**{"bundle_key_for": S("/foo"),
             "agent.skill_bundles.build_bundle_invocation_message": R("ValueError", "nope")}))
case("_dispatch_bundle", {"rid": RID, "name": "foo", "arg": "x", "session": None},
     okio(**{"bundle_key_for": S("/foo"),
             "agent.skill_bundles.build_bundle_invocation_message": S(None)}))
case("_dispatch_bundle", {"rid": RID, "name": "foo", "arg": "x", "session": None},
     okio(**{"bundle_key_for": S("/foo"),
             "agent.skill_bundles.build_bundle_invocation_message": R("SystemExit", "halt")}))
case("_dispatch_bundle", {"rid": RID, "name": "foo", "arg": "x", "session": None}, {}, fail_closed=True)

# ── _dispatch_skill (7) ──
CMDS = {"/foo": {"name": "Foo"}}
case("_dispatch_skill", {"rid": RID, "name": "foo", "arg": "do", "session": {"session_key": "t"}},
     okio(**{"agent.skill_commands.get_interactive_skill_commands": S(CMDS),
             "agent.skill_commands.split_stacked_skill_commands": S([[], "do"]),
             "agent.skill_commands.build_skill_invocation_message": S("msg"),
             "skill_scaffold_projection": S("body")}))
case("_dispatch_skill", {"rid": RID, "name": "foo", "arg": "do", "session": None},
     okio(**{"agent.skill_commands.get_interactive_skill_commands": S(CMDS),
             "agent.skill_commands.split_stacked_skill_commands": S([["/bar"], "do"]),
             "agent.skill_commands.build_stacked_skill_invocation_message": S([["m"], ["s1", "s2"], ["miss"]]),
             "skill_scaffold_projection": S("body")}))
case("_dispatch_skill", {"rid": RID, "name": "foo", "arg": "do", "session": None},
     okio(**{"agent.skill_commands.get_interactive_skill_commands": S(CMDS),
             "agent.skill_commands.split_stacked_skill_commands": S([[], "do"]),
             "agent.skill_commands.build_skill_invocation_message": S(None)}))
case("_dispatch_skill", {"rid": RID, "name": "none", "arg": "do", "session": None},
     okio(**{"agent.skill_commands.get_interactive_skill_commands": S(CMDS)}))
case("_dispatch_skill", {"rid": RID, "name": "foo", "arg": "do", "session": None},
     okio(**{"agent.skill_commands.get_interactive_skill_commands": R("ValueError", "x")}))
case("_dispatch_skill", {"rid": RID, "name": "foo", "arg": "do", "session": None},
     okio(**{"agent.skill_commands.get_interactive_skill_commands": S(CMDS),
             "agent.skill_commands.split_stacked_skill_commands": R("SystemExit", "halt")}))
case("_dispatch_skill", {"rid": RID, "name": "foo", "arg": "do", "session": None}, {}, fail_closed=True)

# ── _prompt_builtin (5) ──
for mod, fn, kw in [("hermes_cli.learn", "build_learn_prompt", "arg"),
                    ("hermes_cli.plan", "build_plan_prompt", "prompt"),
                    ("hermes_cli.init", "build_init_prompt", ""),
                    ("hermes_cli.x", "build_x", None),
                    ("m", "f", "k")]:
    a = {"module": mod, "fn": fn}
    if kw is not None:
        a["kw"] = kw
    case("_prompt_builtin", a, okio())

# ── prompt_builtin.call (5) ──
for kw in [None, "prompt", "arg", "k", ""]:
    io = okio(**{"hermes_cli.learn.build_learn_prompt": S("built")})
    case("prompt_builtin.call", {"rid": RID, "module": "hermes_cli.learn",
                                 "fn": "build_learn_prompt", "kw": kw, "arg": "user"}, io)
case("prompt_builtin.call", {"rid": RID, "module": "m", "fn": "f", "arg": "user"}, {}, fail_closed=True)

# ── _dispatch_quick (7) ──
for typ, cmd, rc, out in [("exec", "echo a", 0, "ok"), ("exec", "echo b", 1, "bad"),
                          ("exec", "echo c", 1, ""), ("alias", "", 0, ""),
                          ("other", "", 0, "")]:
    cfg = {"quick_commands": {"q": {"type": typ, "command": cmd, "target": "alias-t"}}}
    io = okio(**{"load_cfg": S(cfg),
                 "tools.environments.local.build_subprocess_env": S({"K": "V"}),
                 "subprocess.run": S({"stdout": out, "stderr": "", "returncode": rc}),
                 "agent.redact.redact_sensitive_text": S(out)})
    case("_dispatch_quick", {"rid": RID, "name": "q", "params": {}, "session": None, "arg": ""}, io)
case("_dispatch_quick", {"rid": RID, "name": "missing"}, okio(load_cfg=S({})))
case("_dispatch_quick", {"rid": RID, "name": "q"}, {}, fail_closed=True)

# ── _profile_skill_command (6) ──
for cmds in [S(["/foo"]), S([]), R("ValueError", "x"), S(None), S(["/foo", "/bar"])]:
    case("_profile_skill_command", {"session": {"profile_home": "/p"}, "base": "foo"},
         okio(**{"agent.skill_commands.get_interactive_skill_commands": cmds}))
case("_profile_skill_command", {"session": None, "base": "foo"}, {}, fail_closed=True)

# ── _cmd_init (6) ──
case("_cmd_init", {"rid": RID, "session": {"session_key": "s"}, "arg": "extra"},
     okio(build_init_prompt_for_cwd=S("prompt")))
case("_cmd_init", {"rid": RID, "session": None, "arg": ""},
     okio(build_init_prompt_for_cwd=S("prompt")))
case("_cmd_init", {"rid": RID, "session": {"session_key": "s"}, "arg": ""},
     okio(**{"get_session_cwd": R("ValueError", "x"), "session_cwd": S("/ws"),
             "build_init_prompt_for_cwd": S("p")}))
case("_cmd_init", {"rid": RID, "session": {"session_key": "s"}, "arg": ""},
     okio(**{"get_session_cwd": S(None), "session_cwd": S(None), "build_init_prompt_for_cwd": S("p")}))
case("_cmd_init", {"rid": RID, "session": {"session_key": "s"}, "arg": ""},
     okio(**{"get_session_cwd": S("/cwd"), "build_init_prompt_for_cwd": S("p")}))
case("_cmd_init", {"rid": RID, "session": {}, "arg": ""}, {}, fail_closed=True)

# ── _cmd_moa (6) ──
case("_cmd_moa", {"rid": RID, "session": {"session_key": "s", "agent": {}}, "arg": "go", "params": {"session_id": "s"}},
     okio(**{"load_cfg": S({"moa": {}}),
             "hermes_cli.moa_config.normalize_moa_config": S({"default_preset": "p"}),
             "agent.attr": S("gpt"), "apply_model_switch": S(None)}))
case("_cmd_moa", {"rid": RID, "session": {"session_key": "s", "agent": None}, "arg": "go", "params": {}},
     okio(**{"load_cfg": S({}), "hermes_cli.moa_config.normalize_moa_config": S({"default_preset": "p"})}))
case("_cmd_moa", {"rid": RID, "session": {"session_key": "s"}, "arg": "", "params": {}}, okio())
case("_cmd_moa", {"rid": RID, "session": None, "arg": "go", "params": {}}, okio())
case("_cmd_moa", {"rid": RID, "session": {"session_key": "s", "agent": {}}, "arg": "go", "params": {}},
     okio(**{"load_cfg": S({}), "hermes_cli.moa_config.normalize_moa_config": S({"default_preset": "p"}),
             "agent.attr": S("m"), "apply_model_switch": R("SessionNotOwned", "no")}))
case("_cmd_moa", {"rid": RID, "session": {"session_key": "s"}, "arg": "go"}, {}, fail_closed=True)

# ── _cmd_focus (6) ──
case("_cmd_focus", {"rid": RID, "params": {"session_id": "s"}, "arg": "status", "session": None},
     okio(**{"load_cfg": S({"display": {"focus_view": True, "focus_saved_tool_progress": "x"}}),
             "hermes_cli.focus_view.resolve_focus_arg": S("status"),
             "hermes_cli.focus_view.format_focus_status": S("focus on")}))
case("_cmd_focus", {"rid": RID, "params": {"session_id": "s"}, "arg": "status", "session": None},
     okio(**{"load_cfg": S({"display": {}}),
             "hermes_cli.focus_view.resolve_focus_arg": S("status"),
             "load_tool_progress_mode": S("all"),
             "hermes_cli.focus_view.format_focus_status": S("off")}))
case("_cmd_focus", {"rid": RID, "params": {"session_id": "s"}, "arg": "help", "session": None},
     okio(**{"load_cfg": S({}), "hermes_cli.focus_view.resolve_focus_arg": S("usage")}))
case("_cmd_focus", {"rid": RID, "params": {"session_id": "s"}, "arg": "on", "session": None},
     okio(**{"load_cfg": S({}), "hermes_cli.focus_view.resolve_focus_arg": S(True),
             "dispatch": S({"result": {"tool_progress": "all"}}),
             "hermes_cli.focus_view.format_focus_toggle_message": S("toggled")}))
case("_cmd_focus", {"rid": RID, "params": {}, "arg": "on", "session": None},
     okio(**{"load_cfg": S({}), "hermes_cli.focus_view.resolve_focus_arg": S(True),
             "dispatch": S({"error": {"code": 1}})}))
case("_cmd_focus", {"rid": RID, "params": {}, "arg": "", "session": None}, {}, fail_closed=True)

# ── _cmd_retry (7) ──
REW = [[{"role": "user"}], [0], None]
case("_cmd_retry", {"rid": RID, "session": {"running": False}, "arg": ""},
     okio(**{"rewind_prelude": S(REW),
             "session_rules.turns.history_before_user_originated_turn": S(REW),
             "session_rules.turns.retryable_user_text": S("text"),
             "rewind_or_err": S([[{"role": "user"}, {"content": "text"}, 1], None])}))
case("_cmd_retry", {"rid": RID, "session": {}, "arg": ""}, okio(busy_error=S({"error": 1})))
case("_cmd_retry", {"rid": RID, "session": None, "arg": ""}, okio())
case("_cmd_retry", {"rid": RID, "session": {"running": False}, "arg": ""},
     okio(**{"rewind_prelude": S([[],[],None])}))
case("_cmd_retry", {"rid": RID, "session": {"running": False}, "arg": ""},
     okio(**{"rewind_prelude": S(REW),
             "session_rules.turns.history_before_user_originated_turn": S(REW),
             "session_rules.turns.retryable_user_text": R("ValueError", "bad")}))
case("_cmd_retry", {"rid": RID, "session": {"running": False}, "arg": ""},
     okio(**{"rewind_prelude": S(REW),
             "session_rules.turns.history_before_user_originated_turn": S(REW),
             "session_rules.turns.retryable_user_text": R("SystemExit", "halt")}))
case("_cmd_retry", {"rid": RID, "session": {"running": False}, "arg": ""}, {}, fail_closed=True)

# ── _cmd_steer (6) ──
case("_cmd_steer", {"rid": RID, "session": {"running": True}, "arg": "go"},
     okio(**{"agent.steer": S(True)}))
case("_cmd_steer", {"rid": RID, "session": {"running": True}, "arg": "go"},
     okio(**{"agent.steer": S(False)}))
case("_cmd_steer", {"rid": RID, "session": {"running": False}, "arg": "a" * 100}, okio())
case("_cmd_steer", {"rid": RID, "session": None, "arg": "go"}, okio())
case("_cmd_steer", {"rid": RID, "session": {"running": True}, "arg": "go"},
     okio(**{"agent.steer": R("ValueError", "x")}))
case("_cmd_steer", {"rid": RID, "session": {"running": True}, "arg": ""}, {}, fail_closed=True)

# ── _cmd_goal (6) ──
GD = {"output": "ok", "prompt": "p", "kickoff": True, "error": False}
case("_cmd_goal", {"rid": RID, "session": {"session_key": "s"}, "arg": "list"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "hermes_cli.goals.GoalManager": S({"id": 1}),
             "hermes_cli.goals.last_user_message_from_db": S("last"),
             "dispatch_goal_command": S(GD)}))
case("_cmd_goal", {"rid": RID, "session": {"session_key": "s"}, "arg": "x"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "hermes_cli.goals.GoalManager": S({"id": 1}),
             "hermes_cli.goals.last_user_message_from_db": S("last"),
             "dispatch_goal_command": S({"output": "o", "prompt": "p", "kickoff": False, "error": False})}))
case("_cmd_goal", {"rid": RID, "session": {"session_key": "s"}, "arg": "x"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "dispatch_goal_command": S({"output": "o", "prompt": None, "error": False})}))
case("_cmd_goal", {"rid": RID, "session": {"session_key": "s"}, "arg": "x"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "dispatch_goal_command": S({"output": "err", "prompt": "p", "error": True})}))
case("_cmd_goal", {"rid": RID, "session": None, "arg": "x"},
     okio(session_key_or_err=S([None, None, S({"error": 1})])))
case("_cmd_goal", {"rid": RID, "session": {}, "arg": "x"}, {}, fail_closed=True)

# ── _cmd_loop (6) ──
case("_cmd_loop", {"rid": RID, "session": {"session_key": "s"}, "arg": "add"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "hermes_cli.loops.LoopManager": S({"id": 1}),
             "hermes_cli.loops.dispatch_loop_command": S({"output": "created", "created": True}),
             "hermes_cli.loops.goal_blocks_loop_tick": S(True)}))
case("_cmd_loop", {"rid": RID, "session": {"session_key": "s"}, "arg": "list"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "hermes_cli.loops.LoopManager": S({"id": 1}),
             "hermes_cli.loops.dispatch_loop_command": S({"output": "o", "created": False})}))
case("_cmd_loop", {"rid": RID, "session": {"session_key": "s"}, "arg": "x"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "hermes_cli.loops.dispatch_loop_command": S({"output": "", "created": True}),
             "hermes_cli.loops.goal_blocks_loop_tick": S(False)}))
case("_cmd_loop", {"rid": RID, "session": {"session_key": "s"}, "arg": "x"},
     okio(**{"session_key_or_err": S(["s", "mod", None]),
             "hermes_cli.loops.dispatch_loop_command": S({"output": "o", "created": True}),
             "hermes_cli.loops.goal_blocks_loop_tick": R("ValueError", "x")}))
case("_cmd_loop", {"rid": RID, "session": None, "arg": "x"},
     okio(session_key_or_err=S([None, None, S({"error": 1})])))
case("_cmd_loop", {"rid": RID, "session": {}, "arg": "x"}, {}, fail_closed=True)

# ── _cmd_undo (7) ──
UREW = [[{"role": "user"}], [0, 1], None]
UOK = [[{"u": 1}, {"content": "text"}, 2], None]
case("_cmd_undo", {"rid": RID, "session": {"session_key": "s", "running": False}, "arg": "2"},
     okio(**{"rewind_prelude": S(UREW), "rewind_or_err": S(UOK),
             "session_rules.turns.flatten_message_text": S("target"),
             "agent.attr": S(None),
             "tui_gateway.checkpoints.undo_rollback_hint_for": S({"text": "hint"})}))
case("_cmd_undo", {"rid": RID, "session": {"session_key": "s", "running": False}, "arg": ""},
     okio(**{"rewind_prelude": S(UREW), "rewind_or_err": S(UOK),
             "session_rules.turns.flatten_message_text": S("target"),
             "agent.attr": S({"on": True}), "agent.on_session_switch": S(None),
             "agent.invalidate_system_prompt": S(None), "agent.set_last_flushed": S(None),
             "tui_gateway.checkpoints.undo_rollback_hint_for": S(None)}))
case("_cmd_undo", {"rid": RID, "session": None, "arg": ""}, okio())
case("_cmd_undo", {"rid": RID, "session": {"session_key": "s"}, "arg": "abc"}, okio(busy_error=S({"e":1})))
case("_cmd_undo", {"rid": RID, "session": {"running": False}, "arg": ""}, okio())
case("_cmd_undo", {"rid": RID, "session": {"session_key": "s", "running": False}, "arg": "x"},
     okio(**{"rewind_prelude": S(UREW), "rewind_or_err": S(UOK),
             "session_rules.turns.flatten_message_text": R("SystemExit", "halt")}))
case("_cmd_undo", {"rid": RID, "session": {"session_key": "s", "running": False}, "arg": ""}, {}, fail_closed=True)

# ── _cmd_compress (7) ──
case("_cmd_compress", {"rid": RID, "params": {"session_id": "s"}, "session": {"running": False, "agent": {}}},
     okio(**{"session_uses_compute_host": S(True),
             "compute_host_slash": S({"status": "ok", "text": "done"})}))
case("_cmd_compress", {"rid": RID, "params": {}, "session": {"running": False}},
     okio(**{"session_uses_compute_host": S(True),
             "compute_host_slash": S({"status": "failed", "text": "bad"})}))
case("_cmd_compress", {"rid": RID, "params": {}, "session": {"running": False}},
     okio(**{"session_uses_compute_host": S(True),
             "compute_host_slash": S({"status": "pending", "text": "wait"})}))
case("_cmd_compress", {"rid": RID, "params": {}, "session": {"running": False}},
     okio(**{"session_uses_compute_host": S(False), "compress_live_with_feedback": S("out")}))
case("_cmd_compress", {"rid": RID, "params": {}, "session": {"running": False}},
     okio(**{"session_uses_compute_host": S(False),
             "compress_live_with_feedback": R("CompressionBusy", "busy")}))
case("_cmd_compress", {"rid": RID, "params": {}, "session": {"running": False}},
     okio(**{"session_uses_compute_host": S(False),
             "compress_live_with_feedback": R("ValueError", "v")}))
case("_cmd_compress", {"rid": RID, "params": {}, "session": {}}, {}, fail_closed=True)

# ── _pending_subcommand_store (5) ──
case("_pending_subcommand_store", {"session": {}}, okio(**{"agent.attr": S({"store": 1})}))
case("_pending_subcommand_store", {"session": None},
     okio(**{"tools.memory_tool.load_on_disk_store": S({"disk": 1})}))
case("_pending_subcommand_store", {"session": {}},
     okio(**{"agent.attr": R("ValueError", "x"),
             "tools.memory_tool.load_on_disk_store": S({"disk": 1})}))
case("_pending_subcommand_store", {"session": None},
     okio(**{"tools.memory_tool.load_on_disk_store": R("ValueError", "x")}))
case("_pending_subcommand_store", {"session": None}, {}, fail_closed=True)

# ── _write_approval_mode_setter (5) ──
for sub in ["memory", "skills", "a.b", "x", ""]:
    case("_write_approval_mode_setter", {"subsystem": sub}, okio())

# ── write_approval_mode_setter.call (5) ──
for enabled in [True, False, True, False, True]:
    case("write_approval_mode_setter.call",
         {"key": "memory.write_approval", "enabled": enabled},
         okio(write_config_key=S(None)))

# ── _run_pending_review (6) ──
case("_run_pending_review", {"rid": RID, "subsystem": "skills", "arg": "pending", "session": None, "unknown": "u"},
     okio(handle_pending_subcommand=S("done")))
case("_run_pending_review", {"rid": RID, "subsystem": "skills", "arg": "pending", "session": None, "unknown": "u"},
     okio(handle_pending_subcommand=S(None)))
case("_run_pending_review", {"rid": RID, "subsystem": "memory", "arg": "pending", "session": {}},
     okio(**{"agent.attr": S({"st": 1}), "handle_pending_subcommand": S("out")}))
case("_run_pending_review", {"rid": RID, "subsystem": "skills", "arg": "", "session": None, "unknown": "u"},
     okio(handle_pending_subcommand=S("x")))
case("_run_pending_review", {"rid": RID, "subsystem": "skills", "arg": "pending", "session": None},
     okio(handle_pending_subcommand=R("SystemExit", "halt")))
case("_run_pending_review", {"rid": RID, "subsystem": "skills", "arg": "x", "session": None}, {}, fail_closed=True)

# ── _cmd_memory (5) ──
for arg in ["pending", "mode on", "", "approve 1", "x"]:
    case("_cmd_memory", {"rid": RID, "session": None, "arg": arg, "unknown": "memory usage"},
         okio(handle_pending_subcommand=S("out")))

# ── _cmd_skills (6) ──
case("_cmd_skills", {"rid": RID, "session": None, "arg": "pending", "unknown": "skills usage",
                     "wa_enabled": True, "wa_pending": 1},
     okio(**{"hermes_cli.commands.resolve_command": S({"desktop_subcommands": ["pending", "approval"]}),
             "handle_pending_subcommand": S("out")}))
case("_cmd_skills", {"rid": RID, "session": None, "arg": "pending", "unknown": "u",
                     "wa_enabled": False, "wa_pending": 0},
     okio(**{"hermes_cli.commands.resolve_command": S({"desktop_subcommands": ["pending"]})}))
case("_cmd_skills", {"rid": RID, "session": None, "arg": "search", "unknown": "u"},
     okio(**{"hermes_cli.commands.resolve_command": S({"desktop_subcommands": ["pending"]})}))
case("_cmd_skills", {"rid": RID, "session": None, "arg": "", "unknown": "u"},
     okio(**{"hermes_cli.commands.resolve_command": S({"desktop_subcommands": []})}))
case("_cmd_skills", {"rid": RID, "session": None, "arg": "mode on", "unknown": "u", "wa_pending": 0},
     okio(**{"hermes_cli.commands.resolve_command": S({"desktop_subcommands": ["mode"]}),
             "handle_pending_subcommand": S("out")}))
case("_cmd_skills", {"rid": RID, "session": None, "arg": "pending"}, {}, fail_closed=True)

# ── _slash_worker_for (6) ──
case("_slash_worker_for", {"rid": RID, "sid": "s", "session": {"slash_worker": {"w": 1}}}, okio())
case("_slash_worker_for", {"rid": RID, "sid": "s", "session": {"session_key": "s", "agent": {"model": "gpt"}}},
     okio(**{"session.get": S({"w": 2})}))
case("_slash_worker_for", {"rid": RID, "sid": "s", "session": {"session_key": "s"}},
     okio(**{"agent.attr": S(None), "resolve_model": S("m"), "slash_worker_ctor": S({"w": 3})}))
case("_slash_worker_for", {"rid": RID, "sid": "s", "session": {"session_key": "s"}},
     okio(**{"agent.attr": S("gpt"), "slash_worker_ctor": R("ValueError", "bad")}))
case("_slash_worker_for", {"rid": RID, "sid": "s", "session": {}},
     okio(sessions_lock=R("SystemExit", "halt")))
case("_slash_worker_for", {"rid": RID, "sid": "s", "session": {}}, {}, fail_closed=True)

# ── _container_checkpoint_refusal (6) ──
case("_container_checkpoint_refusal", {"session": {"session_key": "s"}, "cwd": "/w"},
     okio(**{"mgr.unsupported_backend_reason": S("no backend")}))
case("_container_checkpoint_refusal", {"session": None, "cwd": None},
     okio(**{"mgr.unsupported_backend_reason": S(None)}))
case("_container_checkpoint_refusal", {"session": {"session_key": ""}, "cwd": "/w"},
     okio(**{"mgr.unsupported_backend_reason": S("x")}))
case("_container_checkpoint_refusal", {"session": {"session_key": "s"}, "cwd": "/w"},
     okio(**{"mgr.unsupported_backend_reason": R("ValueError", "x")}))
case("_container_checkpoint_refusal", {"session": {"session_key": "s"}, "cwd": "/w"},
     okio(**{"set_session_context": R("SystemExit", "halt")}))
case("_container_checkpoint_refusal", {"session": {}, "cwd": "/w"}, {}, fail_closed=True)

# ── _configure_session_tools (7) ──
CFG = {"k": 1}
io_cfg = lambda **kw: okio(**{
    "load_config": S(CFG),
    "hermes_cli.config.load_config": S(CFG),
    "hermes_cli.tools_config._get_plugin_toolset_keys": S(["core", "files"]),
    "hermes_cli.tools_config._get_platform_tools": S(["b", "a"]),
    "mcp_server_rows": S([[], [{"key": "srv"}]]),
    **kw,
})
case("_configure_session_tools", {"rid": RID, "params": {"action": "enable", "names": ["core"]}, "sid": "s", "session": None},
     io_cfg())
case("_configure_session_tools", {"rid": RID, "params": {"action": " DISABLE ", "names": ["core", "files"]}, "sid": "s", "session": {"k": 1}},
     io_cfg(**{"reset_session_agent": S({"reset": True})}))
case("_configure_session_tools", {"rid": RID, "params": {"action": "enable", "names": ["srv:tool"]}, "sid": "s", "session": None},
     io_cfg(**{"hermes_cli.tools_config._apply_mcp_change": S([]),
               "mcp_plugin_write_error": S(None)}))
case("_configure_session_tools", {"rid": RID, "params": {"action": "enable", "names": ["srv:tool"]}, "sid": "s", "session": None},
     io_cfg(**{"mcp_plugin_write_error": S({"error": 1})}))
case("_configure_session_tools", {"rid": RID, "params": {"action": "bad", "names": ["x"]}, "sid": "s", "session": None}, io_cfg())
case("_configure_session_tools", {"rid": RID, "params": {"action": "enable", "names": []}, "sid": "s", "session": None}, io_cfg())
case("_configure_session_tools", {"rid": RID, "params": {"action": "enable", "names": ["x"]}, "sid": "s", "session": None}, {}, fail_closed=True)

# ── _skills_install (6) ──
case("_skills_install", {"rid": RID, "query": "foo"},
     okio(**{"hermes_cli.skills_hub.do_install": S(True)}))
case("_skills_install", {"rid": RID, "query": "foo"},
     okio(**{"hermes_cli.skills_hub.do_install": S(None),
             "tools.skills_sync_bundled_ops.bundled_skill_for_install": S(True)}))
case("_skills_install", {"rid": RID, "query": "foo", "capture_log": "l1\nl2\nreason"},
     okio(**{"hermes_cli.skills_hub.do_install": S(None),
             "tools.skills_sync_bundled_ops.bundled_skill_for_install": S(False)}))
case("_skills_install", {"rid": RID, "query": "foo"},
     okio(**{"hermes_cli.skills_hub.do_install": S(False)}))
case("_skills_install", {"rid": RID, "query": "foo"},
     okio(**{"hermes_cli.skills_hub.do_install": R("ValueError", "x")}))
case("_skills_install", {"rid": RID, "query": "foo"}, {}, fail_closed=True)

# ── register (5) ──
for s in [None, {"name": "s"}, {}, "srv", 1]:
    case("register", {"server": s}, okio())


OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {OUT}")
for k in sorted(counts):
    print(f"  {k}: {counts[k]}")
thin = [k for k, v in counts.items() if v < 5]
if thin:
    print("THIN:", thin)
    raise SystemExit(1)
