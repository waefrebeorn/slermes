#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_plugins_flow.jsonl for the TUI
plugin-method FLOW port (src/gateway/tui_methods_plugins_flow.c).

Each case: {"func": <live def name>, "in": {"args": {...}, "io": {...}},
["fail_closed": true]}.

io scripts mirror the C universal seam: a plain value is returned on every
call; a bare LIST is a per-call queue (exhaustion is fail-closed); a
{"__script__": true, "ret": v} wrapper returns v (REQUIRED for list-valued
results); {"__script__": true, "raise": {"type", "message"}} raises.
tools_mod scripts return the module NAME string (queues for multi-import
flows). ok scripts return the JSON-RPC envelope. io["toolsets.data"] scripts
the toolsets module DATA attributes (untraced).

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a
BaseException propagation case. ASCII corpus; POSIX normalized paths;
integral numbers; str()-projected values are str/int/bool/null only.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_plugins_flow.jsonl"

CASES = []


def case(func, args=None, io=None, fail_closed=False):
    c = {"func": func, "in": {"args": args or {}, "io": io or {}}}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_script(kind, message):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def ok_env(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


RID = "r1"

# ── _tools_mod ──
for mod in ["hermes_cli.plugins", "tools.mcp_tool_common", ""]:
    case("_tools_mod", {"module": mod}, {"importlib.import_module": mod})
case("_tools_mod", {"module": "x"},
     {"importlib.import_module": raise_script("ValueError", "bad module")})
case("_tools_mod", {"module": "x"},
     {"importlib.import_module": raise_script("SystemExit", "bye")})
case("_tools_mod", {"module": "x"}, {}, fail_closed=True)

# ── _is_registry_command ──
REG_IO = {"tools_mod": "hermes_cli.commands"}
case("_is_registry_command", {"base": "help"},
     {**REG_IO, "commands.resolve_command": {"cmd": "help"}})
case("_is_registry_command", {"base": "nope"},
     {**REG_IO, "commands.resolve_command": None})
case("_is_registry_command", {"base": "help"},
     {**REG_IO, "commands.resolve_command": raise_script("ValueError", "boom")})
case("_is_registry_command", {"base": "help"},
     {"tools_mod": raise_script("RuntimeError", "import down")})
case("_is_registry_command", {"base": "help"},
     {"tools_mod": raise_script("SystemExit", "bye")})
case("_is_registry_command", {"base": "help"},
     {"tools_mod": "other.module"})          # AttributeError -> caught -> False
case("_is_registry_command", {"base": "help"}, REG_IO, fail_closed=True)

# ── _plugin_command_handler ──
PCH_IO = {"tools_mod": "hermes_cli.plugins"}
case("_plugin_command_handler", {"name": "cmd"},
     {**PCH_IO, "plugins.get_plugin_command_handler": {"h": 1}})
case("_plugin_command_handler", {"name": "cmd"},
     {**PCH_IO, "plugins.get_plugin_command_handler": None})
case("_plugin_command_handler", {"name": "cmd"},
     {**PCH_IO, "plugins.get_plugin_command_handler": raise_script("ValueError", "x")})
case("_plugin_command_handler", {"name": "cmd"},
     {"tools_mod": raise_script("RuntimeError", "down")})
case("_plugin_command_handler", {"name": "cmd"},
     {**PCH_IO, "plugins.get_plugin_command_handler": raise_script("SystemExit", "bye")})
case("_plugin_command_handler", {"name": "cmd"}, PCH_IO, fail_closed=True)

# ── _ (plugins.catalog.lookup) ──
ENTRY = {"name": "plug", "repo": "org/repo", "sha": "abc123",
         "subdir": "", "tier": "community"}
ENTRY2 = {"name": "other", "repo": "org/other", "sha": "def",
          "subdir": "sub", "tier": "core"}
case("_", {"rid": RID, "params": {"name": "  "}},
     {"plugins_cmd_catalog.looks_like_catalog_name": False,
      "ok": ok_env(RID, {"status": "invalid_name"})})
case("_", {"rid": RID, "params": {"name": "plug"}},
     {"plugins_cmd_catalog.looks_like_catalog_name": True,
      "plugin_catalog.load_catalog_live": script([]),
      "ok": ok_env(RID, {"status": "unavailable"})})
case("_", {"rid": RID, "params": {"name": "plug"}},
     {"plugins_cmd_catalog.looks_like_catalog_name": True,
      "plugin_catalog.load_catalog_live": script([ENTRY2]),
      "ok": ok_env(RID, {"status": "unknown"})})
case("_", {"rid": RID, "params": {"name": " plug "}},
     {"plugins_cmd_catalog.looks_like_catalog_name": True,
      "plugin_catalog.load_catalog_live": script([ENTRY]),
      "plugin_catalog.resolved_removed_entries": script([]),
      "plugin_catalog.match_removed": [False, False],
      "ok": ok_env(RID, {"status": "found", "entry": ENTRY})})
case("_", {"rid": RID, "params": {"name": "plug"}},
     {"plugins_cmd_catalog.looks_like_catalog_name": True,
      "plugin_catalog.load_catalog_live": script([ENTRY]),
      "plugin_catalog.resolved_removed_entries": script([{"name": "plug"}]),
      "plugin_catalog.match_removed": [True],
      "ok": ok_env(RID, {"status": "unknown"})})
case("_", {"rid": RID, "params": {"name": "plug"}},
     {"plugins_cmd_catalog.looks_like_catalog_name": True,
      "plugin_catalog.load_catalog_live": script([ENTRY]),
      "plugin_catalog.resolved_removed_entries": script([{"repo": "org/repo"}]),
      "plugin_catalog.match_removed": [False, True],
      "ok": ok_env(RID, {"status": "unknown"})})
case("_", {"rid": RID, "params": {"name": "plug"}},
     {"plugins_cmd_catalog.looks_like_catalog_name": True,
      "plugin_catalog.load_catalog_live": raise_script("RuntimeError", "dead")})
case("_", {"rid": RID, "params": {"name": "plug"}}, {}, fail_closed=True)
case("_", {"rid": RID, "params": {"name": "plug"}},
     {"plugins_cmd_catalog.looks_like_catalog_name": True,
      "plugin_catalog.load_catalog_live": script([ENTRY]),
      "plugin_catalog.resolved_removed_entries": script([]),
      "plugin_catalog.match_removed": raise_script("SystemExit", "bye")})

# ── register (three modules) ──
for reg in ["methods_plugin_catalog.register",
            "methods_tools_plugin_commands.register",
            "methods_tools_plugin_servers.register"]:
    case(reg, {"server": "srv"}, {"bind_module": None})
    case(reg, {"server": {"id": 1}}, {"bind_module": None})
    case(reg, {"server": None}, {"bind_module": None})
    case(reg, {"server": "srv"},
         {"bind_module": raise_script("ValueError", "bind failed")})
    case(reg, {"server": "srv"},
         {"bind_module": raise_script("SystemExit", "bye")})
    case(reg, {"server": "srv"}, {}, fail_closed=True)

# ── _server_identity ──
SI_TM = {"tools_mod": "tools.mcp_tool_config"}
case("_server_identity", {"cfg": {"command": "run", "args": ["a", 1], "url": "u"}},
     {**SI_TM, "mcp_tool_config._interpolate_env_vars":
         script({"command": "run", "args": ["a", 1], "url": "u"})})
case("_server_identity", {"cfg": "not-a-dict"},
     {**SI_TM, "mcp_tool_config._interpolate_env_vars": script({})})
case("_server_identity", {"cfg": {"args": "solo"}},
     {**SI_TM, "mcp_tool_config._interpolate_env_vars": script({"args": "solo"})})
case("_server_identity", {"cfg": {"args": None, "command": "c"}},
     {**SI_TM, "mcp_tool_config._interpolate_env_vars":
         script({"args": None, "command": "c"})})
case("_server_identity", {"cfg": {"args": [1, True, None]}},
     {**SI_TM, "mcp_tool_config._interpolate_env_vars":
         script({"args": [1, True, None]})})
case("_server_identity", {"cfg": {}},
     {**SI_TM, "mcp_tool_config._interpolate_env_vars": raise_script("ValueError", "env")})
case("_server_identity", {"cfg": {}},
     {**SI_TM, "mcp_tool_config._interpolate_env_vars": raise_script("SystemExit", "bye")})
case("_server_identity", {"cfg": {}}, SI_TM, fail_closed=True)

# ── _session_home_scope ──
SH_TM = {"tools_mod": ["hermes_constants", "agent.runtime_cwd"]}
case("_session_home_scope", {"session": {"profile_home": "/h/p1", "cwd": "/w"}},
     {**SH_TM, "hermes_constants.set_hermes_home_override": "t1",
      "agent.runtime_cwd.set_session_cwd": "c1",
      "agent.runtime_cwd.reset_session_cwd": None,
      "hermes_constants.reset_hermes_home_override": None})
case("_session_home_scope", {"session": None, "profile": "p2"},
     {**SH_TM, "profile_home": "/h/p2",
      "hermes_constants.set_hermes_home_override": "t1",
      "hermes_constants.reset_hermes_home_override": None})
case("_session_home_scope", {"session": None, "profile": None}, SH_TM)
case("_session_home_scope", {"session": {"cwd": "/w"}, "cwd": "/override"},
     {**SH_TM, "agent.runtime_cwd.set_session_cwd": "c1",
      "agent.runtime_cwd.reset_session_cwd": None})
case("_session_home_scope", {"session": {"profile_home": "/h/p1", "cwd": "/w"}},
     {**SH_TM, "hermes_constants.set_hermes_home_override": "t1",
      "agent.runtime_cwd.set_session_cwd": raise_script("RuntimeError", "cwd bad")})
case("_session_home_scope", {"session": {"profile_home": "/h/p1", "cwd": "/w"}},
     {**SH_TM, "hermes_constants.set_hermes_home_override": "t1",
      "agent.runtime_cwd.set_session_cwd": "c1",
      "agent.runtime_cwd.reset_session_cwd": raise_script("RuntimeError", "reset bad")})
case("_session_home_scope", {"session": {"profile_home": "/h/p1", "cwd": "/w"}},
     {**SH_TM, "hermes_constants.set_hermes_home_override": None,
      "agent.runtime_cwd.set_session_cwd": "c1",
      "agent.runtime_cwd.reset_session_cwd": None})
case("_session_home_scope", {"session": {"profile_home": None, "cwd": ""}}, SH_TM)
case("_session_home_scope", {"session": None, "profile": "p2"},
     {**SH_TM, "profile_home": raise_script("ProfileUnavailableError", "Profile 'p2' does not exist.")})
case("_session_home_scope", {"session": {"profile_home": "/h/p1", "cwd": "/w"}},
     {**SH_TM, "hermes_constants.set_hermes_home_override": "t1",
      "agent.runtime_cwd.set_session_cwd": raise_script("SystemExit", "bye")})
case("_session_home_scope", {"session": None}, {}, fail_closed=True)

# ── _run_plugin_command ──
SESS = {"session_key": "sk", "cwd": "/w", "agent": {"session_id": "sid1"}}
RPC_IO = {
    "tools_mod": ["hermes_cli.plugins", "hermes_cli.plugin_command_dispatch",
                  "hermes_cli.plugin_choices"],
    "set_session_context": script(["tok"]),
    "session_source": "cli",
    "plugin_choices.clarify_asker": "asker1",
    "plugin_choices.bind_asker": {"cm": 1},
    "plugin_command_dispatch.invoke_plugin_command": {"raw": "out"},
    "plugins.resolve_plugin_command_result": "final",
    "clear_session_context": None,
}
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": SESS, "sid": "s1"},
     RPC_IO)
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": None, "sid": ""},
     {**RPC_IO, "session_source": "desktop"})
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": SESS},
     {**RPC_IO, "plugins.resolve_plugin_command_result": None})
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": SESS},
     {**RPC_IO, "plugin_command_dispatch.invoke_plugin_command":
         raise_script("ValueError", "invoke bad")})
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": SESS},
     {**RPC_IO, "clear_session_context": raise_script("RuntimeError", "clear bad")})
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": SESS},
     {**RPC_IO, "set_session_context": raise_script("RuntimeError", "ctx bad")})
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": SESS},
     {**RPC_IO, "plugin_command_dispatch.invoke_plugin_command":
         raise_script("SystemExit", "bye")})
