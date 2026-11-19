#!/usr/bin/env python3
"""gen_plugin_revive_cases.py — deterministic fixture generator for the
plugin_revive differential oracle. Pure data construction (no quarry imports):
the ORACLE computes expected values from the live defs; this file only builds
inputs, initial registry state, and scripted seam results.

Regenerate: python tests/gen_plugin_revive_cases.py
Output:     tests/fixtures/plugin_revive.jsonl  (>=5 cases per ported fn)

io script conventions (shared with the C harness stub):
  io[seam] = value            -> reused for every call of that seam
  io[seam] = {"__list__": []} -> consumed one element per call (last repeats)
  {"__raise__": cls, "msg": m} -> raised exception (m is str(exc); KeyError m
                                  is the raw key, repr'd on both sides)
  discover_and_load script: {"ret": null, "set_discovered": bool} — the stubs
  apply the registry effect; a raise forces _discovered=False.
State contracts: has_middleware_attr=False => middleware={};
has_discovered_attr=False => discovered=False and no discover effect scripts.
"""
import json
from pathlib import Path

CASES = []


def add(fn, inp=None, state=None, io=None):
    CASES.append({"fn": fn, "in": inp or {}, "state": state or {}, "io": io or {}})


def raise_script(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


# ── _env_enabled ──
for val, want in [("1", True), ("TRUE", True), (" yes ", True), ("0", False),
                  (None, False), ("", False), ("on\t\n", True), ("no", False),
                  ("\u00a0true\u00a0", True), ("False", False)]:
    add("_env_enabled", {"name": "HERMES_X"}, io={"getenv": val})

# ── _install_plugin_debug_handler ──
add("_install_plugin_debug_handler", {"force": False}, {"plugins_debug": False})
add("_install_plugin_debug_handler", {"force": False},
    {"plugins_debug": True, "debug_handler_installed": False})
add("_install_plugin_debug_handler", {"force": False},
    {"plugins_debug": True, "debug_handler_installed": True})
add("_install_plugin_debug_handler", {"force": True},
    {"plugins_debug": False}, {"getenv": "1"})
add("_install_plugin_debug_handler", {"force": True},
    {"plugins_debug": True}, {"getenv": "0"})
add("_install_plugin_debug_handler", {"force": True},
    {"plugins_debug": False, "debug_handler_installed": True}, {"getenv": "1"})

# ── _tool_override_allowed ──
def cfg_entry(entry):
    return {"plugins": {"entries": {"p1": entry}}}

add("_tool_override_allowed", {"name": "write_file"}, {"manifest_source": "bundled"})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": cfg_entry({"granted_capabilities": ["tools.override"]})})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": cfg_entry({"allow_tool_override": True})})
add("_tool_override_allowed", {"name": "write_file"}, {}, {"load_config": cfg_entry({})})
add("_tool_override_allowed", {"name": "write_file"}, {}, {"load_config": None})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": raise_script("RuntimeError", "config unreadable")})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"import_config": raise_script("ModuleNotFoundError", "No module named 'hermes_cli.config'")})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": cfg_entry({"granted_capabilities": ["llm.model_override"],
                               "allow_tool_override": False})})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": cfg_entry({"granted_capabilities": [" tools.override "]})})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": cfg_entry({"allow_tool_override": "yes"})})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": cfg_entry({"allow_tool_override": 0})})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": cfg_entry("not-a-dict")})
add("_tool_override_allowed", {"name": "write_file"}, {},
    {"load_config": raise_script("SystemExit", "config exit")})
add("_tool_override_allowed", {"name": "write_file"}, {}, {"load_config": [1, 2]})
add("_tool_override_allowed", {"name": "write_file"},
    {"manifest_source": "project", "plugin_id": "p1", "home_path": "/h/proj"},
    {"load_config": cfg_entry({"allow_tool_override": True})})

# ── _platform_name_from_manifest ──
for mf in [{"name": "telegram-platform", "path": "/x/y"},
           {"name": "discord", "path": "/plugins/discord"},
           {"name": "discord", "path": "/plugins/discord/"},
           {"name": "slack", "path": None},
           {"name": "", "path": "a/b/c"},
           {"name": "-platform", "path": None},
           {"name": "x-platform", "path": "C:\\plug\\x"},
           {"name": "win", "path": "p2\\sub\\"},
           {"name": "dots", "path": "a/./b"},
           {"name": "up", "path": "a/.."}]:
    add("_platform_name_from_manifest", {"manifest": mf})
