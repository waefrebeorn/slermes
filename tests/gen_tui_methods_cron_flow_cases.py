#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_cron_flow.jsonl for the TUI cron-method
FLOW port (src/gateway/tui_methods_cron_flow.c). The named quarry module
tui_gateway/methods_cron.py does not exist; the real defining module of the TUI
cron method (cron.manage) is tui_gateway/methods_tools.py.

Each case: {"func": <dispatch name>, "in": {"args": {...}, "io": {...}},
["fail_closed": true]}. The two decorated `def _` handlers dispatch under
_h_cron_manage / _h_learning_frames; every other func uses its live name.

io scripts mirror the C universal seam: a plain value is returned on every call;
a bare LIST is a per-call queue (exhaustion is fail-closed); a
{"__script__": true, "ret": v} wrapper returns v (use it for list-valued results);
{"__script__": true, "raise": {"type", "message"}} raises. tools_mod scripts
return the module NAME string. ok/err scripts return the JSON-RPC envelope the
server global would build. cronjob scripts return a JSON STRING (json.loads runs
real on both sides; integral JSON only).

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a BaseException
propagation case. ASCII corpus; POSIX paths; integral numbers.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_cron_flow.jsonl"

CASES = []


def case(func, args=None, io=None, fail_closed=False):
    c = {"func": func, "in": {"args": args or {}, "io": io or {}}}
    if fail_closed:
        c["fail_closed"] = True
    CASES.append(c)


def ok_env(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def err_env(rid, code, message, data=None):
    e = {"code": code, "message": message}
    if data is not None:
        e["data"] = data
    return {"jsonrpc": "2.0", "id": rid, "error": e}


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_script(kind, message):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


RID = "r1"
SCOPE = {"scope": "s1"}


def capture_kwargs(timeout, flags=0):
    return {"capture_output": True, "text": True, "encoding": "utf-8",
            "errors": "replace", "timeout": timeout, "stdin": -3, "creationflags": flags}


# -- _profile_scoped_rpc --
case("_profile_scoped_rpc", {"rid": RID, "params": {}, "fail_code": 5024},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE,
      "body": ok_env(RID, {"x": 1})})
case("_profile_scoped_rpc", {"rid": RID, "params": {"profile": "p1"}, "fail_code": 5023},
     {"str_arg": "p1", "profile_home": "/h/p1", "session_profile_runtime_scope": SCOPE,
      "body": ok_env(RID, {"x": 1})})
case("_profile_scoped_rpc", {"rid": RID, "params": {}, "required": [["name", "stripped"]]},
     {"err": err_env(RID, 4063, "name required")})
case("_profile_scoped_rpc", {"rid": RID, "params": {"profile": "p1"}},
     {"str_arg": "p1", "profile_home": raise_script("ProfileUnavailableError", "Profile 'p1' does not exist."),
      "err": err_env(RID, 4064, "profile 'p1' not found")})
case("_profile_scoped_rpc", {"rid": RID, "params": {}, "fail_code": 5024, "prefix": "P:"},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE,
      "body": raise_script("RuntimeError", "boom"), "err": err_env(RID, 5024, "P:boom")})
case("_profile_scoped_rpc", {"rid": RID, "params": {}},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE,
      "body": raise_script("SystemExit", "bye")})
case("_profile_scoped_rpc", {"rid": RID, "params": {}},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE},
     fail_closed=True)

# -- _guarded --
case("_guarded", {"rid": RID, "params": {}, "fail_code": 5000},
     {"body": ok_env(RID, {"ok": True})})
case("_guarded", {"rid": RID, "params": {"session_id": "s"}, "live_session": True},
     {"sess": script([{"sid": "s"}, None]), "body": ok_env(RID, {"ok": True})})
case("_guarded", {"rid": RID, "params": {"session_id": "s"}, "live_session": True},
     {"sess": script([None, err_env(RID, 4001, "session not found")])})
case("_guarded", {"rid": RID, "params": {}, "fail_code": 5000, "prefix": "E:"},
     {"body": raise_script("RuntimeError", "boom"), "err": err_env(RID, 5000, "E:boom")})
case("_guarded", {"rid": RID, "params": {}},
     {"body": raise_script("SystemExit", "x")})