case("_run_plugin_command",
     {"handler": "h1", "arg": "a", "session": {"session_key": "sk", "agent": {}}},
     RPC_IO)
case("_run_plugin_command", {"handler": "h1", "arg": "a", "session": {}}, RPC_IO)
case("_run_plugin_command", {"handler": "h1", "arg": "a"}, {}, fail_closed=True)

# ── _dispatch_plugin_in_profile_scope ──
DPS_IO = {
    "session_home_scope": {"cm": 1},
    "tools_mod": "hermes_cli.plugins",
    "plugins.plugin_command_absent_nowait": False,
    "session_profile_runtime_scope": {"cm": 2},
    "plugin_command_handler": "h1",
    "run_plugin_command": "out",
}
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}, "sid": "s1"}, DPS_IO)
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}},
     {**DPS_IO, "plugins.plugin_command_absent_nowait": True})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": None, "profile": "p"},
     {**DPS_IO, "profile_home": "/h/p"})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": None, "profile": None},
     {**DPS_IO, "profile_home": None})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}},
     {**DPS_IO, "session_profile_runtime_scope": raise_script("ValueError", "scope bad"),
      "is_registry_command": True})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}},
     {**DPS_IO, "session_profile_runtime_scope": raise_script("ValueError", "scope bad"),
      "is_registry_command": False})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}},
     {**DPS_IO, "session_profile_runtime_scope": raise_script("SystemExit", "bye")})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}},
     {**DPS_IO, "plugin_command_handler": None})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}},
     {**DPS_IO, "run_plugin_command": raise_script("RuntimeError", "run bad")})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}},
     {"session_home_scope": raise_script("RuntimeError", "home bad")})