add("_platform_name_from_manifest", {"manifest": None})

# ── _load_directory_module ──
def dir_state(**kw):
    st = {"sys_modules": {}, "bare_module_scope": {}}
    st.update(kw)
    return st

def dir_io(exists=True, spec="spec:1", mod="mod:1", exec_script=None):
    io = {"fs.exists": exists, "module_from_spec": mod}
    if spec is not None:
        io["spec_from_file_location"] = spec
    if exec_script is not None:
        io["exec_module"] = exec_script
    return io

MF1 = {"name": "p1", "path": "p1", "key": ""}
add("_load_directory_module", {"dir": "p1", "manifest": MF1}, dir_state(), dir_io())
add("_load_directory_module", {"dir": "p1", "manifest": MF1}, dir_state(), dir_io(exists=False))
add("_load_directory_module", {"dir": "p1", "manifest": MF1}, dir_state(), dir_io(spec=None))
add("_load_directory_module", {"dir": "p1", "manifest": MF1}, dir_state(),
    dir_io(exec_script=raise_script("ValueError", "boom")))
add("_load_directory_module", {"dir": "p1", "manifest": MF1}, dir_state(),
    dir_io(exec_script=raise_script("SystemExit", "die")))
old_mods = {
    "hermes_plugins.p1": {"kind": "mod", "token": "old:1", "package": "hermes_plugins.p1", "path": ["p1"]},
    "hermes_plugins.p1.sub": {"kind": "mod", "token": "old:2", "package": "hermes_plugins.p1.sub", "path": ["p1"]},
    "unrelated": {"kind": "mod", "token": "u:1", "package": "unrelated", "path": []},
}
add("_load_directory_module", {"dir": "p1", "manifest": MF1},
    dir_state(sys_modules=dict(old_mods)), dir_io())
add("_load_directory_module", {"dir": "p1", "manifest": MF1, "module_name": "custom.mod"},
    dir_state(), dir_io())
add("_load_directory_module",
    {"dir": "p1", "manifest": {"name": "openai", "path": "p1", "key": "image_gen/openai"}},
    dir_state(), dir_io())
add("_load_directory_module", {"dir": "p1", "manifest": MF1},
    dir_state(bare_module_scope={"hermes_plugins.p1": "/h/other"}, scope_key="/h/s1"), dir_io())
add("_load_directory_module", {"dir": "p1", "manifest": MF1},
    dir_state(bare_module_scope={"hermes_plugins.p1": "/h/s1"}, scope_key="/h/s1"), dir_io())
add("_load_directory_module", {"dir": "p1", "manifest": MF1}, dir_state(),
    dir_io(spec={"token": "spec:2", "loader": None}))
add("_load_directory_module", {"dir": "p2/sub", "manifest": {"name": "p2", "path": "p2/sub", "key": ""}},
    dir_state(), dir_io())
add("_load_directory_module", {"dir": "p2/sub", "manifest": {"name": "p2", "path": "p2/sub", "key": ""}},
    dir_state(), dir_io(exists=False))

# ── _load_entrypoint_module ──
MFE = {"name": "ep1", "path": None, "key": ""}
EPS = [{"name": "other", "group": "hermes_agent.plugins"},
       {"name": "ep1", "group": "hermes_agent.plugins"}]
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": {"kind": "list", "eps": EPS}, "ep.load": "mod:ep1"})
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": {"kind": "select", "eps": EPS}, "ep.select": EPS, "ep.load": "mod:ep1"})
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": {"kind": "dict", "groups": {"hermes_agent.plugins": EPS, "g2": []}},
     "ep.load": "mod:ep1"})
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": {"kind": "list", "eps": [{"name": "nope", "group": "hermes_agent.plugins"}]}})
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": {"kind": "list", "eps": EPS},
     "ep.load": raise_script("ImportError", "no module named ep1")})
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": {"kind": "list", "eps": EPS}, "ep.load": None})
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": {"kind": "list",
                      "eps": [{"name": "ep1", "group": "wrong.group"},
                              {"name": "ep1", "group": "hermes_agent.plugins"}]},
     "ep.load": "fn:register"})
add("_load_entrypoint_module", {"path": "x", "manifest": MFE}, {},
    {"entry_points": raise_script("RuntimeError", "metadata scan failed")})

