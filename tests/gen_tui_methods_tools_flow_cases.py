#!/usr/bin/env python3
"""Generate tests/fixtures/tui_methods_tools_flow.jsonl for the
tui_gateway/methods_tools.py FLOW port (src/gateway/tui_methods_tools_flow.c).

Each case: {"func": <live def name>, "in": {"args": {...}, "io": {...},
["state": {...}], ["invoke_loaded": [...]]}, ["fail_closed": true]}.

io scripts mirror the C universal seam: a plain value is returned on every
call; a bare LIST is a per-call queue (exhaustion is fail-closed); a
{"__script__": true, "ret": v} wrapper returns v (use it for list-valued
results); {"__script__": true, "raise": {"type", "message"}} raises.
tools_mod scripts return the module NAME string. ok/err scripts return the
JSON-RPC envelope the server global would build.

Every func gets >= 5 cases: happy paths, branch variants, error paths, a
fail-closed missing-script case, and (where a catch site exists) a
BaseException propagation case. ASCII corpus; POSIX paths; integral numbers.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "tui_methods_tools_flow.jsonl"

CASES = []


def case(func, args=None, io=None, state=None, fail_closed=False, invoke_loaded=None):
    inp = {"args": args or {}, "io": io or {}}
    if state is not None:
        inp["state"] = state
    if invoke_loaded is not None:
        inp["invoke_loaded"] = invoke_loaded
    c = {"func": func, "in": inp}
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

# ── _joined_output ──
for r, want in [
    ({"stdout": "out", "stderr": "err"}, "out\nerr"),
    ({"stdout": "out", "stderr": ""}, "out"),
    ({"stdout": "", "stderr": "err"}, "err"),
    ({"stdout": "", "stderr": ""}, ""),
    ({"stdout": "  out  ", "stderr": " err "}, "out  \n err"),
    ({"stdout": None, "stderr": None}, ""),
]:
    case("_joined_output", {"r": r})

# ── _is_snapshot_restore ──
for arg in ["restore", "REWIND now", "  Restore  extra", "snapshot", "", None]:
    case("_is_snapshot_restore", {"arg": arg})
case("_is_snapshot_restore", {"arg": "   "})          # IndexError: [0] of []
case("_is_snapshot_restore", {"arg": 5})              # AttributeError .split

# ── _worker_refused_skill ──
for exc, want in [
    ({"type": "RuntimeError", "message": "skill command refused before process: /x"}, True),
    ({"type": "RuntimeError", "message": "some other failure"}, False),
    ({"type": "ValueError", "message": ""}, False),
    ({"type": "Exception", "message": "prefix skill command refused before process: / tail"}, True),
    ({"type": "SystemExit", "message": "skill command refused before process: /y"}, True),
]:
    case("_worker_refused_skill", {"exc": exc})

# ── _refusal_data ──
for result in [
    {"source_moved": True},
    {"exists": True},
    {"source_moved": True, "exists": True},
    {},
    {"source_moved": 0, "exists": ""},
    {"source_moved": "yes"},
]:
    case("_refusal_data", {"result": result})

# ── _with_activation ──
ACT = {"plug-a": {"name": "plug-a", "key": "cat/plug-a", "live_now": True}}
case("_with_activation", {"result": {"name": "plug-a"}, "name": "plug-a"},
     state={"_plugin_activations": ACT})
case("_with_activation", {"result": {"plugin_name": "plug-a"}, "name": "other"},
     state={"_plugin_activations": ACT})
case("_with_activation", {"result": {"name": "plug-a"}, "name": None},
     state={"_plugin_activations": ACT})
case("_with_activation", {"result": {"name": "nope"}, "name": "nope"},
     state={"_plugin_activations": ACT})
case("_with_activation", {"result": {"name": "plug-a", "activation": {"already": 1}},
                          "name": "plug-a"},
     state={"_plugin_activations": ACT})
case("_with_activation", {"result": {"name": "plug-a"}, "name": "plug-a"},
     state={"_plugin_activations": {}})
case("_with_activation", {"result": {}, "name": ""},
     state={"_plugin_activations": ACT})

# ── _exec_out ──
for output in ["hello", "", None, "line1\nline2", "x" * 200]:
    case("_exec_out", {"rid": RID, "output": output},
         io={"ok": ok_env(RID, {"type": "exec", "output": output})})

# ── _run_action ──
TABLE = {"list": "h_list", "search": "h_search"}
case("_run_action", {"rid": RID, "params": {"action": "search"}, "table": TABLE,
                     "label": "skills", "extra": ["q"]},
     io={"h_search": ok_env(RID, {"results": []})})
case("_run_action", {"rid": RID, "params": {"action": "list"}, "table": TABLE,
                     "label": "plugins", "extra": []},
     io={"h_list": ok_env(RID, {"plugins": []})})
case("_run_action", {"rid": RID, "params": {}, "table": TABLE,
                     "label": "plugins", "extra": []},
     io={"h_list": ok_env(RID, {"plugins": []})})
case("_run_action", {"rid": RID, "params": {"action": "bogus"}, "table": TABLE,
                     "label": "skills", "extra": ["q"]},
     io={"err": err_env(RID, 4017, "unknown skills action: bogus")})
case("_run_action", {"rid": RID, "params": {"action": None}, "table": TABLE,
                     "label": "plugins", "extra": []},
     io={"err": err_env(RID, 4017, "unknown plugins action: None")})
case("_run_action", {"rid": RID, "params": {"action": "search"}, "table": TABLE,
                     "label": "skills", "extra": ["q"]},
     io={"h_search": raise_script("ValueError", "bad query")})
case("_run_action", {"rid": RID, "params": {"action": "search"}, "table": TABLE,
                     "label": "skills", "extra": ["q"]},
     io={}, fail_closed=True)

# ── _cmd_queue ──
case("_cmd_queue", {"rid": RID, "arg": "do it"},
     io={"ok": ok_env(RID, {"type": "send", "message": "do it"})})
case("_cmd_queue", {"rid": RID, "arg": " "},
     io={"ok": ok_env(RID, {"type": "send", "message": " "})})
case("_cmd_queue", {"rid": RID, "arg": ""},
     io={"err": err_env(RID, 4004, "usage: /queue <prompt>")})
case("_cmd_queue", {"rid": RID, "arg": None},
     io={"err": err_env(RID, 4004, "usage: /queue <prompt>")})
case("_cmd_queue", {"rid": RID, "arg": "x"}, io={}, fail_closed=True)

# ── _cmd_snapshot ──
SNAP_MSG = ("/snapshot restore is blocked in the TUI because it changes config/state on disk "
            "while the live agent has cached settings. Run it in the classic CLI, then restart the TUI.")
case("_cmd_snapshot", {"rid": RID, "arg": "restore"},
     io={"ok": ok_env(RID, {"type": "exec", "output": SNAP_MSG})})
case("_cmd_snapshot", {"rid": RID, "arg": "rewind now"},
     io={"ok": ok_env(RID, {"type": "exec", "output": SNAP_MSG})})
case("_cmd_snapshot", {"rid": RID, "arg": "status"})
case("_cmd_snapshot", {"rid": RID, "arg": ""})
case("_cmd_snapshot", {"rid": RID, "arg": "   "})     # IndexError
case("_cmd_snapshot", {"rid": RID, "arg": "restore"}, io={}, fail_closed=True)

# ── _mcp_reload_confirm_required ──
TM_CONFIG = {"tools_mod": "hermes_cli.config"}
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": {"approvals": {"mcp_reload_confirm": True}}})
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": {"approvals": {"mcp_reload_confirm": False}}})
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": {"approvals": {}}})
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": {"approvals": None}})
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": "not-a-dict"})
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": raise_script("ValueError", "bad config")})
case("_mcp_reload_confirm_required", {},
     io={"tools_mod": raise_script("ImportError", "no config module")})
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": raise_script("SystemExit", "boom")})
case("_mcp_reload_confirm_required", {},
     io={**TM_CONFIG, "config.load_config": {"approvals": {"mcp_reload_confirm": None}}})

# ── _session_key_or_err ──
case("_session_key_or_err", {"rid": RID, "session": None, "module": "hermes_cli.goals",
                             "label": "goals"},
     io={"err": err_env(RID, 4001, "no active session")})
case("_session_key_or_err", {"rid": RID, "session": {}, "module": "hermes_cli.goals",
                             "label": "goals"},
     io={"err": err_env(RID, 4001, "no active session")})
case("_session_key_or_err", {"rid": RID, "session": {"session_key": ""},
                             "module": "hermes_cli.goals", "label": "goals"},
     io={"err": err_env(RID, 4001, "no session key")})
case("_session_key_or_err", {"rid": RID, "session": {"session_key": "sk1"},
                             "module": "hermes_cli.goals", "label": "goals"},
     io={"tools_mod": "hermes_cli.goals"})
case("_session_key_or_err", {"rid": RID, "session": {"session_key": "sk1"},
                             "module": "hermes_cli.loops", "label": "loops"},
     io={"tools_mod": raise_script("ImportError", "no loops"),
         "err": err_env(RID, 5030, "loops unavailable: no loops")})
case("_session_key_or_err", {"rid": RID, "session": {"session_key": "sk1"},
                             "module": "hermes_cli.goals", "label": "goals"},
     io={"tools_mod": raise_script("SystemExit", "boom")})
case("_session_key_or_err", {"rid": RID, "session": {"session_key": "sk1"},
                             "module": "hermes_cli.goals", "label": "goals"},
     io={"err": err_env(RID, 5030, "goals unavailable: no script for tools_mod")},
     fail_closed=True)

# ── _ensure_plugin_activation_listener ──
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": {"scope_key": "k1"}, "on_plugin_loaded": None},
     state={})
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": {"scope_key": "k1"}, "on_plugin_loaded": None},
     state={"_plugin_activations": {}, "_plugin_activation_subscribed": ["k1"]})
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": {"scope_key": "k1"}, "on_plugin_loaded": None},
     state={},
     invoke_loaded=[[{"name": "p1", "key": "cat/p1", "tools": 2}]])
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": {"scope_key": "k2"}, "on_plugin_loaded": None},
     state={"_plugin_activations": {}, "_plugin_activation_subscribed": ["k1"]},
     invoke_loaded=[[{"name": "p2", "key": "cat/p2"}], [{"name": "p3", "key": "cat/p3"}]])
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": raise_script("RuntimeError", "no manager")})
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": {"scope_key": "k1"},
         "on_plugin_loaded": raise_script("ValueError", "bad listener")},
     state={})
case("_ensure_plugin_activation_listener", {}, io={}, fail_closed=True)
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": {"other": 1}})
case("_ensure_plugin_activation_listener", {},
     io={"get_plugin_manager": {"scope_key": "k1"}, "on_plugin_loaded": None},
     state={"_plugin_activations": {}, "_plugin_activation_subscribed": ["k1"]},
     invoke_loaded=[[{"name": "p9", "key": "cat/p9"}]])  # early return: no mutation

# ── _plugins_list ──
ROWS = [
    {"name": "a", "key": "cat/a", "source": "bundled"},
    {"name": "b", "key": "cat/b", "source": "git"},
    {"name": "c", "key": "cat/c", "source": "local"},
]
case("_plugins_list", {"rid": RID, "params": {}},
     io={"plugin_rows": script(ROWS),
         "ok": ok_env(RID, {"plugins": ROWS, "user_count": 2, "bundled_count": 1})})
case("_plugins_list", {"rid": RID, "params": {}},
     io={"plugin_rows": script([ROWS[0]]),
         "ok": ok_env(RID, {"plugins": [ROWS[0]], "user_count": 0, "bundled_count": 1})})
case("_plugins_list", {"rid": RID, "params": {}},
     io={"plugin_rows": script([]),
         "ok": ok_env(RID, {"plugins": [], "user_count": 0, "bundled_count": 0})})
case("_plugins_list", {"rid": RID, "params": {}},
     io={"plugin_rows": script([{"name": "x"}])})     # KeyError 'source'
case("_plugins_list", {"rid": RID, "params": {}},
     io={"plugin_rows": raise_script("RuntimeError", "scan failed")})
case("_plugins_list", {"rid": RID, "params": {}}, io={}, fail_closed=True)

# ── _plugins_toggle ──
TOG_OK = {"ok": True, "name": "cat/b", "unchanged": False, "restart_required": True,
          "gateway_reloaded": True}
case("_plugins_toggle", {"rid": RID, "params": {"key": "cat/b", "enable": True}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_set_agent_plugin_enabled": TOG_OK,
         "plugin_rows": script(ROWS),
         "ok": ok_env(RID, {"ok": True, "unchanged": False, "restart_required": True,
                            "gateway_reloaded": True, "activation": ACT["plug-a"],
                            "name": "cat/b", "plugin": ROWS[1]})},
     state={"_plugin_activations": ACT, "_plugin_activation_subscribed": []})
case("_plugins_toggle", {"rid": RID, "params": {"name": "b", "enable": False}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_set_agent_plugin_enabled": {**TOG_OK, "name": ""},
         "plugin_rows": script(ROWS),
         "ok": ok_env(RID, {"ok": True, "unchanged": False, "restart_required": True,
                            "gateway_reloaded": True, "activation": None,
                            "name": "b", "plugin": ROWS[1]})})
case("_plugins_toggle", {"rid": RID, "params": {}},
     io={"err": err_env(RID, 4019, "plugins.toggle requires a 'key' or 'name'")})
case("_plugins_toggle", {"rid": RID, "params": {"key": "cat/b"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_set_agent_plugin_enabled": {"ok": False, "error": "denied"},
         "err": err_env(RID, 5026, "denied")})
case("_plugins_toggle", {"rid": RID, "params": {"key": "cat/b"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_set_agent_plugin_enabled": {"ok": False},
         "err": err_env(RID, 5026, "toggle failed")})
case("_plugins_toggle", {"rid": RID, "params": {"key": "zzz"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_set_agent_plugin_enabled": {**TOG_OK, "name": "zzz"},
         "plugin_rows": script(ROWS),
         "ok": ok_env(RID, {"ok": True, "unchanged": False, "restart_required": True,
                            "gateway_reloaded": True, "activation": None,
                            "name": "zzz", "plugin": None})})
case("_plugins_toggle", {"rid": RID, "params": {"key": 5}})   # AttributeError .strip
case("_plugins_toggle", {"rid": RID, "params": {"key": "cat/b"}},
     io={"ensure_plugin_activation_listener": raise_script("SystemExit", "boom")})
case("_plugins_toggle", {"rid": RID, "params": {"key": "cat/b"}},
     io={"ensure_plugin_activation_listener": None}, fail_closed=True)

# ── _plugins_install ──
INST_OK = {"ok": True, "enabled": True, "plugin_name": "plug-a", "name": "cat/plug-a"}
case("_plugins_install", {"rid": RID, "params": {"identifier": "user/repo", "force": True}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_install_plugin": INST_OK,
         "ok": ok_env(RID, {**INST_OK, "activation": ACT["plug-a"]})},
     state={"_plugin_activations": ACT, "_plugin_activation_subscribed": []})
case("_plugins_install", {"rid": RID, "params": {"identifier": "user/repo"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_install_plugin": {**INST_OK, "enabled": False},
         "ok": ok_env(RID, {**INST_OK, "enabled": False})})
case("_plugins_install", {"rid": RID, "params": {"repo": "user/repo"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_install_plugin": {"ok": False, "source_moved": True},
         "err": err_env(RID, 5026, "install failed", {"reason": "source_moved"})})
case("_plugins_install", {"rid": RID, "params": {"identifier": "x", "enable": False}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_install_plugin": {"ok": False, "exists": True, "error": "dup"},
         "err": err_env(RID, 5026, "dup", {"reason": "exists"})})
case("_plugins_install", {"rid": RID, "params": {}},
     io={"err": err_env(RID, 4019,
                        "plugins.install requires 'identifier', 'repo', or 'catalog_name'")})
case("_plugins_install", {"rid": RID, "params": {"catalog_name": " curated "}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_install_plugin": {**INST_OK, "enabled": False},
         "ok": ok_env(RID, {**INST_OK, "enabled": False})})
case("_plugins_install", {"rid": RID,
                          "params": {"identifier": "x", "ref": " abc ", "inspected_sha": "sha1"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_install_plugin": {**INST_OK, "enabled": False},
         "ok": ok_env(RID, {**INST_OK, "enabled": False})})
case("_plugins_install", {"rid": RID, "params": {"identifier": "x"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_install_plugin": raise_script("SystemExit", "boom")})
case("_plugins_install", {"rid": RID, "params": {"identifier": "x"}},
     io={"ensure_plugin_activation_listener": None}, fail_closed=True)

# ── _plugins_inspect ──
case("_plugins_inspect", {"rid": RID, "params": {"identifier": "user/repo"}},
     io={"tools_mod": "hermes_cli.plugins_cmd_inspect",
         "plugins_cmd_inspect.inspect_plugin_source": {"halves": ["agent"]},
         "ok": ok_env(RID, {"halves": ["agent"]})})
case("_plugins_inspect", {"rid": RID, "params": {"catalog_name": "curated"}},
     io={"tools_mod": "hermes_cli.plugins_cmd_inspect",
         "plugins_cmd_inspect.inspect_plugin_source": {"halves": []},
         "ok": ok_env(RID, {"halves": []})})
case("_plugins_inspect", {"rid": RID, "params": {}},
     io={"err": err_env(RID, 4019,
                        "plugins.inspect requires 'identifier', 'repo', or 'catalog_name'")})
case("_plugins_inspect", {"rid": RID, "params": {"identifier": "x"}},
     io={"tools_mod": "hermes_cli.plugins_cmd_inspect",
         "plugins_cmd_inspect.inspect_plugin_source": raise_script("ValueError", "unreadable")})
case("_plugins_inspect", {"rid": RID, "params": {"catalog_name": 7}},
     io={"tools_mod": "hermes_cli.plugins_cmd_inspect",
         "plugins_cmd_inspect.inspect_plugin_source": {"halves": []},
         "ok": ok_env(RID, {"halves": []})})
case("_plugins_inspect", {"rid": RID, "params": {"identifier": "x"}},
     io={}, fail_closed=True)

# ── _plugins_install_desktop ──
case("_plugins_install_desktop", {"rid": RID, "params": {"identifier": "user/repo", "ref": "abc"}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.install_desktop_plugin": {"ok": True, "name": "repo"},
         "ok": ok_env(RID, {"ok": True, "name": "repo"})})
case("_plugins_install_desktop", {"rid": RID, "params": {"identifier": "x"}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.install_desktop_plugin": {"ok": False, "exists": True},
         "err": err_env(RID, 5026, "desktop install failed", {"reason": "exists"})})
case("_plugins_install_desktop", {"rid": RID, "params": {}},
     io={"err": err_env(RID, 4019,
                        "plugins.manage install_desktop requires 'identifier', 'repo', or 'catalog_name'")})
case("_plugins_install_desktop", {"rid": RID,
                                  "params": {"repo": "u/r", "force": True, "catalog_name": "c"}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.install_desktop_plugin": {"ok": True},
         "ok": ok_env(RID, {"ok": True})})
case("_plugins_install_desktop", {"rid": RID, "params": {"identifier": "x"}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.install_desktop_plugin": raise_script("KeyboardInterrupt", "")})
case("_plugins_install_desktop", {"rid": RID, "params": {"identifier": "x"}},
     io={}, fail_closed=True)

# ── _plugins_remove_desktop ──
case("_plugins_remove_desktop", {"rid": RID, "params": {"name": "p1"}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.remove_desktop_plugin": {"ok": True, "removed": True},
         "ok": ok_env(RID, {"ok": True, "removed": True})})
case("_plugins_remove_desktop", {"rid": RID, "params": {"key": "cat/p1"}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.remove_desktop_plugin": {"ok": False, "error": "bundled"},
         "err": err_env(RID, 5026, "bundled")})
case("_plugins_remove_desktop", {"rid": RID, "params": {"name": "p1", "force": True}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.remove_desktop_plugin": {"ok": False},
         "err": err_env(RID, 5026, "desktop remove failed")})
case("_plugins_remove_desktop", {"rid": RID, "params": {}},
     io={"err": err_env(RID, 4019, "plugins.manage remove_desktop requires a 'name'")})
case("_plugins_remove_desktop", {"rid": RID, "params": {"name": "p1"}},
     io={"tools_mod": "hermes_cli.desktop_plugin_install",
         "desktop_plugin_install.remove_desktop_plugin": raise_script("RuntimeError", "io")})
case("_plugins_remove_desktop", {"rid": RID, "params": {"name": "p1"}},
     io={}, fail_closed=True)

# ── _plugins_update ──
UPD_OK = {"ok": True, "name": "cat/b", "unchanged": False, "sha": "abc123",
          "gateway_reloaded": True, "warnings": ["w1"]}
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_update_user_plugin": UPD_OK,
         "ok": ok_env(RID, {"ok": True, "unchanged": False, "name": "cat/b",
                            "warnings": ["w1"], "sha": "abc123", "gateway_reloaded": True,
                            "activation": ACT["plug-a"]})},
     state={"_plugin_activations": ACT, "_plugin_activation_subscribed": []})
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_update_user_plugin": {**UPD_OK, "unchanged": True},
         "ok": ok_env(RID, {"ok": True, "unchanged": True, "name": "cat/b",
                            "warnings": ["w1"], "sha": "abc123", "gateway_reloaded": True})})
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b", "accept_capabilities": True}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_update_user_plugin":
             {"ok": False, "consent_required": True, "delta": ["+tool"], "name": "cat/b",
              "extra_ignored": 1},
         "ok": ok_env(RID, {"ok": False, "name": "cat/b", "consent_required": True,
                            "delta": ["+tool"]})})
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b", "force": True}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_update_user_plugin":
             {"ok": False, "scan_blocked": True, "scan_verdict": "caution",
              "scan_findings": ["f1"], "error": "blocked"},
         "ok": ok_env(RID, {"ok": False, "error": "blocked", "scan_blocked": True,
                            "scan_verdict": "caution", "scan_findings": ["f1"]})})
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_update_user_plugin": {"ok": False, "error": "nope"},
         "err": err_env(RID, 4021, "nope")})
case("_plugins_update", {"rid": RID, "params": {}},
     io={"err": err_env(RID, 4019, "plugins.update requires a 'name'")})
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b"}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_update_user_plugin":
             {"ok": True, "unchanged": True, "warnings": None, "restart_required": False},
         "ok": ok_env(RID, {"ok": True, "unchanged": True, "name": "cat/b",
                            "warnings": [], "restart_required": False})})
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b"}},
     io={"ensure_plugin_activation_listener": raise_script("GeneratorExit", "")})
case("_plugins_update", {"rid": RID, "params": {"name": "cat/b"}},
     io={"ensure_plugin_activation_listener": None}, fail_closed=True)

# ── _plugins_remove ──
case("_plugins_remove", {"rid": RID, "params": {"name": "p1"}},
     io={"tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_remove_user_plugin": {"ok": True, "removed": True},
         "ok": ok_env(RID, {"ok": True, "removed": True})})
case("_plugins_remove", {"rid": RID, "params": {"name": "p1"}},
     io={"tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_remove_user_plugin": {"ok": False, "error": "bundled"},
         "err": err_env(RID, 5026, "bundled")})
case("_plugins_remove", {"rid": RID, "params": {"name": "p1"}},
     io={"tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_remove_user_plugin": {"ok": False},
         "err": err_env(RID, 5026, "remove failed")})
case("_plugins_remove", {"rid": RID, "params": {"name": "  "}},
     io={"err": err_env(RID, 4019, "plugins.remove requires a 'name'")})
case("_plugins_remove", {"rid": RID, "params": {"name": "p1"}},
     io={"tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd.dashboard_remove_user_plugin": raise_script("PermissionError", "ro")})
case("_plugins_remove", {"rid": RID, "params": {"name": "p1"}},
     io={}, fail_closed=True)

# ── _portable_server_names ──
case("_portable_server_names", {"plugin_dir": "/h/plugins/p1", "key": "cat/p1"},
     io={"tools_mod": ["hermes_cli.agent_plugins", "hermes_cli.plugins_manifest"],
         "agent_plugins.load_agent_plugin": {"mcp_servers": ["s2", "s1"]},
         "plugins_manifest.portable_mcp_server_name": ["cat/p1:s2", "cat/p1:s1"]})
case("_portable_server_names", {"plugin_dir": "/h/plugins/p1", "key": "cat/p1"},
     io={"tools_mod": ["hermes_cli.agent_plugins", "hermes_cli.plugins_manifest"],
         "agent_plugins.load_agent_plugin": {"mcp_servers": ["s1", "s1"]},
         "plugins_manifest.portable_mcp_server_name": ["cat/p1:s1", "cat/p1:s1"]})
case("_portable_server_names", {"plugin_dir": "/h/plugins/p1", "key": "cat/p1"},
     io={"tools_mod": ["hermes_cli.agent_plugins", "hermes_cli.plugins_manifest"],
         "agent_plugins.load_agent_plugin": {"mcp_servers": []}})
case("_portable_server_names", {"plugin_dir": "/h/plugins/p1", "key": "cat/p1"},
     io={"tools_mod": ["hermes_cli.agent_plugins", "hermes_cli.plugins_manifest"],
         "agent_plugins.load_agent_plugin": {"mcp_servers": ["s1"]},
         "plugins_manifest.portable_mcp_server_name": raise_script("ValueError", "bad")})
case("_portable_server_names", {"plugin_dir": "/h/plugins/p1", "key": "cat/p1"},
     io={"tools_mod": "hermes_cli.agent_plugins",
         "agent_plugins.load_agent_plugin": raise_script("ImportError", "no pkg")})
case("_portable_server_names", {"plugin_dir": "/h/plugins/p1", "key": "cat/p1"},
     io={"tools_mod": ["hermes_cli.agent_plugins", "hermes_cli.plugins_manifest"],
         "agent_plugins.load_agent_plugin": {"other": 1}})
case("_portable_server_names", {"plugin_dir": "/h/plugins/p1", "key": "cat/p1"},
     io={}, fail_closed=True)

# ── _plugins_settings ──
FOUND = ["p1", "1.0", "desc", "git", "/h/plugins/p1", "cat/p1"]
SETTINGS_IO = {
    "tools_mod": ["hermes_cli.plugins_cmd", "hermes_cli.plugins_settings",
                  "hermes_cli.plugin_events", "tools.mcp_tool_discovery"],
    "plugins_cmd._find_plugin_entry": script(FOUND),
    "plugins_settings.save_plugin_settings": script(["k1"]),
    "plugin_rows": script(ROWS),
    "plugin_events.broadcast_plugin_event": None,
    "plugins_cmd._is_portable_plugin_dir": True,
    "portable_server_names": script(["cat/p1:s1"]),
    "spawn_context_thread": {"id": "th1"},
    "thread.start": None,
}
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {"k1": "v"}}},
     io={**SETTINGS_IO,
         "ok": ok_env(RID, {"ok": True, "name": "cat/p1", "written": ["k1"],
                            "plugin": ROWS[1]})})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {}}},
     io={**SETTINGS_IO, "plugins_cmd._is_portable_plugin_dir": False,
         "ok": ok_env(RID, {"ok": True, "name": "cat/p1", "written": ["k1"],
                            "plugin": ROWS[1]})})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {"a": 1}}},
     io={**SETTINGS_IO, "portable_server_names": script([]),
         "ok": ok_env(RID, {"ok": True, "name": "cat/p1", "written": ["k1"],
                            "plugin": ROWS[1]})})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat.p1", "values": {"a": 1}}},
     io={**SETTINGS_IO,
         "plugins_cmd._find_plugin_entry": script(
             ["p1", "1.0", "desc", "git", "/h/plugins/p1", "cat.p1"]),
         "ok": ok_env(RID, {"ok": True, "name": "cat.p1", "written": ["k1"],
                            "plugin": None})})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": []}},
     io={"err": err_env(RID, 4019,
                        "plugins.settings requires a 'key' and a 'values' mapping")})
case("_plugins_settings", {"rid": RID, "params": {"values": {}}},
     io={"err": err_env(RID, 4019,
                        "plugins.settings requires a 'key' and a 'values' mapping")})
case("_plugins_settings", {"rid": RID, "params": {"key": "nope", "values": {}}},
     io={"tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd._find_plugin_entry": None,
         "err": err_env(RID, 4020, "plugin 'nope' not found")})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {}}},
     io={"tools_mod": "hermes_cli.plugins_cmd",
         "plugins_cmd._find_plugin_entry": script(["a", "b"])})   # unpack ValueError
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {}}},
     io={**SETTINGS_IO,
         "plugins_settings.save_plugin_settings": raise_script("ValueError", "secret refused"),
         "err": err_env(RID, 4021, "secret refused")})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {}}},
     io={**SETTINGS_IO,
         "plugins_settings.save_plugin_settings": raise_script("PermissionError", "read-only"),
         "err": err_env(RID, 4021, "read-only")})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {}}},
     io={**SETTINGS_IO,
         "plugins_settings.save_plugin_settings": raise_script("SystemExit", "boom")})
case("_plugins_settings", {"rid": RID, "params": {"key": "cat/p1", "values": {}}},
     io={"tools_mod": "hermes_cli.plugins_cmd"}, fail_closed=True)

# ── _plugins_update_all ──
case("_plugins_update_all", {"rid": RID, "params": {}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_auto_update",
         "plugins_auto_update.update_all_unattended": {"updated": ["a"], "needs_review": []},
         "ok": ok_env(RID, {"updated": ["a"], "needs_review": []})})
case("_plugins_update_all", {"rid": RID, "params": {}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_auto_update",
         "plugins_auto_update.update_all_unattended": {},
         "ok": ok_env(RID, {})})
case("_plugins_update_all", {"rid": RID, "params": {}},
     io={"ensure_plugin_activation_listener": raise_script("RuntimeError", "listener")})
case("_plugins_update_all", {"rid": RID, "params": {}},
     io={"ensure_plugin_activation_listener": None,
         "tools_mod": "hermes_cli.plugins_auto_update",
         "plugins_auto_update.update_all_unattended": raise_script("OSError", "disk")})
case("_plugins_update_all", {"rid": RID, "params": {}},
     io={"ensure_plugin_activation_listener": None}, fail_closed=True)

# ── _plugins_auto_update ──
case("_plugins_auto_update", {"rid": RID, "params": {}},
     io={"tools_mod": "hermes_cli.plugins_auto_update",
         "plugins_auto_update.auto_update_state": {"enabled": True},
         "ok": ok_env(RID, {"enabled": True})})
case("_plugins_auto_update", {"rid": RID, "params": {"enable": None}},
     io={"tools_mod": "hermes_cli.plugins_auto_update",
         "plugins_auto_update.auto_update_state": {"enabled": False},
         "ok": ok_env(RID, {"enabled": False})})
case("_plugins_auto_update", {"rid": RID, "params": {"enable": True}},
     io={"tools_mod": "hermes_cli.plugins_auto_update",
         "plugins_auto_update.set_auto_update": {"enabled": True},
         "ok": ok_env(RID, {"enabled": True})})
case("_plugins_auto_update", {"rid": RID, "params": {"enable": False}},
     io={"tools_mod": "hermes_cli.plugins_auto_update",
         "plugins_auto_update.set_auto_update": {"enabled": False},
         "ok": ok_env(RID, {"enabled": False})})
case("_plugins_auto_update", {"rid": RID, "params": {"enable": True}},
     io={"tools_mod": ["hermes_cli.plugins_auto_update", "hermes_cli.plugins_cmd"],
         "plugins_auto_update.set_auto_update":
             raise_script("PluginOperationError", "cannot disable"),
         "err": err_env(RID, 4021, "cannot disable")})
case("_plugins_auto_update", {"rid": RID, "params": {"enable": True}},
     io={"tools_mod": ["hermes_cli.plugins_auto_update", "hermes_cli.plugins_cmd"],
         "plugins_auto_update.set_auto_update": raise_script("ValueError", "bad")})
case("_plugins_auto_update", {"rid": RID, "params": {"enable": True}},
     io={"tools_mod": ["hermes_cli.plugins_auto_update", "hermes_cli.plugins_cmd"],
         "plugins_auto_update.set_auto_update": raise_script("SystemExit", "boom")})
case("_plugins_auto_update", {"rid": RID, "params": {"enable": True}},
     io={"tools_mod": "hermes_cli.plugins_auto_update"}, fail_closed=True)

# ── _plugins_onboarding ──
case("_plugins_onboarding", {"rid": RID, "params": {}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.onboarding_entries": script([{"name": "a", "installed": False}]),
         "ok": ok_env(RID, {"onboarding": [{"name": "a", "installed": False}]})})
case("_plugins_onboarding", {"rid": RID, "params": {}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.onboarding_entries": script([]),
         "ok": ok_env(RID, {"onboarding": []})})
case("_plugins_onboarding", {"rid": RID, "params": {}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.onboarding_entries": raise_script("RuntimeError", "x")})
case("_plugins_onboarding", {"rid": RID, "params": {}},
     io={"tools_mod": raise_script("ImportError", "no module")})
case("_plugins_onboarding", {"rid": RID, "params": {}}, io={}, fail_closed=True)

# ── _plugins_presence ──
case("_plugins_presence", {"rid": RID, "params": {"names": ["a", "b"]}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.presence_for": script([{"name": "a"}, {"name": "b"}]),
         "ok": ok_env(RID, {"presence": [{"name": "a"}, {"name": "b"}]})})
case("_plugins_presence", {"rid": RID, "params": {"names": None}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.presence_for": script([]),
         "ok": ok_env(RID, {"presence": []})})
case("_plugins_presence", {"rid": RID, "params": {}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.presence_for": script([]),
         "ok": ok_env(RID, {"presence": []})})
case("_plugins_presence", {"rid": RID, "params": {"names": ["a"]}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.presence_for": raise_script("ValueError", "bad names")})
case("_plugins_presence", {"rid": RID, "params": {"names": []}},
     io={"tools_mod": "hermes_cli.plugin_catalog_presence",
         "plugin_catalog_presence.presence_for": script([]),
         "ok": ok_env(RID, {"presence": []})})
case("_plugins_presence", {"rid": RID, "params": {"names": ["a"]}},
     io={}, fail_closed=True)


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = [f for f, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"generator: funcs with <5 cases: {thin}")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"wrote {OUT}: {len(CASES)} cases across {len(counts)} funcs")
    for func in sorted(counts):
        print(f"  {func}: {counts[func]}")


if __name__ == "__main__":
    main()