case("_guarded", {"rid": RID, "params": {}}, {}, fail_closed=True)

# -- _rpc --
case("_rpc", {"rid": RID, "params": {}, "name": "tools.show", "fail_code": 5034},
     {"body": ok_env(RID, {"sections": []})})
case("_rpc", {"rid": RID, "params": {}, "name": "tools.show", "fail_code": 5034,
              "prefix": "tools.show failed: "},
     {"body": raise_script("ValueError", "bad"), "err": err_env(RID, 5034, "tools.show failed: bad")})
case("_rpc", {"rid": RID, "params": {"session_id": "s"}, "name": "x", "live_session": True},
     {"sess": script([None, err_env(RID, 4001, "session not found")])})
case("_rpc", {"rid": RID, "params": {}, "name": "x"},
     {"body": raise_script("KeyboardInterrupt", "")})
case("_rpc", {"rid": RID, "params": {}, "name": "x"}, {}, fail_closed=True)

# -- _scoped_rpc --
case("_scoped_rpc", {"rid": RID, "params": {}, "name": "cron.manage", "fail_code": 5023},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE,
      "body": ok_env(RID, {"jobs": []})})
case("_scoped_rpc", {"rid": RID, "params": {"profile": "p"}, "name": "mcp.servers.x",
                     "catch_resolve": False},
     {"str_arg": "p", "profile_home": raise_script("RuntimeError", "resolve fail")})
case("_scoped_rpc", {"rid": RID, "params": {"profile": "p1"}, "name": "cron.manage"},
     {"str_arg": "p1", "profile_home": raise_script("ProfileUnavailableError", "gone"),
      "err": err_env(RID, 4064, "profile 'p1' not found")})
case("_scoped_rpc", {"rid": RID, "params": {}, "name": "x", "required": [["name", "nonempty"]]},
     {"err": err_env(RID, 4063, "name required")})
case("_scoped_rpc", {"rid": RID, "params": {}, "name": "x"},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE},
     fail_closed=True)

# -- _mcp_rpc --
case("_mcp_rpc", {"rid": RID, "params": {"name": "srv"}, "name": "list"},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE,
      "body": ok_env(RID, {"servers": []})})
case("_mcp_rpc", {"rid": RID, "params": {}, "name": "list"},
     {"err": err_env(RID, 4063, "name required")})
case("_mcp_rpc", {"rid": RID, "params": {"name": "srv", "profile": "p"}, "name": "test"},
     {"str_arg": "p", "profile_home": raise_script("RuntimeError", "resolve fail")})
case("_mcp_rpc", {"rid": RID, "params": {"name": "srv"}, "name": "add"},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE,
      "body": raise_script("RuntimeError", "boom"), "err": err_env(RID, 5024, "boom")})
case("_mcp_rpc", {"rid": RID, "params": {"name": "srv"}, "name": "list"},
     {"str_arg": "", "profile_home": None, "session_profile_runtime_scope": SCOPE},
     fail_closed=True)

# -- _h_cron_manage --
case("_h_cron_manage", {"rid": RID, "params": {"action": "list"}},
     {"tools_mod": "tools.cronjob_tools", "cronjob": '{"jobs":[],"count":0}',
      "str_arg": "", "ok": ok_env(RID, {"jobs": [], "count": 0})})
case("_h_cron_manage", {"rid": RID, "params": {"action": "list", "include_disabled": "yes", "profile": "p1"}},
     {"tools_mod": "tools.cronjob_tools", "cronjob": '{"jobs":[],"count":0}',
      "str_arg": "p1", "ok": ok_env(RID, {"jobs": [], "count": 0, "scoped": "p1"})})
case("_h_cron_manage", {"rid": RID, "params": {"action": "add", "name": "j1", "schedule": "0 * * * *",
                                               "prompt": "do", "repeat": "3", "continuity": "true",
                                               "deliver": "bot-chat:x"}},
     {"tools_mod": "tools.cronjob_tools", "str_arg": "bot-chat:x",
      "cronjob": '{"id":"j1"}', "ok": ok_env(RID, {"id": "j1"})})
case("_h_cron_manage", {"rid": RID, "params": {"action": "remove", "name": "j1"}},
     {"tools_mod": "tools.cronjob_tools", "cronjob": '{"removed":true}',
      "ok": ok_env(RID, {"removed": True})})
