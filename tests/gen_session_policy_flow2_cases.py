#!/usr/bin/env python3
"""Generate tests/fixtures/session_policy_flow2.jsonl -- differential cases for
the gateway/session_policy.py second-wave FLOW port
(src/gateway/session_policy_flow2.c).

Deterministic, >=5 cases per ported func: branch boundaries, scripted
sequences, fail-closed seams (missing/empty script -> RuntimeError), real
exception kinds (Exception swallowed where live swallows; BaseException passes
through), named AuthorityRefusal refusals, and the documented scalar/path/
strict-JSON domains. Mutable-state funcs (pin/unpin_reasoning,
bind/unbind_turn_workspace) carry in.config / in.overrides.

Run: python tests/gen_session_policy_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_policy_flow2.jsonl")

CASES = []


def C(func, io=None, **inp):
    CASES.append({"func": func, "in": dict(inp, io=io if io is not None else {})})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def AR(reason, field=None, detail=None):
    d = {"__raise__": "AuthorityRefusal", "msg": reason}
    if field is not None:
        d["field"] = field
    if detail is not None:
        d["detail"] = detail
    return d


# -- model_route --
C("model_route", {"normalize_route_base_url": "https://x/"},
  block={"provider": " OpenAI ", "base_url": "https://x"})
C("model_route", {"normalize_route_base_url": ""}, block={"provider": "auto", "base_url": None})
C("model_route", {"normalize_route_base_url": "ep"}, block=None)
C("model_route", {"normalize_route_base_url": "ep"}, block={"provider": 5})
C("model_route", {"normalize_route_base_url": "ep"}, block={"provider": None})
C("model_route", {}, block={"provider": "p"})  # fail-closed
C("model_route", {"normalize_route_base_url": R("ValueError", "bad url")}, block={"provider": "p"})
C("model_route", {"normalize_route_base_url": R("SystemExit", "bye")}, block={"provider": "p"})
C("model_route", {"normalize_route_base_url": ["a", "b"]}, block={"provider": "p"})

# -- _held_pick --
C("_held_pick", {"normalize_route_base_url": ["u1/", "u1/"]},
  frozen={"provider": "p", "base_url": "u1", "default": "m1", "api_key": "k"},
  live={"provider": "p", "base_url": "u1", "default": "m2", "api_key": "k2", "extra": "e"})
C("_held_pick", {"normalize_route_base_url": ["u1/", "u2/"]},
  frozen={"provider": "p", "base_url": "u1", "default": "m1"},
  live={"provider": "p", "base_url": "u2", "default": "m2"})
C("_held_pick", {"normalize_route_base_url": ["", ""]}, frozen={}, live={})
C("_held_pick", {}, frozen={}, live={})  # fail-closed
C("_held_pick", {"normalize_route_base_url": R("KeyboardInterrupt")}, frozen={}, live={})
C("_held_pick", {"normalize_route_base_url": ["", ""]}, frozen={}, live=None)  # live.items AttributeError

# -- entry_serves --
C("entry_serves", {"normalize_route_base_url": ""}, block={"base_url": None}, config={})
C("entry_serves", {"normalize_route_base_url": ["u/", "u/"], "_named_entry": {"base_url": "u"}},
  block={"base_url": "u", "provider": "p"}, config={})
C("entry_serves", {"normalize_route_base_url": ["u/"]}, block={"base_url": "u", "provider": ""}, config={})
C("entry_serves", {"normalize_route_base_url": ["u/", "u/", "u/"], "_named_entry": {"base_url": "u"}},
  block={"base_url": "u", "provider": "p", "api_key_from": ["providers", "x", "key"]},
  config={"providers": {"x": {"base_url": "u"}}})
C("entry_serves", {"normalize_route_base_url": ["u/", "u/"], "_named_entry": {"base_url": "u"}},
  block={"base_url": "u", "provider": "p", "api_key_from": ["providers", "x", "key"]},
  config={"providers": {"x": 5}})
C("entry_serves", {}, block={"base_url": "u"}, config={})  # fail-closed
C("entry_serves", {"normalize_route_base_url": ["u/"], "_named_entry": R("ValueError", "x")},
  block={"base_url": "u", "provider": "p"}, config={})
C("entry_serves", {"normalize_route_base_url": ["u/"], "_named_entry": {"base_url": ""}},
  block={"base_url": "u", "provider": "p"}, config={})
C("entry_serves", {"normalize_route_base_url": ["u/", R("SystemExit", "bye")],
                   "_named_entry": {"base_url": "u"}},
  block={"base_url": "u", "provider": "p"}, config={})

# -- bound_route_holds --
C("bound_route_holds", {"resolved_route": [["id1", "u1"], ["id2", "u2"]], "same_route": True},
  bound={"provider": "p"}, bound_config={}, block={"provider": "q"}, config={})
C("bound_route_holds", {"resolved_route": [["auto", ""], ["id2", ""]],
                        "normalize_route_base_url": ["e", "e"]},
  bound={"base_url": "u"}, bound_config={}, block={"base_url": "u"}, config={})
C("bound_route_holds", {"resolved_route": [["auto", ""], ["id2", ""]],
                        "normalize_route_base_url": ["e1", "e2"]},
  bound={"base_url": "u"}, bound_config={}, block={"base_url": "v"}, config={})
C("bound_route_holds", {"resolved_route": [None]},
  bound={}, bound_config={}, block={}, config={})
C("bound_route_holds", {"resolved_route": [["only"]]},
  bound={}, bound_config={}, block={}, config={})
C("bound_route_holds", {"resolved_route": [["id", "u"], None]},
  bound={}, bound_config={}, block={}, config={})
C("bound_route_holds", {}, bound={}, bound_config={}, block={}, config={})  # fail-closed
C("bound_route_holds", {"resolved_route": [["id", "u"], ["id2", "u2"]],
                        "same_route": R("ValueError", "x")},
  bound={}, bound_config={}, block={}, config={})
C("bound_route_holds", {"resolved_route": [["id", "u"], ["id2", "u2"]], "same_route": "yes"},
  bound={}, bound_config={}, block={}, config={})
C("bound_route_holds", {"resolved_route": [["id", ""], ["id2", ""]], "same_route": True},
  bound={"provider": "p"}, bound_config={}, block={}, config={})

# -- _without_auto_pick --
C("_without_auto_pick", {}, params={"provider": "auto", "model": "m"})
C("_without_auto_pick", {}, params={"provider": " AUTO ", "x": 1})
C("_without_auto_pick", {}, params={"provider": "openai"})
C("_without_auto_pick", {}, params={"provider": None})
C("_without_auto_pick", {}, params={})
C("_without_auto_pick", {}, params={"provider": "auto"})
C("_without_auto_pick", {}, params=None)

# -- _keeps_profile_route --
C("_keeps_profile_route", {"profile_route_identity": "id", "resolved_route": [["id", "u"]]},
  picked="p", config={})
C("_keeps_profile_route", {"profile_route_identity": "auto"}, picked="p", config={})
C("_keeps_profile_route", {"profile_route_identity": "id"}, picked="", config={})
C("_keeps_profile_route", {"profile_route_identity": "id"}, picked=None, config={})
C("_keeps_profile_route", {"profile_route_identity": "id", "resolved_route": [["other", "u"]]},
  picked="p", config={})
C("_keeps_profile_route", {}, picked="p", config={})  # fail-closed
C("_keeps_profile_route", {"profile_route_identity": "id", "resolved_route": [None]},
  picked="p", config={})
C("_keeps_profile_route", {"profile_route_identity": "id",
                           "resolved_route": R("GeneratorExit")}, picked="p", config={})

# -- _with_entry_pick --
C("_with_entry_pick", {}, params={"provider": 5}, config={})
C("_with_entry_pick", {}, params={}, config={})
C("_with_entry_pick", {"profile_route_identity": "auto"}, params={"provider": "p"}, config={})
C("_with_entry_pick", {"profile_route_identity": "custom:deepseek",
                       "configured_entry_key": "deepseek"},
  params={"provider": "deepseek"}, config={})
C("_with_entry_pick", {"profile_route_identity": "custom:deepseek",
                       "configured_entry_key": "other"},
  params={"provider": "deepseek"}, config={})
C("_with_entry_pick", {"profile_route_identity": "custom:deepseek",
                       "configured_entry_key": None},
  params={"provider": "deepseek"}, config={})
C("_with_entry_pick", {"profile_route_identity": "custom", "normalize_route_base_url": ["cu/"],
                       "_route_key": ["cukey", "cukey"]},
  params={"provider": "custom", "base_url": "u", "model": "m"},
  config={"model": {"base_url": "cu"}})
C("_with_entry_pick", {"profile_route_identity": "custom", "normalize_route_base_url": ["cu/"],
                       "_route_key": ["cukey"], "profile_target": [["M2", "p"]],
                       "routed_custom_identity": [None]},
  params={"provider": "custom", "model": "M"}, config={"model": {"base_url": "cu"}})
C("_with_entry_pick", {"profile_route_identity": "custom:x", "normalize_route_base_url": ["cu/"],
                       "_route_key": [""], "routed_custom_identity": ["custom:y"]},
  params={"provider": "custom", "model": "m"}, config={"model": {"base_url": "cu"}})
C("_with_entry_pick", {"profile_route_identity": "custom:x", "normalize_route_base_url": ["cu/"],
                       "_route_key": [""], "routed_custom_identity": ["custom:y"]},
  params={"provider": "custom", "model": "m", "api_key": "k"},
  config={"model": {"base_url": "cu"}})
C("_with_entry_pick", {"profile_route_identity": "custom:x", "normalize_route_base_url": ["cu/"],
                       "_route_key": [""], "routed_custom_identity": [None]},
  params={"provider": "custom", "model": "m"}, config={"model": {"base_url": "cu"}})
C("_with_entry_pick", {}, params={"provider": "custom"}, config={})  # fail-closed
C("_with_entry_pick", {"profile_route_identity": "custom:x", "normalize_route_base_url": ["cu/"],
                       "_route_key": [""], "routed_custom_identity": R("SystemExit", "bye")},
  params={"provider": "custom", "model": "m"}, config={"model": {"base_url": "cu"}})
C("_with_entry_pick", {"profile_route_identity": "custom", "normalize_route_base_url": ["cu/"],
                       "_route_key": ["cukey", "other"], "routed_custom_identity": [None]},
  params={"provider": "custom", "base_url": "u", "model": "m"},
  config={"model": {"base_url": "cu"}})

# -- pin_reasoning_effort --
C("pin_reasoning_effort", {}, config={}, effort="high", session=False)
C("pin_reasoning_effort", {}, config={"agent": {"reasoning_effort": "low", "reasoning_overrides": {"x": 1}}},
  effort="high")
C("pin_reasoning_effort", {}, config={"agent": {"reasoning_effort": "low"}}, effort="high", session=True)
C("pin_reasoning_effort", {}, config={"session_pins": {"reasoning": False, "other": 1}},
  effort="x", session=True)
C("pin_reasoning_effort", {}, config={"agent": None}, effort="x")
C("pin_reasoning_effort", {}, config={"session_pins": "x"}, effort="e", session=True)
C("pin_reasoning_effort", {}, config={"session_pins": {"reasoning": True}}, effort="e", session=True)

# -- unpin_reasoning --
C("unpin_reasoning", {"_follow": [None, None]},
  config={"agent": {"reasoning_effort": "high", "reasoning_overrides": {}}},
  profile={"agent": {"reasoning_effort": "low"}})
C("unpin_reasoning", {"_follow": [None, None]}, config={}, profile={})
C("unpin_reasoning", {"_follow": [None, None]},
  config={"agent": {"reasoning_effort": "high"}},
  profile={"agent": {"reasoning_overrides": {"x": 1}}})
C("unpin_reasoning", {"_follow": [None, None]},
  config={"session_pins": {"reasoning": True}}, profile={"agent": {"reasoning_effort": "med"}})
C("unpin_reasoning", {}, config={}, profile={})  # fail-closed _follow
C("unpin_reasoning", {"_follow": R("ValueError", "x")}, config={}, profile={})
C("unpin_reasoning", {"_follow": R("SystemExit", "bye")}, config={}, profile={})
C("unpin_reasoning", {"_follow": [None, None]},
  config={"agent": {"reasoning_effort": "h"}}, profile={"agent": None})

# -- profile_config --
C("profile_config", {"authority_persistence": {"profile_dir": "/h/p1"},
                     "_load_gateway_config": {"model": {"default": "m"}},
                     "present_sections": {"model": {"default": "m"}}}, authority={"id": "a"})
C("profile_config", {"authority_persistence": {"profile_dir": ""},
                     "_load_gateway_config": {}, "present_sections": {}}, authority={})
C("profile_config", {"authority_persistence": {"profile_dir": "/"},
                     "_load_gateway_config": {}, "present_sections": {}}, authority={})
C("profile_config", {"authority_persistence": {"profile_dir": "//"},
                     "_load_gateway_config": {}, "present_sections": {}}, authority={})
C("profile_config", {"authority_persistence": None}, authority={})
C("profile_config", {}, authority={})  # fail-closed
C("profile_config", {"authority_persistence": {"profile_dir": "/h"},
                     "_load_gateway_config": R("ValueError", "x")}, authority={})
C("profile_config", {"authority_persistence": {"profile_dir": "/h"},
                     "_load_gateway_config": {}, "present_sections": R("KeyboardInterrupt")}, authority={})
C("profile_config", {"authority_persistence": {"profile_dir": "/h/p1"},
                     "_load_gateway_config": {"gateway": None},
                     "present_sections": {"gateway": None}}, authority={})

# -- _key_slot --
C("_key_slot", {}, credential_ref="i:e:s#fp")
C("_key_slot", {}, credential_ref="i:e:s")
C("_key_slot", {}, credential_ref="#fp")
C("_key_slot", {}, credential_ref="a#b#c")
C("_key_slot", {}, credential_ref="")
C("_key_slot", {}, credential_ref="x#")
C("_key_slot", {}, credential_ref=None)

# -- launch_key --
C("launch_key", {}, policy={"credential_ref": None}, authority={})
C("launch_key", {}, policy={"credential_ref": "i:e:s#fp"},
  authority={"_local_launch_keys": {"i:e:s": "secret"}})
C("launch_key", {}, policy={"credential_ref": "i:e:s#fp"}, authority={"_local_launch_keys": {}})
C("launch_key", {}, policy={"credential_ref": "i:e:s"}, authority={})
C("launch_key", {}, policy={"credential_ref": "i:e:s#fp"},
  authority={"_local_launch_keys": {"i:e:s": None}})
C("launch_key", {}, policy={"credential_ref": "i:e:s#fp"},
  authority={"_local_launch_keys": {"i:e:s": ""}})
C("launch_key", {}, policy={"credential_ref": 5}, authority={})

# -- route_launch_key --
C("route_launch_key", {"present_sections": {"model": {"provider": "p", "base_url": "u"}},
                       "resolved_route": [["id", "u"], ["id", "u"]], "same_route": True},
  policy={"config_json": '{"model": {"provider": "p", "base_url": "u"}}', "credential_ref": "i:e:s#fp"},
  config={"model": {"provider": "p", "base_url": "u"}},
  authority={"_local_launch_keys": {"i:e:s": "k"}})
C("route_launch_key", {"present_sections": {"model": {"provider": "p", "base_url": "u"}},
                       "resolved_route": [["id", "u"], ["id", "u"]], "same_route": False},
  policy={"config_json": '{"model": {"provider": "p", "base_url": "u"}}', "credential_ref": "i:e:s#fp"},
  config={"model": {"provider": "p", "base_url": "u"}}, authority={"_local_launch_keys": {"i:e:s": "k"}})
C("route_launch_key", {}, policy={"config_json": None, "credential_ref": "i:e:s#fp"},
  config={}, authority={})
C("route_launch_key", {"present_sections": [[1]]},
  policy={"config_json": "{}", "credential_ref": None}, config={}, authority={})
C("route_launch_key", {}, policy={"config_json": "{}", "credential_ref": None}, config={}, authority={})
C("route_launch_key", {"present_sections": {"model": {"provider": "p", "base_url": "u"}},
                       "resolved_route": [["auto", ""], ["id", ""]],
                       "normalize_route_base_url": ["u/", "u/"]},
  policy={"config_json": '{"model": {"provider": "p", "base_url": "u"}}', "credential_ref": "i:e:s#fp"},
  config={"model": {"provider": "p", "base_url": "u"}},
  authority={"_local_launch_keys": {"i:e:s": "k"}})
C("route_launch_key", {"present_sections": {"model": {}},
                       "resolved_route": R("ValueError", "x")},
  policy={"config_json": "{}", "credential_ref": None}, config={}, authority={})

# -- workspace_ready --
C("workspace_ready", {"Path.is_absolute": True, "Path.is_dir": True}, policy={"cwd": "/x"})
C("workspace_ready", {"Path.is_absolute": False}, policy={"cwd": "/x"})
C("workspace_ready", {"Path.is_absolute": True, "Path.is_dir": False}, policy={"cwd": "/x"})
C("workspace_ready", {"Path.is_absolute": True, "Path.is_dir": "yes"}, policy={"cwd": "/x"})
C("workspace_ready", {"Path.is_absolute": 0}, policy={"cwd": "/x"})
C("workspace_ready", {}, policy={"cwd": "/x"})  # fail-closed
C("workspace_ready", {"Path.is_absolute": True, "Path.is_dir": R("OSError", "x")}, policy={"cwd": "/x"})
C("workspace_ready", {}, policy={"cwd": None})

# -- bind_turn_workspace --
C("bind_turn_workspace", {}, policy=None, task_id="t", overrides={})
C("bind_turn_workspace", {}, policy={"terminal_json": "{}", "cwd": "/x"}, task_id="", overrides={})
C("bind_turn_workspace", {"_qualify_task_key": "key"},
  policy={"terminal_json": '{"TERMINAL_ENV": "ssh"}', "cwd": "/x"}, task_id="t", overrides={})
C("bind_turn_workspace", {"terminal_uses_host_cwd": False, "_qualify_task_key": "key"},
  policy={"terminal_json": '{"TERMINAL_ENV": "docker"}', "cwd": "/x"}, task_id="t", overrides={})
C("bind_turn_workspace", {"terminal_uses_host_cwd": True,
                          "resolve_task_overrides": {"cwd": "/x", "cwd_source": "session"}},
  policy={"terminal_json": '{"TERMINAL_ENV": "docker"}', "cwd": "/x"}, task_id="t", overrides={})
C("bind_turn_workspace", {"terminal_uses_host_cwd": True,
                          "resolve_task_overrides": {"cwd": "/other"},
                          "workspace_key_for": "wk", "register_task_env_overrides": None},
  policy={"terminal_json": '{"TERMINAL_ENV": "docker"}', "cwd": "/x"}, task_id="t", overrides={})
C("bind_turn_workspace", {"terminal_uses_host_cwd": True, "resolve_task_overrides": {},
                          "workspace_key_for": "wk", "register_task_env_overrides": None},
  policy={"terminal_json": '{"TERMINAL_ENV": "docker"}', "cwd": "/x"}, task_id="t", overrides={})
C("bind_turn_workspace", {"terminal_uses_host_cwd": True},
  policy={"terminal_json": '{"TERMINAL_ENV": "docker"}', "cwd": "/x"}, task_id="t", overrides={})
C("bind_turn_workspace", {"_qualify_task_key": "key", "get_session_cwd": "/x",
                          "clear_session_cwd": None},
  policy={"terminal_json": '{"TERMINAL_ENV": "ssh"}', "cwd": "/x"}, task_id="t",
  overrides={"key": {"workspace_key": "wk", "cwd": "/x"}})
C("bind_turn_workspace", {"terminal_uses_host_cwd": True, "resolve_task_overrides": {},
                          "workspace_key_for": "wk",
                          "register_task_env_overrides": R("ValueError", "x")},
  policy={"terminal_json": '{"TERMINAL_ENV": "docker"}', "cwd": "/x"}, task_id="t", overrides={})

# -- unbind_turn_workspace --
C("unbind_turn_workspace", {}, task_id="", overrides={})
C("unbind_turn_workspace", {"_qualify_task_key": "key"}, task_id="t", overrides={})
C("unbind_turn_workspace", {"_qualify_task_key": "key"}, task_id="t",
  overrides={"key": {"cwd": "/x"}})
C("unbind_turn_workspace", {"_qualify_task_key": "key", "get_session_cwd": "/x",
                            "clear_session_cwd": None}, task_id="t",
  overrides={"key": {"workspace_key": "wk", "cwd": "/x"}})
C("unbind_turn_workspace", {"_qualify_task_key": "key", "get_session_cwd": "/other"}, task_id="t",
  overrides={"key": {"workspace_key": "wk", "cwd": "/x"}})
C("unbind_turn_workspace", {"_qualify_task_key": "key", "get_session_cwd": None,
                            "clear_session_cwd": None}, task_id="t",
  overrides={"key": {"workspace_key": "wk", "cwd": None}})
C("unbind_turn_workspace", {}, task_id="t", overrides={})  # fail-closed
C("unbind_turn_workspace", {"_qualify_task_key": "key", "get_session_cwd": R("SystemExit", "bye")},
  task_id="t", overrides={"key": {"workspace_key": "wk", "cwd": "/x"}})
C("unbind_turn_workspace", {"_qualify_task_key": "key"}, task_id="t", overrides={"key": None})

# -- accept_launch_hooks --
C("accept_launch_hooks", {"_load_gateway_config": {"hooks": []}, "accept_for_launch": None},
  policy={"request_json": '{"accept_hooks": true}', "ignore_user_config": False})
C("accept_launch_hooks", {}, policy={"request_json": '{"accept_hooks": false}', "ignore_user_config": False})
C("accept_launch_hooks", {}, policy={"request_json": '{"accept_hooks": true}', "ignore_user_config": True})
C("accept_launch_hooks", {}, policy={"request_json": '{"accept_hooks": true}', "ignore_user_config": True})
C("accept_launch_hooks", {}, policy={"request_json": "[1]", "ignore_user_config": False})
C("accept_launch_hooks", {}, policy={"request_json": "null", "ignore_user_config": False})
C("accept_launch_hooks", {}, policy={"request_json": '{"accept_hooks": true}', "ignore_user_config": False})
C("accept_launch_hooks", {"_load_gateway_config": {}, "accept_for_launch": R("ValueError", "x")},
  policy={"request_json": '{"accept_hooks": true}', "ignore_user_config": False})
C("accept_launch_hooks", {"_load_gateway_config": {}, "accept_for_launch": R("GeneratorExit")},
  policy={"request_json": '{"accept_hooks": true}', "ignore_user_config": False})

# -- refresh_profile_hooks --
C("refresh_profile_hooks", {}, policy=None)
C("refresh_profile_hooks", {}, policy={"ignore_user_config": True})
C("refresh_profile_hooks", {"sync_from_config": None}, policy={"ignore_user_config": False})
C("refresh_profile_hooks", {"sync_from_config": R("RuntimeError", "boom"), "logger.warning": None},
  policy={"ignore_user_config": False})
C("refresh_profile_hooks", {"sync_from_config": R("SystemExit", "bye")},
  policy={"ignore_user_config": False})
C("refresh_profile_hooks", {"sync_from_config": R("ValueError", "x"), "logger.warning": None},
  policy={"ignore_user_config": False})
C("refresh_profile_hooks", {"logger.warning": None}, policy={"ignore_user_config": False})
C("refresh_profile_hooks", {"sync_from_config": R("RuntimeError", "boom"),
                            "logger.warning": R("ValueError", "log")},
  policy={"ignore_user_config": False})

# -- register_worker_hooks --
C("register_worker_hooks", {}, policy={"ignore_user_config": True})
C("register_worker_hooks", {"policy.config": {"model": {}},
                            "shell_hooks.register_from_config": None,
                            "outbound_webhooks.register_from_config": None},
  policy={"ignore_user_config": False, "kanban_json": None, "request_json": '{"accept_hooks": true}'})
C("register_worker_hooks", {"policy.config": {}, "shell_hooks.register_from_config": None,
                            "outbound_webhooks.register_from_config": None},
  policy={"ignore_user_config": False, "kanban_json": '{"accept_hooks": true}', "request_json": "{}"})
C("register_worker_hooks", {"policy.config": {}, "shell_hooks.register_from_config": None,
                            "outbound_webhooks.register_from_config": None},
  policy={"ignore_user_config": False, "kanban_json": None, "request_json": '{"accept_hooks": 1}'})
C("register_worker_hooks", {"policy.config": {}, "shell_hooks.register_from_config": None,
                            "outbound_webhooks.register_from_config": None},
  policy={"ignore_user_config": False, "kanban_json": None, "request_json": "{}"})
C("register_worker_hooks", {}, policy={"ignore_user_config": False, "kanban_json": "[1]",
                                       "request_json": "{}"})
C("register_worker_hooks", {}, policy={"ignore_user_config": False, "kanban_json": None,
                                       "request_json": "{}"})  # fail-closed policy.config
C("register_worker_hooks", {"policy.config": {},
                            "shell_hooks.register_from_config": R("ValueError", "x")},
  policy={"ignore_user_config": False, "kanban_json": None, "request_json": "{}"})
C("register_worker_hooks", {"policy.config": {}, "shell_hooks.register_from_config": None,
                            "outbound_webhooks.register_from_config": R("KeyboardInterrupt")},
  policy={"ignore_user_config": False, "kanban_json": None, "request_json": "{}"})

# -- _in_coding_workspace --
C("_in_coding_workspace", {"in_focus_workspace": True}, config={}, source="gui", cwd="/x")
C("_in_coding_workspace", {"in_focus_workspace": False}, config={}, source="cli", cwd="/x")
C("_in_coding_workspace", {"logger.debug": None}, config={}, source="unknown", cwd="/x")
C("_in_coding_workspace", {"in_focus_workspace": R("ValueError", "x"), "logger.debug": None},
  config={}, source="gui", cwd="/x")
C("_in_coding_workspace", {"in_focus_workspace": R("SystemExit", "bye")},
  config={}, source="gui", cwd="/x")
C("_in_coding_workspace", {"in_focus_workspace": AR("invalid_params"), "logger.debug": None},
  config={}, source="gui", cwd="/x")
C("_in_coding_workspace", {"logger.debug": None}, config={}, source="gui", cwd="/x")
C("_in_coding_workspace", {"in_focus_workspace": R("RuntimeError", "x"),
                           "logger.debug": R("ValueError", "log")}, config={}, source="gui", cwd="/x")
C("_in_coding_workspace", {"in_focus_workspace": "maybe"}, config={}, source="gui", cwd="/x")
C("_in_coding_workspace", {"in_focus_workspace": True}, config={}, source="tool", cwd="/x")



# ═══════════════════════════════════════════════════════════════════════════
# Third-wave remainder funcs (7-8 cases each).
# ═══════════════════════════════════════════════════════════════════════════

# -- session_pinned --
C("session_pinned", {}, config={"session_pins": {"reasoning": True}}, field="reasoning", launched=False)
C("session_pinned", {}, config={"session_pins": {"reasoning": False}}, field="reasoning", launched=True)
C("session_pinned", {}, config={"session_pins": {}}, field="reasoning", launched=True)
C("session_pinned", {}, config={"session_pins": {"other": True}}, field="reasoning", launched=False)
C("session_pinned", {}, config={"session_pins": "notadict"}, field="reasoning", launched=True)
C("session_pinned", {}, config={}, field="reasoning", launched=False)
C("session_pinned", {}, config={"session_pins": {"reasoning": 1}}, field="reasoning", launched=True)
C("session_pinned", {}, config={"session_pins": {"reasoning": True}}, field="toolsets", launched="x")

# -- created_surface --
C("created_surface", {}, label=None)
C("created_surface", {}, label="")
C("created_surface", {}, label="local")
C("created_surface", {}, label="unknown")
C("created_surface", {}, label="cli")
C("created_surface", {}, label="tui")
C("created_surface", {}, label="gui")
C("created_surface", {}, label="acp")
C("created_surface", {}, label="tool")
C("created_surface", {}, label="oneshot")
C("created_surface", {}, label="custom")
C("created_surface", {}, label=5)

# -- profile_target --
C("profile_target", {}, config={"model": {"provider": " OpenAI ", "default": " m1 "}})
C("profile_target", {}, config={"model": {"provider": "AUTO", "default": "m"}})
C("profile_target", {}, config={"model": {"provider": "custom"}})
C("profile_target", {}, config={"model": "  "})
C("profile_target", {}, config={"model": " m1 "})
C("profile_target", {}, config={})
C("profile_target", {}, config={"model": None})
C("profile_target", {}, config={"model": 5})

# -- present_sections --
C("present_sections", {}, config={"gateway": None, "model": {"default": "m"}, "a": 1})
C("present_sections", {}, config={"model": "  "})
C("present_sections", {}, config={"model": " m1 "})
C("present_sections", {}, config={"model": 5})
C("present_sections", {}, config={"x": None, "y": False})
C("present_sections", {}, config={})
C("present_sections", {}, config={"model": None})

# -- terminal_uses_host_cwd --
C("terminal_uses_host_cwd", {}, terminal={"TERMINAL_ENV": " local "})
C("terminal_uses_host_cwd", {}, terminal={"TERMINAL_ENV": "LOCAL"})
C("terminal_uses_host_cwd", {}, terminal={})
C("terminal_uses_host_cwd", {}, terminal={"TERMINAL_ENV": "docker", "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE": "YES"})
C("terminal_uses_host_cwd", {}, terminal={"TERMINAL_ENV": "docker", "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE": "0"})
C("terminal_uses_host_cwd", {}, terminal={"TERMINAL_ENV": "ssh"})
C("terminal_uses_host_cwd", {}, terminal={"TERMINAL_ENV": "docker", "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE": "1"})
C("terminal_uses_host_cwd", {}, terminal={"TERMINAL_ENV": None})

# -- _valid_client_env --
C("_valid_client_env", {}, value=None)
C("_valid_client_env", {}, value='{"A":"b"}')
C("_valid_client_env", {}, value='{"A":1}')
C("_valid_client_env", {}, value='{"A":["x","y"]}', kind="list")
C("_valid_client_env", {}, value='{"A":"x"}', kind="list")
C("_valid_client_env", {}, value='{oops')
C("_valid_client_env", {}, value='[]')
C("_valid_client_env", {}, value=123)

# -- client_env --
C("client_env", {}, client_env_json='{"A":"b"}')
C("client_env", {}, client_env_json=None)
C("client_env", {}, client_env_json='[1,2]')
C("client_env", {}, client_env_json='"hi"')
C("client_env", {}, client_env_json='null')
C("client_env", {}, client_env_json='{}')
C("client_env", {}, client_env_json='{"x":null,"y":true}')

# -- ignore_rules --
C("ignore_rules", {}, request_json='{"ignore_rules":true}', safe_mode=False)
C("ignore_rules", {}, request_json='{}', safe_mode=False)
C("ignore_rules", {}, request_json='{}', safe_mode=True)
C("ignore_rules", {}, request_json='{"ignore_rules":false}', safe_mode=True)
C("ignore_rules", {}, request_json='{"ignore_rules":"yes"}', safe_mode=False)
C("ignore_rules", {}, request_json='{"ignore_rules":1}', safe_mode=False)
C("ignore_rules", {}, request_json='{"other":true}', safe_mode=False)

# -- yolo / pass_session_id / verbose --
for _fn, _key in (("yolo", "yolo"), ("pass_session_id", "pass_session_id"), ("verbose", "verbose")):
    C(_fn, {}, request_json='{"%s":true}' % _key)
    C(_fn, {}, request_json='{}')
    C(_fn, {}, request_json='{"%s":false}' % _key)
    C(_fn, {}, request_json='{"%s":"on"}' % _key)
    C(_fn, {}, request_json='{"other":1}')
    C(_fn, {}, request_json='{"%s":0}' % _key)

# -- follows_profile --
C("follows_profile", {}, request_json='{"follow_profile_config":true}', config_json='{}')
C("follows_profile", {}, request_json='{"follow_profile_config":false}', config_json='{"follow_adopted":true}')
C("follows_profile", {}, request_json='{}', config_json='{"follow_adopted":true}')
C("follows_profile", {}, request_json='{}', config_json='{}')
C("follows_profile", {}, request_json='{"follow_profile_config":1}', config_json='{"follow_adopted":1}')
C("follows_profile", {}, request_json='{"follow_profile_config":true}', config_json='{"follow_adopted":false}')

# -- checkpoints_enabled / provider / base_url (policy.config seam) --
C("checkpoints_enabled", {"policy.config": {"checkpoints": {"enabled": True}}}, policy={})
C("checkpoints_enabled", {"policy.config": {"checkpoints": {"enabled": False}}}, policy={})
C("checkpoints_enabled", {"policy.config": {}}, policy={})
C("checkpoints_enabled", {"policy.config": {"checkpoints": None}}, policy={})
C("checkpoints_enabled", {"policy.config": {"checkpoints": True}}, policy={})
C("checkpoints_enabled", {"policy.config": {"checkpoints": {"enabled": 1}}}, policy={})
C("checkpoints_enabled", {}, policy={})  # fail-closed
C("checkpoints_enabled", {"policy.config": R("SystemExit", "bye")}, policy={})

for _fn, _field in (("provider", "provider"), ("base_url", "base_url")):
    C(_fn, {"policy.config": {"model": {_field: "v"}}}, policy={})
    C(_fn, {"policy.config": {}}, policy={})
    C(_fn, {"policy.config": {"model": {}}}, policy={})
    C(_fn, {"policy.config": {"model": {_field: None}}}, policy={})
    C(_fn, {"policy.config": {"model": {"other": "z"}}}, policy={})
    C(_fn, {}, policy={})  # fail-closed
    C(_fn, {"policy.config": R("SystemExit", "bye")}, policy={})

def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func count audit (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases to %s" % (len(CASES), OUT))
    print("funcs: %d, min cases/func: %d" % (len(counts), min(counts.values())))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)


if __name__ == "__main__":
    main()