case("_dispatch_plugin_in_profile_scope",
     {"name": "cmd", "arg": "a", "session": {"k": 1}}, {}, fail_closed=True)

# ── _dispatch_plugin ──
DP_IO = {"str_arg": "", "dispatch_plugin_in_profile_scope": "out",
         "ok": ok_env(RID, {"type": "plugin", "output": "out"})}
case("_dispatch_plugin",
     {"rid": RID, "params": {"session_id": "s1"}, "session": {"k": 1},
      "name": "cmd", "arg": "a"}, DP_IO)
case("_dispatch_plugin",
     {"rid": RID, "params": {}, "session": None, "name": "cmd", "arg": ""},
     {**DP_IO, "dispatch_plugin_in_profile_scope": None})
case("_dispatch_plugin",
     {"rid": RID, "params": {}, "session": None, "name": "cmd", "arg": ""},
     {**DP_IO, "dispatch_plugin_in_profile_scope": raise_script("ValueError", "x")})
case("_dispatch_plugin",
     {"rid": RID, "params": {}, "session": None, "name": "cmd", "arg": ""},
     {**DP_IO, "dispatch_plugin_in_profile_scope": raise_script("SystemExit", "bye")})
case("_dispatch_plugin",
     {"rid": RID, "params": {}, "session": None, "name": "cmd", "arg": ""},
     {"str_arg": raise_script("ValueError", "strip bad")})