case("_h_cron_manage", {"rid": RID, "params": {"action": "bogus"}},
     {"tools_mod": "tools.cronjob_tools", "err": err_env(RID, 4016, "unknown cron action: bogus")})
case("_h_cron_manage", {"rid": RID, "params": {"action": "list"}},
     {"tools_mod": "tools.cronjob_tools", "cronjob": raise_script("RuntimeError", "cron fail")})
case("_h_cron_manage", {"rid": RID, "params": {"action": "list"}},
     {"tools_mod": "tools.cronjob_tools"}, fail_closed=True)

# -- _mcp_server_rows --
case("_mcp_server_rows", {},
     {"tools_mod": ["hermes_cli.mcp_config", "tui_gateway.mcp_rpc_helpers"],
      "mcp_config._get_mcp_servers": {"cfg": 1},
      "mcp_rpc_helpers.server_configs_with_sources": script([{"srv": {}}, {"plug": {}}])})
case("_mcp_server_rows", {},
     {"tools_mod": "hermes_cli.mcp_config",
      "mcp_config._get_mcp_servers": raise_script("RuntimeError", "no config")})
case("_mcp_server_rows", {},
     {"tools_mod": ["hermes_cli.mcp_config", raise_script("RuntimeError", "no helpers")],
      "mcp_config._get_mcp_servers": {}})
case("_mcp_server_rows", {},
     {"tools_mod": ["hermes_cli.mcp_config", "tui_gateway.mcp_rpc_helpers"],
      "mcp_config._get_mcp_servers": {},
      "mcp_rpc_helpers.server_configs_with_sources": raise_script("RuntimeError", "bad rows")})
case("_mcp_server_rows", {}, {}, fail_closed=True)

# -- _mcp_named_server --
case("_mcp_named_server", {"rid": RID, "params": {"name": "srv"}},
     {"str_arg": "srv", "mcp_server_rows": script([{"srv": {"x": 1}}, {}])})
case("_mcp_named_server", {"rid": RID, "params": {"name": "nope"}},
     {"str_arg": "nope", "mcp_server_rows": script([{"srv": {}}, {}]),
      "err": err_env(RID, 4064, "server 'nope' not found")})
case("_mcp_named_server", {"rid": RID, "params": {"name": "srv"}},
     {"str_arg": raise_script("RuntimeError", "bad arg")})
case("_mcp_named_server", {"rid": RID, "params": {"name": "srv"}},
     {"str_arg": "srv", "mcp_server_rows": raise_script("RuntimeError", "no rows")})
case("_mcp_named_server", {"rid": RID, "params": {"name": "srv"}}, {}, fail_closed=True)

# -- _mcp_config_server_or_error --
case("_mcp_config_server_or_error", {"rid": RID, "params": {"name": "srv"}},
     {"mcp_named_server": script(["srv", {"srv": {}}, None]),
      "mcp_server_rows": script([{}, {"plug": {}}]), "mcp_plugin_write_error": None})
case("_mcp_config_server_or_error", {"rid": RID, "params": {"name": "srv"}},
     {"mcp_named_server": script(["srv", {}, err_env(RID, 4064, "server 'srv' not found")])})
case("_mcp_config_server_or_error", {"rid": RID, "params": {"name": "srv"}},
     {"mcp_named_server": script(["srv", {}, None]),
      "mcp_server_rows": script([{}, {"plug": {"srv": "p"}}]),
      "mcp_plugin_write_error": err_env(RID, 4090, "server 'srv' is provided by plugin 'p' and cannot be modified")})
case("_mcp_config_server_or_error", {"rid": RID, "params": {"name": "srv"}},
     {"mcp_named_server": script(["srv", {}, None]),
      "mcp_server_rows": raise_script("RuntimeError", "no rows")})
case("_mcp_config_server_or_error", {"rid": RID, "params": {"name": "srv"}}, {}, fail_closed=True)

# -- _user_turn_indices --
case("_user_turn_indices", {"session": {"history": [{"role": "user"}, {"role": "assistant"}]}},
     {"tools_mod": "session_rules.turns",
      "history_without_ephemeral_scaffolding": script([{"role": "user"}, {"role": "assistant"}]),
      "turns.user_originated_turn_view": [{"u": 1}, None]})