# ── has_hook ──
H_STATE = {"hooks": {"pre_tool_call": ["cb1"]}, "discovered": True}
add("has_hook", {"name": "pre_tool_call"}, dict(H_STATE))
add("has_hook", {"name": "missing"}, dict(H_STATE))
add("has_hook", {"name": "x"}, {"hooks": {"x": []}, "discovered": True})
add("has_hook", {"name": "pre_tool_call"}, dict(H_STATE), {"safe_worker_enabled": True})
add("has_hook", {"name": "pre_tool_call"}, {"hooks": {"pre_tool_call": ["cb1"]}, "discovered": False},
    {"discover_and_load": {"ret": None, "set_discovered": True}})
add("has_hook", {"name": "pre_tool_call"}, {"hooks": {}, "discovered": False},
    {"discover_and_load": raise_script("RuntimeError", "sweep failed")})
add("has_hook", {"name": "pre_tool_call"},
    {"hooks": {"pre_tool_call": ["cb1"]}, "discovered": True, "bg_present": True},
    {"bg.is_alive": {"__list__": [True, False]}, "bg.join": None, "in_plugin_load_worker": False})
add("has_hook", {"name": "pre_tool_call"},
    {"hooks": {"pre_tool_call": ["cb1"]}, "discovered": True, "bg_present": True},
    {"bg.is_alive": {"__list__": [True, True]}, "in_plugin_load_worker": True})
add("has_hook", {"name": "pre_tool_call"},
    {"hooks": {"pre_tool_call": ["cb1"]}, "discovered": True,
     "bg_present": True, "bg_is_current": True},
    {"bg.is_alive": True})
add("has_hook", {"name": "pre_tool_call"},
    {"hooks": {"pre_tool_call": ["cb1"]}, "discovered": True,
     "bg_present": True, "delivery_join_abandoned": True},
    {"bg.is_alive": True})
add("has_hook", {"name": "pre_tool_call"},
    {"hooks": {"pre_tool_call": ["cb1"]}, "has_discovered_attr": False})

# ── has_middleware ──
M_STATE = {"middleware": {"request": ["cb"]}, "discovered": True}
add("has_middleware", {"name": "request"}, dict(M_STATE))
add("has_middleware", {"name": "missing"}, dict(M_STATE))
add("has_middleware", {"name": "request"},
    {"middleware": {"request": ["cb"]}, "discovered": True,
     "manager_has_middleware_method": False})
add("has_middleware", {"name": "request"},
    {"middleware": {}, "discovered": True, "manager_has_middleware_method": False,
     "has_middleware_attr": False})
add("has_middleware", {"name": "request"}, dict(M_STATE), {"safe_worker_enabled": True})
add("has_middleware", {"name": "request"},
    {"middleware": {"request": []}, "discovered": True,
     "manager_has_middleware_method": False})
add("has_middleware", {"name": "request"},
    {"middleware": {"request": ["cb"]}, "discovered": False},
    {"discover_and_load": {"ret": None, "set_discovered": True}})

# ── find_plugin_skill ──
SK = {"p1:s": {"path": {"__path__": "skills/p1/s/SKILL.md"}, "bare_name": "s"}}
add("find_plugin_skill", {"name": "p1:s"}, {"plugin_skills": dict(SK)})
add("find_plugin_skill", {"name": "absent"}, {"plugin_skills": dict(SK)})
add("find_plugin_skill", {"name": "p1:e"}, {"plugin_skills": {"p1:e": {}}})
add("find_plugin_skill", {"name": "p1:n"}, {"plugin_skills": {"p1:n": {"bare_name": "n"}}})
add("find_plugin_skill", {"name": "p1:p"}, {"plugin_skills": {"p1:p": {"path": "x/y.md"}}})
add("find_plugin_skill", {"name": "p1:z"}, {"plugin_skills": {"p1:z": {"path": None}}})

# ── remove_plugin_skill ──
add("remove_plugin_skill", {"name": "p1:s"}, {"plugin_skills": dict(SK)})
add("remove_plugin_skill", {"name": "absent"}, {"plugin_skills": dict(SK)})
add("remove_plugin_skill", {"name": "p1:e"}, {"plugin_skills": {"p1:e": {}}})
add("remove_plugin_skill", {"name": "b"},
    {"plugin_skills": {"a": {"path": "a"}, "b": {"path": "b"}, "c": {"path": "c"}}})
add("remove_plugin_skill", {"name": "p1:s"}, {"plugin_skills": {}})