case("_dispatch_plugin",
     {"rid": RID, "params": None, "session": None, "name": "cmd", "arg": ""}, {})
case("_dispatch_plugin",
     {"rid": RID, "params": {"profile": "p"}, "session": None, "name": "cmd", "arg": ""},
     {**DP_IO, "str_arg": "p"})
case("_dispatch_plugin",
     {"rid": RID, "params": {}, "session": None, "name": "cmd", "arg": ""},
     {}, fail_closed=True)

# ── refresh_plugin_sessions ──
case("refresh_plugin_sessions", {"home": "/h", "plugin_key": "pk", "note": "n"},
     {"refresh_live_sessions": None})
case("refresh_plugin_sessions", {"home": "/h", "plugin_key": "pk", "note": ""},
     {"refresh_live_sessions": None})
case("refresh_plugin_sessions", {"home": None, "plugin_key": "pk", "note": "n"},
     {"refresh_live_sessions": None})
case("refresh_plugin_sessions", {"home": "/h", "plugin_key": "pk", "note": "n"},
     {"refresh_live_sessions": raise_script("RuntimeError", "refresh bad")})
case("refresh_plugin_sessions", {"home": "/h", "plugin_key": "pk", "note": "n"},
     {"refresh_live_sessions": raise_script("SystemExit", "bye")})