case("_user_turn_indices", {"session": {"history": []}},
     {"tools_mod": "session_rules.turns", "history_without_ephemeral_scaffolding": script([])})
case("_user_turn_indices", {"session": {"history": [{"role": "user"}]}},
     {"tools_mod": "session_rules.turns",
      "history_without_ephemeral_scaffolding": script([{"role": "user"}]),
      "turns.user_originated_turn_view": raise_script("RuntimeError", "bad turn")})
case("_user_turn_indices", {"session": {"history": [{"role": "user"}]}},
     {"tools_mod": "session_rules.turns",
      "history_without_ephemeral_scaffolding": raise_script("RuntimeError", "no history")})
case("_user_turn_indices", {"session": {}}, {}, fail_closed=True)

# -- _rewind_prelude --
case("_rewind_prelude", {"rid": RID, "session": {"running": True}, "cmd": "rewind", "empty_msg": "nothing"},
     {"busy_error": err_env(RID, 4009, "busy")})
case("_rewind_prelude", {"rid": RID, "session": {}, "cmd": "rewind", "empty_msg": "nothing"},
     {"busy_error": None, "user_turn_indices": script([[{"m": 1}], [0]])})
case("_rewind_prelude", {"rid": RID, "session": {}, "cmd": "rewind", "empty_msg": "nothing to rewind"},
     {"busy_error": None, "user_turn_indices": script([[], []]),
      "err": err_env(RID, 4018, "nothing to rewind")})
case("_rewind_prelude", {"rid": RID, "session": {}, "cmd": "rewind", "empty_msg": "x"},
     {"busy_error": None, "user_turn_indices": raise_script("RuntimeError", "no turns")})
case("_rewind_prelude", {"rid": RID, "session": {}, "cmd": "rewind", "empty_msg": "x"}, {}, fail_closed=True)

# -- _rewind_or_err --
case("_rewind_or_err", {"rid": RID, "session": {}, "keep": 1, "value_err": [4019, "rewind: "], "fail_prefix": "F:"},
     {"rewind_active_session_history": {"rewound": True}})
case("_rewind_or_err", {"rid": RID, "session": {}, "keep": 1, "value_err": [4019, "rewind: "], "fail_prefix": "F:"},
     {"rewind_active_session_history": raise_script("SessionNotOwned", "not owned"),
      "session_not_owned_err": err_env(RID, 4090, "session taken")})
case("_rewind_or_err", {"rid": RID, "session": {}, "keep": -1, "value_err": [4019, "rewind: "], "fail_prefix": "F:"},
     {"rewind_active_session_history": raise_script("ValueError", "bad keep"),
      "err": err_env(RID, 4019, "rewind: bad keep")})
case("_rewind_or_err", {"rid": RID, "session": {}, "keep": 1, "value_err": [4019, "rewind: "], "fail_prefix": "rewind failed: "},
     {"rewind_active_session_history": raise_script("RuntimeError", "boom"),
      "err": err_env(RID, 5008, "rewind failed: boom")})
case("_rewind_or_err", {"rid": RID, "session": {}, "keep": 1, "value_err": [4019, "rewind: "], "fail_prefix": "F:"},
     {"rewind_active_session_history": raise_script("SystemExit", "x")})
case("_rewind_or_err", {"rid": RID, "session": {}, "keep": 1, "value_err": [4019, "rewind: "], "fail_prefix": "F:"},
     {}, fail_closed=True)

# -- _learning_mutation --
case("_learning_mutation", {"rid": RID, "params": {"id": "n1"}, "fn_name": "node_detail", "arg_keys": ["id"]},
     {"tools_mod": "agent.learning_mutations", "learning_mutations.call": {"content": "c"},
      "ok": ok_env(RID, {"content": "c"})})
case("_learning_mutation", {"rid": RID, "params": {"id": "n1", "content": "new"},
                            "fn_name": "edit_node", "arg_keys": ["id", "content"]},
     {"tools_mod": "agent.learning_mutations", "learning_mutations.call": {"ok": True},
      "ok": ok_env(RID, {"ok": True})})