# ── get_pre_tool_call_directive ──
def d_case(results, inp_extra=None, state=None, io_extra=None):
    inp = {"tool_name": "shell_exec", "args": {"command": "ls"}, "hook_kwargs": {}}
    if inp_extra:
        inp.update(inp_extra)
    io = {"lifecycle.invoke_hook": results}
    if io_extra:
        io.update(io_extra)
    add("get_pre_tool_call_directive", inp, state, io)

d_case([])
d_case([{"action": "block", "message": "no shell"}])
d_case([{"action": "approve", "message": "ask human", "rule_key": " rk "}])
d_case([{"action": "approve", "message": "ask"}, {"action": "block", "message": "veto"}])
d_case([{"action": "block"}])
d_case([{"action": "block", "message": ""}])
d_case([{"action": "modify", "args": {"a": 1}}, {"action": "block", "message": "b"}])
d_case([None, "x", 5, {"action": "approve"}])
d_case([{"action": "block", "message": 5}])
d_case([], {"tool_name": "shell_exec"}, {"whitelist_allowed": ["other"]})
d_case([], {"tool_name": "shell_exec"}, {"whitelist_allowed": ["shell_exec"]})
d_case([], {"tool_name": "shell_exec"}, {"whitelist_allowed": []})
d_case([], {"tool_name": "shell_exec"},
       {"whitelist_allowed": ["other"], "whitelist_fmt": "no {tool_name} here"})
d_case([], {"tool_name": "shell_exec"},
       {"whitelist_allowed": ["other"], "whitelist_fmt": "{{literal}} {tool_name}"})
d_case(raise_script("RuntimeError", "hook bus down"))
d_case([{"action": "modify", "args": {"a": 1}}, {"action": "approve", "message": "ok"}],
       {"args": None})
d_case([{"action": "approve", "message": "ok"}],
       {"hook_kwargs": {"task_id": "t1", "session_id": "s1", "tool_call_id": "tc1",
                        "turn_id": "tu1", "api_request_id": "ar1", "conversation_id": "c1",
                        "middleware_trace": [{"m": 1}]}})
d_case([{"action": "approve", "message": "first"}, {"action": "approve", "message": "second"}])
d_case([{"action": "modify", "args": {}}, {"action": "modify", "args": {"b": 2}},
        {"action": "approve", "message": "m"}])

# ── get_pre_tool_call_block_message ──
def b_case(results, state=None):
    add("get_pre_tool_call_block_message",
        {"tool_name": "shell_exec", "args": {}, "hook_kwargs": {}},
        state, {"lifecycle.invoke_hook": results})

b_case([{"action": "block", "message": "denied"}])
b_case([{"action": "approve", "message": "ask"}])
b_case([])
b_case([], {"whitelist_allowed": ["other"]})
b_case([{"action": "block"}])

# ── resolve_pre_tool_block ──
def r_case(results, io_extra=None, state=None, hook_kwargs=None):
    io = {"lifecycle.invoke_hook": results}
    if io_extra:
        io.update(io_extra)
    add("resolve_pre_tool_block",
        {"tool_name": "shell_exec", "args": {}, "hook_kwargs": hook_kwargs or {}},
        state, io)