case("refresh_plugin_sessions", {"home": "/h", "plugin_key": "pk", "note": "n"},
     {}, fail_closed=True)

# ── _refresh_live_sessions ──
SESS_REC = {"agent": {"id": "a1", "platform": "cli"}, "profile_home": "/h/p1"}
RLS_IO = {
    "hermes_constants.hermes_home_key": ["hk1", "hk1"],
    "sessions_items": script([["s1", SESS_REC]]),
    "tools_mod": "tools.mcp_tool_agent",
    "session_profile_runtime_scope": {"cm": 1},
    "load_enabled_toolsets": script(["t1"]),
    "carry_onboarding_toolset": script(["t1", "t2"]),
    "load_disabled_toolsets": script([]),
    "mcp_tool_agent.refresh_agent_mcp_tools": None,
    "session_info": {"info": 1},
    "emit": None,
}
case("_refresh_live_sessions", {"home": "/h/p1"}, RLS_IO)
case("_refresh_live_sessions", {"home": None},
     {k: v for k, v in RLS_IO.items() if k != "hermes_constants.hermes_home_key"})
case("_refresh_live_sessions", {"home": "/h/p1", "plugin_note": ["pk", "n"]},
     {**RLS_IO, "tools_mod": ["tools.mcp_tool_agent", "agent.plugin_live_notes"],
      "plugin_live_notes.queue_plugin_notice": None})
case("_refresh_live_sessions", {"home": "/h/p1"},
     {**RLS_IO, "mcp_tool_agent.refresh_agent_mcp_tools":
         raise_script("RuntimeError", "mcp down"),
      "logger.warning": None})
case("_refresh_live_sessions", {"home": "/h/p1"},
     {**RLS_IO, "mcp_tool_agent.refresh_agent_mcp_tools":
         raise_script("SystemExit", "bye")})
case("_refresh_live_sessions", {"home": "/h/p1"},
     {**RLS_IO, "hermes_constants.hermes_home_key": ["hk1", "other"]})
case("_refresh_live_sessions", {"home": "/h/p1"},
     {**RLS_IO, "sessions_items": script([["s1", {"agent": {"id": "a1"},
                                                 "profile_home": None}]]),
      "hermes_constants.hermes_home_key": ["hk1", "hk1"],
      "get_process_hermes_home": "/h/p1"})
case("_refresh_live_sessions", {"home": "/h/p1"},
     {**RLS_IO, "sessions_items": script([["s1", {"agent": None}]])})
case("_refresh_live_sessions", {"home": "/h/p1"},
     {**RLS_IO, "session_profile_runtime_scope":
         raise_script("RuntimeError", "scope bad"),
      "logger.warning": None})
case("_refresh_live_sessions", {"home": None, "preserve_prefix": True},
     {k: v for k, v in RLS_IO.items() if k != "hermes_constants.hermes_home_key"},
     fail_closed=False)
case("_refresh_live_sessions", {"home": None}, {}, fail_closed=True)

# ── _plugin_server_rows ──
PKG = {"manifest": {"name": "plug",
                    "extensions": {"com.nousresearch.hermes":
                                   {"servers": {"srv": {}}}}}}