case("_learning_mutation", {"rid": RID, "params": {"id": "n1"}, "fn_name": "delete_node", "arg_keys": ["id"]},
     {"tools_mod": "agent.learning_mutations", "learning_mutations.call": raise_script("RuntimeError", "no node")})
case("_learning_mutation", {"rid": RID, "params": {}, "fn_name": "node_detail", "arg_keys": ["id"]},
     {"tools_mod": raise_script("ImportError", "no module")})
case("_learning_mutation", {"rid": RID, "params": {}, "fn_name": "node_detail", "arg_keys": ["id"]},
     {}, fail_closed=True)

# -- _h_learning_frames --
case("_h_learning_frames", {"rid": RID, "params": {}},
     {"tools_mod": ["agent.learning_graph", "agent.learning_graph_render"],
      "learning_graph.build_learning_graph": {"nodes": []},
      "learning_graph_render.render_frames": {"frames": []}, "ok": ok_env(RID, {"frames": []})})
case("_h_learning_frames", {"rid": RID, "params": {"cols": 100, "rows": 30, "frames": 10}},
     {"tools_mod": ["agent.learning_graph", "agent.learning_graph_render"],
      "learning_graph.build_learning_graph": {"nodes": []},
      "learning_graph_render.render_frames": {"frames": []}, "ok": ok_env(RID, {"frames": []})})
case("_h_learning_frames", {"rid": RID, "params": {"cols": 5, "rows": 3}},
     {"tools_mod": ["agent.learning_graph", "agent.learning_graph_render"],
      "learning_graph.build_learning_graph": {"nodes": []},
      "learning_graph_render.render_frames": {"frames": []}, "ok": ok_env(RID, {"frames": []})})
case("_h_learning_frames", {"rid": RID, "params": {"cols": "abc"}},
     {"tools_mod": ["agent.learning_graph", "agent.learning_graph_render"],
      "learning_graph.build_learning_graph": {"nodes": []},
      "learning_graph_render.render_frames": {"frames": []}, "ok": ok_env(RID, {"frames": []})})
case("_h_learning_frames", {"rid": RID, "params": {}},
     {"tools_mod": "agent.learning_graph",
      "learning_graph.build_learning_graph": raise_script("RuntimeError", "no graph")})
case("_h_learning_frames", {"rid": RID, "params": {}}, {}, fail_closed=True)

# -- _skills_search --
case("_skills_search", {"rid": RID, "params": {}, "query": "q"},
     {"tools_mod": ["tools.skills_hub_search", "tools.skills_hub_github"],
      "skills_hub_github.GitHubAuth": {"auth": 1},
      "skills_hub_search.create_source_router": {"router": 1},
      "skills_hub_search.unified_search": script([{"name": "s1", "description": "d1"}]),
      "ok": ok_env(RID, {"results": [{"name": "s1", "description": "d1"}]})})
case("_skills_search", {"rid": RID, "params": {}, "query": "q"},
     {"tools_mod": ["tools.skills_hub_search", "tools.skills_hub_github"],
      "skills_hub_github.GitHubAuth": {"auth": 1},
      "skills_hub_search.create_source_router": {"router": 1},
      "skills_hub_search.unified_search": None, "ok": ok_env(RID, {"results": []})})
case("_skills_search", {"rid": RID, "params": {}, "query": "q"},
     {"tools_mod": ["tools.skills_hub_search", "tools.skills_hub_github"],
      "skills_hub_github.GitHubAuth": {"auth": 1},
      "skills_hub_search.create_source_router": {"router": 1},
      "skills_hub_search.unified_search": raise_script("RuntimeError", "search fail")})
case("_skills_search", {"rid": RID, "params": {}, "query": "q"},
     {"tools_mod": ["tools.skills_hub_search", "tools.skills_hub_github"],
      "skills_hub_github.GitHubAuth": raise_script("RuntimeError", "no auth")})
case("_skills_search", {"rid": RID, "params": {}, "query": "q"}, {}, fail_closed=True)

# -- _skills_browse --
case("_skills_browse", {"rid": RID, "params": {"page": 2, "page_size": 10}, "query": "q"},
     {"tools_mod": "hermes_cli.skills_hub", "skills_hub.browse_skills": {"items": []},
      "ok": ok_env(RID, {"items": []})})