APPR = [{"action": "approve", "message": "needs approval", "rule_key": "shell_exec:ls"}]
r_case([{"action": "block", "message": "vetoed"}])
r_case([])
r_case(APPR, {"approval.set_ctx": "tok:1", "approval.request": {"approved": True},
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": "tok:1",
              "approval.request": {"approved": False, "message": "human said no"},
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": "tok:1", "approval.request": {"approved": False},
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": "tok:1",
              "approval.request": raise_script("RuntimeError", "gate crashed"),
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": None, "approval.request": {"approved": False}})
r_case(APPR, {"approval.set_ctx": raise_script("ValueError", "ctx broken"),
              "approval.request": {"approved": True}})
r_case(APPR, {"approval.set_ctx": raise_script("SystemExit", "ctx exit")})
r_case(APPR, {"approval.set_ctx": "tok:1", "approval.request": None,
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": "tok:1",
              "approval.request": {"approved": False, "message": ""},
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": "tok:1",
              "approval.request": {"approved": False, "message": 0},
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": "tok:1",
              "approval.request": raise_script("SystemExit", "gate exit"),
              "approval.reset_ctx": None})
r_case(APPR, {"approval.set_ctx": "tok:1", "approval.request": {"approved": True},
              "approval.reset_ctx": raise_script("ValueError", "reset broken")})
r_case([{"action": "approve", "message": "m"}],
       {"approval.set_ctx": "tok:2", "approval.request": {"approved": True},
        "approval.reset_ctx": None},
       hook_kwargs={"turn_id": "tu9", "tool_call_id": "tc9", "session_id": "s9"})
r_case([{"action": "approve"}],
       {"approval.set_ctx": None, "approval.request": {"approved": False}})

# ── get_pre_verify_continue_message ──
def v_case(results, kwargs=None, state=None, io_extra=None):
    io = {"manager.invoke_hook": results}
    if io_extra:
        io.update(io_extra)
    st = {"discovered": True}
    if state:
        st.update(state)
    add("get_pre_verify_continue_message",
        {"tool_name": "vestigial", "kwargs": kwargs or {}}, st, io)

v_case([{"action": "continue", "message": " keep going "}])
v_case([{"decision": "block", "reason": " tests failed "}])
v_case([{"action": "CONTINUE", "message": "x"}])
v_case([{"action": "continue", "message": "   "}])
v_case([{"action": "finish", "message": "x"}])
v_case([None, {"action": "continue", "message": 5}])
v_case([])
v_case([{"action": "continue", "message": "go"}], {"changed_paths": "ab"})
v_case([{"action": "continue", "message": "go"}], {"changed_paths": None})
v_case(raise_script("RuntimeError", "verify bus down"))
v_case([{"action": "continue", "message": "go"}], {},
       {"discovered": False},
       {"discover_and_load": {"ret": None, "set_discovered": True}})
v_case([{"action": "continue", "reason": "r"}])
v_case([{"action": None, "decision": None}])
v_case([{"action": 0, "decision": "block", "reason": "r"}])
v_case([{"action": "continue", "message": "go"}],
       {"session_id": "s1", "platform": "cli", "model": "m1", "coding": True,
        "attempt": 2, "final_response": "done", "changed_paths": ["a.py", "b.py"]})

# ── _ensure_plugins_discovered ──
add("_ensure_plugins_discovered", {}, {"discovered": False},
    {"discover_and_load": {"ret": None, "set_discovered": True}})
add("_ensure_plugins_discovered", {}, {"discovered": True})
add("_ensure_plugins_discovered", {}, {"discovered": False},
    {"discover_and_load": raise_script("RuntimeError", "sweep failed")})
add("_ensure_plugins_discovered", {}, {"discovered": True},
    {"discover_and_load": {"ret": None, "set_discovered": False}})
add("_ensure_plugins_discovered", {}, {},
    {"get_plugin_manager": raise_script("RuntimeError", "no home")})

# ── get_plugin_context_engine ──
add("get_plugin_context_engine", {"name": "vestigial"},
    {"context_engine": "engine:mem", "discovered": True})
add("get_plugin_context_engine", {"name": "vestigial"}, {"discovered": True})
add("get_plugin_context_engine", {"name": "vestigial"},
    {"context_engine": "engine:mem", "discovered": False},
    {"discover_and_load": {"ret": None, "set_discovered": True}})
add("get_plugin_context_engine", {"name": "vestigial"}, {},
    {"discover_and_load": raise_script("RuntimeError", "sweep failed")})
add("get_plugin_context_engine", {"name": "vestigial"},
    {"context_engine": "", "discovered": True})

# ── get_plugin_command_handler ──
CMDS = {"cmd": {"handler": "h:1", "description": "d", "plugin": "p1"}}
add("get_plugin_command_handler", {"name": "cmd"},
    {"plugin_commands": dict(CMDS), "discovered": True})
add("get_plugin_command_handler", {"name": "absent"},
    {"plugin_commands": dict(CMDS), "discovered": True})
add("get_plugin_command_handler", {"name": "empty"},
    {"plugin_commands": {"empty": {}}, "discovered": True})
add("get_plugin_command_handler", {"name": "nohandler"},
    {"plugin_commands": {"nohandler": {"description": "d"}}, "discovered": True})
add("get_plugin_command_handler", {"name": "cmd"},
    {"plugin_commands": dict(CMDS), "discovered": False},
    {"discover_and_load": {"ret": None, "set_discovered": True}})
add("get_plugin_command_handler", {"name": "cmd"}, {},
    {"discover_and_load": raise_script("RuntimeError", "sweep failed")})


def main():
    out = Path(__file__).resolve().parent / "fixtures" / "plugin_revive.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["fn"]] = counts.get(c["fn"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        raise SystemExit(f"fixture coverage <5: {thin}")
    with open(out, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} fns -> {out}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()