PSR_IO = {
    "tools_mod": ["hermes_cli.agent_plugins", "hermes_cli.plugins_manifest",
                  "tools.mcp_liveness", "tools.mcp_tool_common",
                  "tools.mcp_tool_scope", "hermes_cli.plugins_cmd_catalog",
                  "hermes_platform.declaration"],
    "agent_plugins.load_agent_plugin": PKG,
    "plugins_cmd_catalog.catalog_install_record": {"catalog_name": "plug"},
    "plugins_manifest.portable_mcp_server_name": "internal-1",
    "mcp_tool_scope._resolve_server_key": "conn-1",
    "core._servers.get": {"id": "s1", "session": None},
    "server._is_recycled_stdio": False,
    "declaration.lookup": {"d": 1},
    "mcp_liveness.status": {"state": "error", "availability": "down"},
    "mcp_liveness.describe": "sentence",
}
PSR_ARGS = {"plugin_dir": "/plugins/p1", "key": "cat/p1", "portable": True,
            "catalog_titles": {"plug": "Title"}}
case("_plugin_server_rows", PSR_ARGS, PSR_IO)
case("_plugin_server_rows", PSR_ARGS,
     {**PSR_IO, "core._servers.get": {"id": "s1", "session": {"live": 1}}})
case("_plugin_server_rows", PSR_ARGS,
     {**PSR_IO, "server._is_recycled_stdio": True})
case("_plugin_server_rows", {**PSR_ARGS, "portable": False}, {})
case("_plugin_server_rows", {**PSR_ARGS, "plugin_dir": None}, {})
case("_plugin_server_rows", PSR_ARGS,
     {**PSR_IO, "agent_plugins.load_agent_plugin":
         {"manifest": {"extensions": {"com.nousresearch.hermes":
                                      {"servers": []}}}}})
case("_plugin_server_rows", PSR_ARGS, {**PSR_IO, "declaration.lookup": None})
case("_plugin_server_rows", PSR_ARGS, {**PSR_IO, "mcp_liveness.status": None})
case("_plugin_server_rows", PSR_ARGS,
     {**PSR_IO, "plugins_cmd_catalog.catalog_install_record": None})
case("_plugin_server_rows", PSR_ARGS,
     {**PSR_IO, "tools_mod": ["hermes_cli.agent_plugins", "hermes_cli.plugins_manifest",
                              "tools.mcp_liveness", "tools.mcp_tool_common",
                              "tools.mcp_tool_scope", "hermes_cli.plugins_cmd_catalog",
                              "hermes_platform.declaration"],
      "agent_plugins.load_agent_plugin":
         {"manifest": {"name": "plug",
                       "extensions": {"com.nousresearch.hermes":
                                      {"servers": {"b": {}, "a": {}}}}}},
      "plugins_manifest.portable_mcp_server_name": ["i-a", "i-b"],
      "mcp_tool_scope._resolve_server_key": ["c-a", "c-b"],
      "core._servers.get": [{"id": "sa", "session": {"x": 1}},
                            {"id": "sb", "session": None}],
      "server._is_recycled_stdio": True,
      "declaration.lookup": {"d": 1},
      "mcp_liveness.status": {"state": "error", "availability": "down"},
      "mcp_liveness.describe": "sentence-b"})
case("_plugin_server_rows", PSR_ARGS, {}, fail_closed=True)
case("_plugin_server_rows", PSR_ARGS,
     {**PSR_IO, "mcp_liveness.status": raise_script("SystemExit", "bye")})