case("_skills_browse", {"rid": RID, "params": {"page": 0}, "query": "3"},
     {"tools_mod": "hermes_cli.skills_hub", "skills_hub.browse_skills": {"items": []},
      "ok": ok_env(RID, {"items": []})})
case("_skills_browse", {"rid": RID, "params": {}, "query": "q"},
     {"tools_mod": "hermes_cli.skills_hub", "skills_hub.browse_skills": {"items": []},
      "ok": ok_env(RID, {"items": []})})
case("_skills_browse", {"rid": RID, "params": {"page": "abc"}, "query": "q"}, {})
case("_skills_browse", {"rid": RID, "params": {"page": 1}, "query": "q"},
     {"tools_mod": "hermes_cli.skills_hub", "skills_hub.browse_skills": raise_script("RuntimeError", "browse fail")})
case("_skills_browse", {"rid": RID, "params": {"page": 1}, "query": "q"}, {}, fail_closed=True)

# -- _capture_run_kwargs --
case("_capture_run_kwargs", {"timeout": 30},
     {"tools_mod": "hermes_cli._subprocess_compat", "subprocess_compat.windows_hide_flags": 0})
case("_capture_run_kwargs", {"timeout": 5},
     {"tools_mod": "hermes_cli._subprocess_compat", "subprocess_compat.windows_hide_flags": 8})
case("_capture_run_kwargs", {"timeout": 30},
     {"tools_mod": "hermes_cli._subprocess_compat",
      "subprocess_compat.windows_hide_flags": raise_script("RuntimeError", "no flags")})
case("_capture_run_kwargs", {"timeout": 30},
     {"tools_mod": raise_script("ImportError", "no compat")})
case("_capture_run_kwargs", {"timeout": 30}, {}, fail_closed=True)

# -- _captured_exec --
case("_captured_exec", {"rid": RID, "cmd": ["ls"], "timeout": 5, "timeout_err": [5009, "timeout"], "fail_code": 5008},
     {"os.getcwd": "/cwd", "capture_run_kwargs": capture_kwargs(5),
      "subprocess.run": {"returncode": 0, "stdout": "out", "stderr": ""},
      "on_result": ok_env(RID, {"output": "out"})})
case("_captured_exec", {"rid": RID, "cmd": ["ls"], "timeout": 5, "timeout_err": [5009, "timeout"], "fail_code": 5008},
     {"os.getcwd": "/cwd", "capture_run_kwargs": capture_kwargs(5),
      "subprocess.run": raise_script("TimeoutExpired", "timed out"),
      "err": err_env(RID, 5009, "timeout")})
case("_captured_exec", {"rid": RID, "cmd": ["ls"], "timeout": 5, "timeout_err": [5009, "timeout"], "fail_code": 5008},
     {"os.getcwd": "/cwd", "capture_run_kwargs": capture_kwargs(5),
      "subprocess.run": raise_script("RuntimeError", "boom"), "err": err_env(RID, 5008, "boom")})
case("_captured_exec", {"rid": RID, "cmd": ["ls"], "timeout": 5, "timeout_err": [5009, "timeout"], "fail_code": 5008},
     {"os.getcwd": "/cwd", "capture_run_kwargs": capture_kwargs(5),
      "subprocess.run": {"returncode": 0}, "on_result": raise_script("RuntimeError", "bad result"),
      "err": err_env(RID, 5008, "bad result")})
case("_captured_exec", {"rid": RID, "cmd": ["ls"], "timeout": 5, "timeout_err": [5009, "timeout"], "fail_code": 5008},
     {"os.getcwd": "/cwd", "capture_run_kwargs": capture_kwargs(5),
      "subprocess.run": raise_script("SystemExit", "x")})
case("_captured_exec", {"rid": RID, "cmd": ["ls"], "timeout": 5, "timeout_err": [5009, "timeout"], "fail_code": 5008},
     {"os.getcwd": raise_script("RuntimeError", "no cwd"), "err": err_env(RID, 5008, "no cwd")})
case("_captured_exec", {"rid": RID, "cmd": ["ls"], "timeout": 5, "timeout_err": [5009, "timeout"], "fail_code": 5008},
     {}, fail_closed=True)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"THIN coverage (<5): {thin}")


if __name__ == "__main__":
    main()
