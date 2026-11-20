#!/usr/bin/env python3
"""Generate tests/fixtures/session_policy_flow4.jsonl -- differential cases for
the gateway/session_policy.py fourth-wave FLOW port.

Deterministic, >=5 cases per ported func (10 funcs): branch boundaries,
scripted sequences, fail-closed seams (missing/empty -> RuntimeError), the FULL
builtin exception hierarchy (Exception subclasses swallowed where live
swallows; BaseException-only kinds propagate), named AuthorityRefusal
refusals, context-manager enter/body/exit traces, and mutable config state.
A list-valued seam result is wrapped as {"__list__": [...]} (both sides unwrap
it before recording the trace) so an array is a result, not a call sequence.

Run: python tests/gen_session_policy_flow4_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_policy_flow4.jsonl")

CASES = []


def C(func, io=None, **inp):
    CASES.append({"func": func, "in": dict(inp, io=io if io is not None else {})})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def L(xs):
    return {"__list__": xs}


GOOD = "work"


def full_io(**over):
    io = {
        "os.path.isdir": True,
        "validate_editor": None,
        "launch_skills": None,
        "present_sections": {"model": {"default": "old"}},
        "without_auto_pick": {"source": "cli", "cwd": "/x", "model": "m"},
        "with_entry_pick": {"source": "cli", "cwd": "/x", "model": "m"},
        "get_platform_tools": L(["a"]),
        "build_profile_terminal_scope": {"TERMINAL_ENV": "local"},
        "terminal_uses_host_cwd": True,
        "private_editor_request": None,
        "extract_config_secrets_config": None,
        "extract_config_secrets_terminal": None,
    }
    io.update(over)
    return io


BPARAMS = {"source": "cli", "cwd": "/x", "model": "m"}
BCONFIG = {"model": {"default": "old"}}

# -- active_policy --
C("active_policy", {}, active=None)
C("active_policy", {}, active={"source": "cli", "platform": "cli", "cwd": "/x"})
C("active_policy", {}, active="s")
C("active_policy", {}, active=True)
C("active_policy", {}, active=[1, 2])
C("active_policy", {}, active=5)

# -- turn_model --
C("turn_model", {"policy.read_profile": False, "policy.current_model": "gpt"}, policy={"source": "cli"}, authority="a")
C("turn_model", {"policy.read_profile": True, "policy.current_model": None}, policy={}, authority=None)
C("turn_model", {"policy.read_profile": [R("SystemExit", "bye")]}, policy={}, authority=None)
C("turn_model", {"policy.read_profile": R("ValueError", "x")}, policy={}, authority=None)
C("turn_model", {"policy.read_profile": False, "policy.current_model": [R("KeyboardInterrupt", "k")]}, policy={}, authority=None)
C("turn_model", {"policy.read_profile": False, "policy.current_model": R("ValueError", "v")}, policy={}, authority=None)
C("turn_model", {}, policy={})
C("turn_model", {"policy.read_profile": False, "policy.current_model": R("CancelledError", "c")}, policy={})

# -- profile_read_scope --
C("profile_read_scope", {"policy.read_profile": False, "body": None}, policy={"source": "cli"}, authority=None)
C("profile_read_scope", {"policy.read_profile": True, "body": None}, policy={}, authority="a")
C("profile_read_scope", {"policy.read_profile": [R("SystemExit", "bye")]}, policy={}, authority=None)
C("profile_read_scope", {"policy.read_profile": False, "body": R("ValueError", "v")}, policy={}, authority=None)
C("profile_read_scope", {"policy.read_profile": False, "body": R("BaseExceptionGroup", "b")}, policy={}, authority=None)
C("profile_read_scope", {"policy.read_profile": False, "body": R("ExceptionGroup", "e")}, policy={}, authority=None)
C("profile_read_scope", {}, policy={})

# -- _serving_local_desktop_within --
BASE_CM = {"scope.__enter__": None, "serving_local_desktop.__enter__": None,
           "serving_local_desktop.__exit__": None, "scope.__exit__": None}
C("_serving_local_desktop_within", dict(BASE_CM, body=None), scope={"TERMINAL_CWD": "/x"})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("ValueError", "v")), scope=None)
C("_serving_local_desktop_within", dict(BASE_CM, body=R("SystemExit", "s")), scope={})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("KeyError", "k"),
    **{"serving_local_desktop.__exit__": True}), scope={})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("ZeroDivisionError", "z"),
    **{"scope.__exit__": True}), scope={})
C("_serving_local_desktop_within", {"scope.__enter__": R("RuntimeError", "e")}, scope={})
C("_serving_local_desktop_within", {"scope.__enter__": None,
    "serving_local_desktop.__enter__": R("SystemExit", "s"), "scope.__exit__": None}, scope={})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("OSError", "o"),
    **{"serving_local_desktop.__exit__": R("KeyboardInterrupt", "k")}), scope={})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("UnicodeDecodeError", "u")), scope={})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("CancelledError", "c")), scope={})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("BaseException", "b")), scope={})
C("_serving_local_desktop_within", dict(BASE_CM, body=R("ExceptionGroup", "e")), scope={})

# -- local_desktop_turn --
SRC_LOCAL = {"platform": "local", "chat_id": "c"}
def pol(**kw):
    return {"policies": {"c": dict({"source": "gui"}, **kw)}}
C("local_desktop_turn", {}, source={"platform": "cli"}, scope={"k": 1})
C("local_desktop_turn", {"runner._adapter_for_source": {}}, source=SRC_LOCAL, scope="scope")
C("local_desktop_turn", {"runner._adapter_for_source": pol(source="cli")}, source=SRC_LOCAL, scope="scope")
C("local_desktop_turn", {"runner._adapter_for_source": pol(config_json='{"desktop_local": true}')}, source=SRC_LOCAL, scope="scope")
C("local_desktop_turn", {"runner._adapter_for_source": pol(request_json='{"desktop_local": true}')}, source=SRC_LOCAL, scope="scope")
C("local_desktop_turn", {"runner._adapter_for_source": pol(config_json="{}")}, source=SRC_LOCAL, scope="scope")
C("local_desktop_turn", {"runner._adapter_for_source": [R("SystemExit", "x")]}, source=SRC_LOCAL, scope="scope")
C("local_desktop_turn", {"runner._adapter_for_source": pol(config_json='{"desktop_local": false}',
    request_json='{"desktop_local": true}')}, source=SRC_LOCAL, scope="scope")
C("local_desktop_turn", {}, source=SRC_LOCAL, scope="scope")  # fail-closed runner

# -- _profile_workspace_cwd --
C("_profile_workspace_cwd", {"profile_name_for_home": None}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": "default"}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD, "read_user_config_raw": {}}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD,
    "read_user_config_raw": {"terminal": {"cwd": "auto"}}}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD,
    "read_user_config_raw": {"terminal": {"cwd": "/a/b"}}, "os.path.isdir": True}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD,
    "read_user_config_raw": {"terminal": {"cwd": "/a/b"}}, "os.path.isdir": False}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD,
    "read_user_config_raw": {"terminal": {"cwd": " . "}}}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD,
    "read_user_config_raw": {"terminal": {"cwd": "  /a/./b/  "}}, "os.path.isdir": True}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": [R("SystemExit", "x")]}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD,
    "read_user_config_raw": R("ValueError", "v")}, home="/h")
C("_profile_workspace_cwd", {"profile_name_for_home": GOOD,
    "read_user_config_raw": {"terminal": {"cwd": "/a/b"}}, "os.path.isdir": R("KeyboardInterrupt", "k")}, home="/h")

# -- _unnamed_extension_toolsets --
C("_unnamed_extension_toolsets", {"get_plugin_toolset_keys": None, "enabled_mcp_server_names": None},
  explicit=["all"], requested=[], config={}, toolsets=[])
C("_unnamed_extension_toolsets", {"get_plugin_toolset_keys": None, "enabled_mcp_server_names": None},
  explicit=["*"], requested=[], config={}, toolsets=[])
C("_unnamed_extension_toolsets", {"get_plugin_toolset_keys": L([]), "enabled_mcp_server_names": L([])},
  explicit=[], requested=[], config={}, toolsets=[])
C("_unnamed_extension_toolsets", {"get_plugin_toolset_keys": L(["p1", "coding"]),
    "enabled_mcp_server_names": L(["m1"]), "resolve_toolset": L(["p1"])},
  explicit=["m1"], requested=["p1"], config={"mcp_servers": {}}, toolsets=["coding", "core"])
C("_unnamed_extension_toolsets", {"get_plugin_toolset_keys": L(["p"]), "enabled_mcp_server_names": L([])},
  explicit=[], requested=[], config={}, toolsets=[])
C("_unnamed_extension_toolsets", {"get_plugin_toolset_keys": R("SystemExit", "x")},
  explicit=[], requested=[], config={}, toolsets=[])
C("_unnamed_extension_toolsets", {"get_plugin_toolset_keys": L(["p"]),
    "enabled_mcp_server_names": R("ValueError", "v")}, explicit=[], requested=[], config={}, toolsets=[])

# -- _resolve_toolsets --
C("_resolve_toolsets", {"get_platform_tools": L(["a", "b"])}, params={}, config={}, source="cli")
C("_resolve_toolsets", {"focus_toolsets": L(["f"])}, params={}, config={}, source="cli", coding_posture=True)
C("_resolve_toolsets", {"saved_toolset_resolver": None, "known": True,
    "get_platform_tools": L(["a"]), "resolve_toolset": L(["a"]), "get_plugin_toolset_keys": L([])},
  params={"toolsets": ["a"]}, config={}, source="cli", toolsets=[], surface_toolsets=[])
C("_resolve_toolsets", {"saved_toolset_resolver": None, "known": False},
  params={"toolsets": ["a"]}, config={}, source="cli")
C("_resolve_toolsets", {}, params={"toolsets": "a"}, config={}, source="cli")
C("_resolve_toolsets", {"saved_toolset_resolver": None, "known": True},
  params={"toolsets": ["desktop_ui"]}, config={}, source="cli")
C("_resolve_toolsets", {"get_platform_tools": L(["a", "p"]), "get_plugin_toolset_keys": L(["p"])},
  params={}, config={}, source="cli", safe_mode=True)
C("_resolve_toolsets", {"get_platform_tools": L(["a"]), "parse_config_string_list": L([])},
  params={}, config={"agent": {"disabled_toolsets": "x"}}, source="tui", surface_toolsets=["project", "desktop_ui"])
C("_resolve_toolsets", {"saved_toolset_resolver": None, "known": [R("SystemExit", "x")]},
  params={"toolsets": ["a"]}, config={}, source="cli")
C("_resolve_toolsets", {}, params={}, config={}, source="cli")  # fail-closed get_platform_tools

# -- _apply_launch_overrides --
C("_apply_launch_overrides", {"without_auto_pick": {}, "with_entry_pick": {}}, params={}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"provider": "p"}, "with_entry_pick": {"provider": "p"},
    "keeps_profile_route": False, "route_bound_keys": L(["api_key"])},
  params={"provider": "p"}, config={"model": {"api_key": "k", "default": "m"}})
C("_apply_launch_overrides", {"without_auto_pick": {"base_url": "https://h/p"},
    "with_entry_pick": {"base_url": "https://h/p"}},
  params={"base_url": "https://h/p"}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"base_url": "https://u@h/p"},
    "with_entry_pick": {"base_url": "https://u@h/p"}}, params={"base_url": "https://u@h/p"}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"yolo": 1}, "with_entry_pick": {"yolo": 1}},
  params={"yolo": 1}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"service_tier": "?"},
    "with_entry_pick": {"service_tier": "?"}, "parse_exact_service_tier": None},
  params={"service_tier": "?"}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"service_tier": "fast"},
    "with_entry_pick": {"service_tier": "fast"}, "parse_exact_service_tier": "fast"},
  params={"service_tier": "fast"}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"run_budget": "x"},
    "with_entry_pick": {"run_budget": "x"}}, params={"run_budget": "x"}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"run_budget": 12.5},
    "with_entry_pick": {"run_budget": 12.5}}, params={"run_budget": 12.5}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"checkpoints": True},
    "with_entry_pick": {"checkpoints": True}}, params={"checkpoints": True},
  config={"checkpoints": {"x": 1, "enabled": False}})
C("_apply_launch_overrides", {"without_auto_pick": {"max_turns": 10},
    "with_entry_pick": {"max_turns": 10}, "resolve_turn_limit": 10},
  params={"max_turns": 10}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"max_turns": "?"},
    "with_entry_pick": {"max_turns": "?"}, "resolve_turn_limit": 0},
  params={"max_turns": "?"}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"reasoning": "high"},
    "with_entry_pick": {"reasoning": "high"}, "parse_reasoning_effort": "high"},
  params={"reasoning": "high"}, config={"agent": {"reasoning_overrides": {"a": 1}}})
C("_apply_launch_overrides", {"without_auto_pick": {"service_tier": "?"},
    "with_entry_pick": {"service_tier": "?"}, "parse_exact_service_tier": R("ValueError", "v")},
  params={"service_tier": "?"}, config={})
C("_apply_launch_overrides", {"without_auto_pick": {"service_tier": "?"},
    "with_entry_pick": {"service_tier": "?"}, "parse_exact_service_tier": R("SystemExit", "s")},
  params={"service_tier": "?"}, config={})

# -- build_policy --
C("build_policy", {"build_forward_policy": {"source": "a2a", "platform": "cli", "cwd": "/x",
    "model": None, "toolsets": []}}, params={"source": "a2a"}, config={})
C("build_policy", {"build_forward_policy": R("SystemExit", "x")}, params={"source": "a2a"}, config={})
C("build_policy", {}, params={"bogus": 1}, config={})
C("build_policy", {}, params={"source": "xxx"}, config={})
C("build_policy", {}, params={"safe_mode": "x"}, config={})
C("build_policy", {}, params={"api_key": "  "}, config={})
C("build_policy", {}, params={"model": "  "}, config={})
C("build_policy", {}, params={"cwd": "rel"}, config={})
C("build_policy", {"os.path.isdir": False}, params={"cwd": "/x"}, config={})
C("build_policy", full_io(), params=BPARAMS, config=dict(BCONFIG), home="/h")
C("build_policy", full_io(validate_editor=R("SystemExit", "v")), params=BPARAMS, config=dict(BCONFIG), home="/h")
C("build_policy", full_io(client_problem="bad client"), params=dict(BPARAMS, client={"x": 1}), config=dict(BCONFIG), home="/h")
C("build_policy", full_io(resolve_agent_cwd="/x"),
  params={"source": "cli", "model": "m"}, config=dict(BCONFIG), home="/h")
C("build_policy", full_io(client_env_rules={"ALLOW": ["X"]}, normalise_client_env={"X": "y"}),
  params=dict(BPARAMS, client_env={"X": "y"}), config=dict(BCONFIG), home="/h")
C("build_policy", full_io(client_env_rules={"ALLOW": []}, normalise_client_env=R("ValueError", "bad")),
  params=dict(BPARAMS, client_env={"X": "y"}), config=dict(BCONFIG), home="/h")
C("build_policy", full_io(), params=dict(BPARAMS, follow_profile_config=True), config=dict(BCONFIG),
  home="/h")  # picked route but profile_target unscripted -> fail closed
C("build_policy", full_io(profile_target=L(["cli"])), params=dict(BPARAMS, follow_profile_config=True),
  config=dict(BCONFIG), home="/h")


def main():
    # dict.get's default is evaluated eagerly: build_policy always resolves the
    # agent cwd, even when params carries an explicit cwd. Script it everywhere.
    for case in CASES:
        if case["func"] == "build_policy":
            case["in"]["io"].setdefault("resolve_agent_cwd", "/x")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, separators=(",", ":"), ensure_ascii=False) + "\n")
    print("wrote %d cases to %s" % (len(CASES), OUT))


if __name__ == "__main__":
    main()