# ── _portable_server_tools ──
PST_IO = {
    "tools_mod": ["tools.mcp_tool_common", "tools.mcp_tool_scope"],
    "manager.get_portable_mcp_servers": {"srv": {"command": "x"}},
    "manager.get_portable_mcp_server_plugins": {"srv": "owner1"},
    "server_identity": [["x", [], ""], ["x", [], ""]],
    "mcp_tool_scope._resolve_server_key": "k1",
    "core._servers.get": {"id": "s1", "session": None,
                          "_registered_tool_names": ["t1", "t2"]},
    "core._lazy_server_tool_names.get": script(["t3"]),
}
PST_ARGS = {"manager": {"m": 1}, "native": {"srv": {"command": "x"}}}
case("_portable_server_tools", PST_ARGS, PST_IO)
case("_portable_server_tools", PST_ARGS,
     {**PST_IO, "server_identity": [["x", [], ""], ["y", [], ""]]})
case("_portable_server_tools", {"manager": {"m": 1}, "native": {}},
     {k: v for k, v in PST_IO.items() if k != "server_identity"})
case("_portable_server_tools", PST_ARGS,
     {**PST_IO, "core._servers.get": None,
      "core._lazy_server_tool_names.get": script(None)})
case("_portable_server_tools",
     {"manager": {}, "native": {"srv": {"command": "x"}}},
     {**PST_IO, "manager.get_portable_mcp_server_plugins":
         {"srv": "owner1", "srv2": "owner2"},
      "manager.get_portable_mcp_servers": {"srv": {"command": "x"}, "srv2": {}},
      "server_identity": [["x", [], ""], ["x", [], ""]],
      "mcp_tool_scope._resolve_server_key": ["k1", "k2"],
      "core._servers.get": [{"id": "s1", "session": None,
                             "_registered_tool_names": ["shared"]},
                            {"id": "s2", "session": None,
                             "_registered_tool_names": ["shared"]}],
      "core._lazy_server_tool_names.get": [script(None), script(None)]})
case("_portable_server_tools", PST_ARGS,
     {**PST_IO, "manager.get_portable_mcp_servers": {},
      "server_identity": [["x", [], ""], ["", [], ""]]})
case("_portable_server_tools", PST_ARGS, {}, fail_closed=True)
case("_portable_server_tools", PST_ARGS,
     {**PST_IO, "manager.get_portable_mcp_servers": raise_script("SystemExit", "bye")})

# ── _plugin_tool_ownership ──
PTO_IO = {
    "tools_mod": ["hermes_cli.plugins", "toolsets", "hermes_cli.config"],
    "toolsets.data": {"_HERMES_CORE_TOOLS": ["core1"],
                      "TOOLSETS": {"ts": {"tools": ["core2"]}}},
    "plugins.discover_plugins": None,
    "get_plugin_manager": {"_plugins": {
        "p1": {"tools_registered": ["t1", "core1", "t2"]},
        "p2": {"tools_registered": ["t2", "t3"]}}},
    "config.load_config": {"mcp_servers": {}},
    "portable_server_tools": {"p1": ["t4"]},
}
case("_plugin_tool_ownership", {}, PTO_IO)
case("_plugin_tool_ownership", {}, {**PTO_IO, "portable_server_tools": {}})
case("_plugin_tool_ownership", {},
     {**PTO_IO, "config.load_config": {"mcp_servers": "not-a-dict"}})
case("_plugin_tool_ownership", {},
     {**PTO_IO, "get_plugin_manager": {"_plugins": {
         "p1": {"tools_registered": ["core1", "core2"]}}},
      "portable_server_tools": {}})
case("_plugin_tool_ownership", {},
     {**PTO_IO, "plugins.discover_plugins": raise_script("RuntimeError", "disc bad")})
case("_plugin_tool_ownership", {},
     {**PTO_IO, "get_plugin_manager": {"_plugins": {}},
      "portable_server_tools": {"p9": ["t9"]}})
case("_plugin_tool_ownership", {}, {}, fail_closed=True)
case("_plugin_tool_ownership", {},
     {**PTO_IO, "portable_server_tools": raise_script("SystemExit", "bye")})

# ── _plugin_rows ──
PR_IO = {
    "tools_mod": ["hermes_cli.plugins_cmd", "hermes_cli.plugins_cmd_catalog",
                  "hermes_cli.plugins_cadence", "hermes_cli.desktop_plugin_halves",
                  "hermes_cli.plugins_settings"],
    "plugins_cmd._get_enabled_set": script(["e1"]),
    "plugins_cmd._get_disabled_set": script([]),
    "plugins_cmd_catalog.catalog_rows_maps": script(
        [{"p1": "pin1"}, {"p1": "v1"}, {"cat": "Title"}]),
    "plugins_cmd._read_install_metadata": {"ref": 1},
    "plugins_cadence.cached_git_updates": {"p1": "newsha"},
    "plugins_cmd._category_active_names": script(["cat"]),
    "plugin_tool_ownership": {"cat/p1": ["t1"]},
    "plugins_cmd._discover_all_plugins": script(
        [["plug", "1.0", "desc", "catalog", "/plugins/p1", "cat/p1"]]),
    "plugins_cmd._plugin_status": "enabled",
    "plugins_cmd._is_portable_plugin_dir": True,
    "desktop_plugin_halves.desktop_source": "plugin.js",
    "plugins_settings.plugin_settings_fields": script([{"field": "x"}]),
    "plugin_server_rows": script([{"name": "srv"}]),
    "plugins_cmd_catalog.catalog_row_fields": {"catalog_name": "plug"},
    "plugins_cmd.pinned_revision": "sha1",
}
case("_plugin_rows", {}, PR_IO)
case("_plugin_rows", {},
     {**PR_IO, "tools_mod": ["hermes_cli.plugins_cmd", "hermes_cli.plugins_cmd_catalog",
                             "hermes_cli.plugins_cadence", "hermes_cli.plugins_settings"],
      "plugins_cmd._discover_all_plugins": script(
          [["plug", "1.0", "desc", "catalog", None, "cat/p1"]]),
      "plugins_cmd._is_portable_plugin_dir": False})
case("_plugin_rows", {}, {**PR_IO, "plugins_cadence.cached_git_updates": {}})
case("_plugin_rows", {}, {**PR_IO, "plugins_cmd.pinned_revision": None})
case("_plugin_rows", {},
     {**PR_IO, "tools_mod": ["hermes_cli.plugins_cmd", "hermes_cli.plugins_cmd_catalog",
                             "hermes_cli.plugins_cadence", "hermes_cli.desktop_plugin_halves",
                             "hermes_cli.plugins_settings", "hermes_cli.desktop_plugin_halves",
                             "hermes_cli.plugins_settings"],
      "plugins_cmd._discover_all_plugins": script(
          [["zeta", "2", "dz", "git", "/plugins/z", "cat/z"],
           ["alpha", "1", "da", "git", "/plugins/a", "cat/a"]]),
      "plugins_cmd._plugin_status": ["enabled", "disabled"],
      "plugins_cmd._is_portable_plugin_dir": [False, True],
      "desktop_plugin_halves.desktop_source": [None, "plugin.js"],
      "plugins_settings.plugin_settings_fields": [script([]), script([{"f": 1}])],
      "plugin_server_rows": [script([]), script([{"name": "s"}])],
      "plugins_cmd_catalog.catalog_row_fields": [{}, {"catalog_name": "alpha"}],
      "plugins_cmd.pinned_revision": [None, "sha-a"],
      "plugins_cadence.cached_git_updates": {"a": "tgt-a"},
      "plugin_tool_ownership": {}})
case("_plugin_rows", {},
     {**PR_IO, "plugins_cmd._discover_all_plugins": raise_script("RuntimeError", "disc")})
case("_plugin_rows", {},
     {**PR_IO, "plugins_cmd._discover_all_plugins": script(
         [["plug", None, None, "catalog", "/plugins/p1", "cat/p1"]])})
case("_plugin_rows", {}, {}, fail_closed=True)
case("_plugin_rows", {},
     {**PR_IO, "plugins_cmd_catalog.catalog_rows_maps":
         raise_script("SystemExit", "bye")})

# ── write ──
OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=True) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases -> {OUT}")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
thin = [fn for fn, n in counts.items() if n < 5]
if thin:
    raise SystemExit(f"THIN COVERAGE: {thin}")