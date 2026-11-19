#!/usr/bin/env python3
"""Generate tests/fixtures/config_flow2.jsonl for the config.py UNCLAIMED
remainder FLOW oracle. Each line is {"func": <name>, "in": {...}}; "in.io"
carries the scripted seam results ({"ret","raise","import_raise","state"} or a
queue LIST) consumed identically by the C harness dispatcher and the oracle's
recording stubs. List-valued seam results are wrapped in {"ret": [...]} so a
queue is never confused with a single list result. Pure data — no quarry import.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_flow2.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def X(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"raise": d}


def IMP(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"import_raise": d}


# ── format_docker_update_message ─────────────────────────────────────────
for m in ["docker update message A", "msg B", "", "line1\nline2", "pull nousresearch/hermes-agent:latest"]:
    case("format_docker_update_message", state={"_DOCKER_UPDATE_MESSAGE": m})

# ── __getattr__ (module_getattr) ─────────────────────────────────────────
case("module_getattr", name="CORE_DECLARED_ENV_NAMES", io={"core_declared_env_names": R(["OPENAI_API_KEY", "ANTHROPIC_API_KEY"])})
case("module_getattr", name="CORE_DECLARED_ENV_NAMES", io={"core_declared_env_names": R([])})
case("module_getattr", name="CORE_DECLARED_ENV_NAMES", io={"core_declared_env_names": X("RuntimeError", "boom")})
case("module_getattr", name="OTHER_ATTR")
case("module_getattr", name="foo")
case("module_getattr", name="CORE_DECLARED_ENV_NAMES", io={})

# ── _managed_source ──────────────────────────────────────────────────────
case("managed_source", filename=".env", io={"get_managed_dir": R("/managed")})
case("managed_source", filename="config.yaml", io={"get_managed_dir": R("/m")})
case("managed_source", filename=".env", io={"get_managed_dir": R(None)})
case("managed_source", filename=".env", io={"get_managed_dir": R("")})
case("managed_source", filename=".env", io={"get_managed_dir": X("OSError", "denied")})
case("managed_source", filename=".env", io={})

# ── env_write_refusal ────────────────────────────────────────────────────
case("env_write_refusal", key="K", action="set", io={"is_managed": R(True), "format_managed_message": R("managed msg")})
case("env_write_refusal", key="K", action="set", io={"is_managed": R(False), "is_env_managed": R(True), "get_managed_dir": R("/m")})
case("env_write_refusal", key="K", action="remove", io={"is_managed": R(False), "is_env_managed": R(True), "get_managed_dir": R(None)})
case("env_write_refusal", key="K", action="set", io={"is_managed": R(False), "is_env_managed": R(False)})
case("env_write_refusal", key="K", action="set", io={"is_managed": X("RuntimeError", "x")})
case("env_write_refusal", key="K", action="set", io={})

# ── _env_write_blocked ───────────────────────────────────────────────────
case("env_write_blocked", key="K", action="set", io={"is_managed": R(False), "is_env_managed": R(False)})
case("env_write_blocked", key="K", action="set", io={"is_managed": R(True), "format_managed_message": R("refusal text")})
case("env_write_blocked", key="K", action="set", io={"is_managed": R(True), "format_managed_message": R("")})
case("env_write_blocked", key="K", action="remove", io={"is_managed": R(False), "is_env_managed": R(True), "get_managed_dir": R("/m")})
case("env_write_blocked", key="K", action="set", io={"is_managed": X("RuntimeError", "x")})

# ── require_env_writable ─────────────────────────────────────────────────
case("require_env_writable", key="K", action="set", io={"is_managed": R(False), "is_env_managed": R(False)})
case("require_env_writable", key="K", action="set", io={"is_managed": R(True), "format_managed_message": R("refusal text")})
case("require_env_writable", key="K", action="set", io={"is_managed": R(True), "format_managed_message": R("")})
case("require_env_writable", key="K", action="set", io={"is_managed": R(False), "is_env_managed": R(True), "get_managed_dir": R(None)})
case("require_env_writable", key="K", action="set", io={"is_managed": X("ValueError", "x")})

# ── _write_anthropic_slots ───────────────────────────────────────────────
case("write_anthropic_slots", token="tok", api_key="key", io={"save_env_value": [R(None), R(None)]})
case("write_anthropic_slots", token="tok", api_key="key", token_first=False, io={"save_env_value": [R(None), R(None)]})
case("write_anthropic_slots", token="tok", api_key="key", save_fn={"fn": 1}, io={"save_fn": [R(None), R(None)]})
case("write_anthropic_slots", token="tok", api_key="key", io={"save_env_value": [X("ValueError", "bad")]})
case("write_anthropic_slots", token="tok", api_key="key", io={"save_env_value": [R(None), X("RuntimeError", "x")]})
case("write_anthropic_slots", token="tok", api_key="key", io={})

# ── _scoped_environ_get ──────────────────────────────────────────────────
case("scoped_environ_get", key="K", io={"get_secret": R("val")})
case("scoped_environ_get", key="K", io={"get_secret": R(None)})
case("scoped_environ_get", key="K", io={"get_secret": IMP("ImportError", "no module"), "os.environ.get": R("envval")})
case("scoped_environ_get", key="K", io={"get_secret": X("RuntimeError", "scope fail")})
case("scoped_environ_get", key="K", io={"get_secret": IMP("KeyboardInterrupt", "")})
case("scoped_environ_get", key="K", io={})

# ── _legacy_gateway_platforms_key ────────────────────────────────────────
for k in ["gateway.platforms.discord.token", "gateway.platforms", "gateway.platforms.x",
          "model.default", "gateway.other.x", "", "gateway.platforms.a\\.b"]:
    case("legacy_gateway_platforms_key", requested_key=k)

# ── _exit_if_key_managed ─────────────────────────────────────────────────
case("exit_if_key_managed", key="model", action="set", io={"is_key_managed": R(True), "get_managed_dir": R("/m")})
case("exit_if_key_managed", key="model", action="set", io={"is_key_managed": R(True), "get_managed_dir": R(None)})
case("exit_if_key_managed", key="model", action="set", io={"is_key_managed": R(False), "get_config_backend": R(None), "get_config_path": R("/h/c.yaml"), "backend_locked": R(None)})
case("exit_if_key_managed", key="model", action="unset", io={"is_key_managed": R(False), "get_config_backend": R(None), "get_config_path": R("/h/c.yaml"), "backend_locked": R("org")})
case("exit_if_key_managed", key="model", action="set", io={"is_key_managed": X("RuntimeError", "x")})
case("exit_if_key_managed", key="model", action="set", io={"is_key_managed": R(False), "get_config_backend": R(None), "get_config_path": X("OSError", "x")})
case("exit_if_key_managed", key="model", action="set", io={})

# ── _touch_skin_file ─────────────────────────────────────────────────────
case("touch_skin_file", key="display.skin", value="dark", io={"get_hermes_home": R("/h"), "path_exists": R(True), "touch": R(None)})
case("touch_skin_file", key="display.skin", value="dark", io={"get_hermes_home": R("/h"), "path_exists": R(False)})
case("touch_skin_file", key="other", value="dark", io={})
case("touch_skin_file", key="display.skin", value="", io={})
case("touch_skin_file", key="display.skin", value=5, io={})
case("touch_skin_file", key="display.skin", value="dark", io={"get_hermes_home": X("OSError", "x")})
case("touch_skin_file", key="display.skin", value="dark", io={"get_hermes_home": R("/h"), "path_exists": R(True), "touch": X("OSError", "x")})
case("touch_skin_file", key="display.skin", value="dark", io={"get_hermes_home": X("KeyboardInterrupt", "")})

# ── _exit_invalid ────────────────────────────────────────────────────────
for m in ["bad key", "", "Invalid config key: 'x'", "nope", "a b c"]:
    case("exit_invalid", msg=m)

# ── _write_user_config ───────────────────────────────────────────────────
case("write_user_config", config_path="/h/c.yaml", user_config={"model": "x"}, io={"ensure_hermes_home": R(None), "recording_raw_config_write": R(None)})
case("write_user_config", config_path="/h/c.yaml", user_config={}, io={"ensure_hermes_home": X("OSError", "x")})
case("write_user_config", config_path="/h/c.yaml", user_config={"a": 1}, io={"ensure_hermes_home": R(None), "recording_raw_config_write": X("RuntimeError", "x")})
case("write_user_config", config_path="/h/c.yaml", user_config={}, io={"ensure_hermes_home": R(None), "recording_raw_config_write": IMP("ImportError", "no")})
case("write_user_config", config_path="/h/c.yaml", user_config={}, io={})

# ── _print_unknown_key_notice ────────────────────────────────────────────
case("print_unknown_key_notice", key="foo", suggestion=None)
case("print_unknown_key_notice", key="foo", suggestion="food")
case("print_unknown_key_notice", key="a.b", suggestion=None)
case("print_unknown_key_notice", key="a.b", suggestion="a.c")
case("print_unknown_key_notice", key="", suggestion=None)
case("print_unknown_key_notice", key="stt.provider", suggestion="stt")

# ── _dropped_shadow_notes ────────────────────────────────────────────────
UC = {"gateway": {"platforms": {"x": 1}}}
case("dropped_shadow_notes", key="discord.token", legacy_key="gateway.platforms.x",
     state={"user_config": UC},
     io={"unset_nested": {"ret": True, "state": {"user_config": {"gateway": {"platforms": {}}}}},
         "locate_nested": R(None), "drop_legacy_disabled": R(False)})
case("dropped_shadow_notes", key="mcp_servers.s.disabled", legacy_key=None,
     state={"user_config": {"mcp_servers": {"s": {"disabled": True}}}},
     io={"locate_nested": R({"container": "mcp_servers.s", "key": "disabled"}), "drop_legacy_disabled": R(True)})
case("dropped_shadow_notes", key="k", legacy_key="x", state={"user_config": {}},
     io={"unset_nested": R(False), "locate_nested": R(None), "drop_legacy_disabled": R(False)})
case("dropped_shadow_notes", key="k", legacy_key="gateway.platforms.x", state={"user_config": UC},
     io={"unset_nested": {"ret": True, "state": {"user_config": {}}},
         "locate_nested": R({"k": 1}), "drop_legacy_disabled": R(True)})
case("dropped_shadow_notes", key="k", legacy_key="x", state={"user_config": {}},
     io={"unset_nested": X("ValueError", "x")})
case("dropped_shadow_notes", key="k", legacy_key=None, state={"user_config": {}},
     io={"locate_nested": R(None), "drop_legacy_disabled": X("RuntimeError", "x")})
case("dropped_shadow_notes", key="k", legacy_key=None, state={"user_config": {}}, io={})

# ── _usage_exit ──────────────────────────────────────────────────────────
case("usage_exit", usage="Usage: U", examples=["a", "b"], extra=None)
case("usage_exit", usage="Usage: U", examples=[], extra=["x", ""])
case("usage_exit", usage="Usage: U", examples=["a"], extra=["e1", "e2"])
case("usage_exit", usage="Usage: U", examples=["only"], extra=None)
case("usage_exit", usage="", examples=[], extra=None)

# ── _run_write_command ───────────────────────────────────────────────────
case("run_write_command", fn_name="set_config_value", args=["k", "v", False], io={"set_config_value": R(None)})
case("run_write_command", fn_name="set_config_value", args=["k", "v", False], io={"set_config_value": X("RuntimeError", "boom")})
case("run_write_command", fn_name="set_config_value", args=["k", "v"], io={"set_config_value": X("ValueError", "bad")})
case("run_write_command", fn_name="unset_config_value", args=["k"], io={"unset_config_value": R(None)})
case("run_write_command", fn_name="set_config_value", args=["k"], io={})

# ── _cmd_config_get ──────────────────────────────────────────────────────
case("cmd_config_get", args={"key": "model"}, io={"get_config_value": R(None)})
case("cmd_config_get", args={"key": ""})
case("cmd_config_get", args={})
case("cmd_config_get", args={"key": "model", "json": True, "raw": True}, io={"get_config_value": R("v")})
case("cmd_config_get", args={"key": "model"}, io={"get_config_value": X("RuntimeError", "x")})

# ── _cmd_config_set ──────────────────────────────────────────────────────
case("cmd_config_set", args={"key": "k", "value": "v"}, io={"set_config_value": R(None)})
case("cmd_config_set", args={"key": "k", "value": ""}, io={"set_config_value": R(None)})
case("cmd_config_set", args={"key": "k"})
case("cmd_config_set", args={"key": "", "value": "v"})
case("cmd_config_set", args={"key": "k", "value": "v", "force": True}, io={"set_config_value": X("RuntimeError", "boom")})

# ── _cmd_config_unset ────────────────────────────────────────────────────
case("cmd_config_unset", args={"key": "k"}, io={"unset_config_value": R(None)})
case("cmd_config_unset", args={"key": ""})
case("cmd_config_unset", args={})
case("cmd_config_unset", args={"key": "k"}, io={"unset_config_value": X("RuntimeError", "x")})
case("cmd_config_unset", args={"key": "k"}, io={"unset_config_value": X("ValueError", "bad")})

# ── _tools_suffix ────────────────────────────────────────────────────────
case("tools_suffix", info={"tools": ["a", "b", "c"]}, fmt=" -> {}")
case("tools_suffix", info={"tools": ["a"]}, fmt="{}")
case("tools_suffix", info={"tools": []}, fmt="{}")
case("tools_suffix", info={}, fmt="{}")
case("tools_suffix", info={"tools": ["a", "b"]}, fmt="x{}y")
case("tools_suffix", info={"tools": ["a", "b"]}, fmt="no placeholder")

# ── _print_banner ────────────────────────────────────────────────────────
for t in ["Hello", "", "X Y", "Configuration Status", "a"]:
    case("print_banner", text=t)

# ── _cmd_config_show ─────────────────────────────────────────────────────
case("cmd_config_show", io={"show_config": R(None)})
case("cmd_config_show", io={"show_config": X("RuntimeError", "x")})
case("cmd_config_show", io={"show_config": IMP("ImportError", "no")})
case("cmd_config_show", io={})
case("cmd_config_show", io={"show_config": R("shown")})

# ── _is_plugin_dir_name ──────────────────────────────────────────────────
for n in ["telegram", "__pycache__", "__init__", ".hidden", "__", "a__", ""]:
    case("is_plugin_dir_name", name=n)

# ── _platform_manifest_env_entries ───────────────────────────────────────
case("platform_manifest_env_entries", manifest={"requires_env": ["FOO_TOKEN", "BAR"]})
case("platform_manifest_env_entries", manifest={"requires_env": [{"name": "X_KEY", "password": False}]})
case("platform_manifest_env_entries", manifest={"requires_env": [{"name": "Y", "secret": True}]})
case("platform_manifest_env_entries", manifest={"requires_env": ["A"], "optional_env": ["B_SECRET"]}, optional=True)
case("platform_manifest_env_entries", manifest={"requires_env": ["A"], "optional_env": ["B_SECRET"]}, optional=False)
case("platform_manifest_env_entries", manifest={"requires_env": [123, None, "", {"name": ""}]})
case("platform_manifest_env_entries", manifest={"requires_env": [{"name": "Z_JSON", "category": "messaging"}]})

# ── _manifest_secret_envs ────────────────────────────────────────────────
case("manifest_secret_envs", manifests=[["d", {"requires_env": ["FOO_TOKEN", "BAR"]}]], core=["BAR"])
case("manifest_secret_envs", manifests=[["d", {"requires_env": ["FOO_TOKEN"]}]], core=None, io={"core_declared_env_names": R([])})
case("manifest_secret_envs", manifests=[["d", {"requires_env": ["FOO_TOKEN"]}]], core=None, io={"core_declared_env_names": R(["FOO_TOKEN"])})
case("manifest_secret_envs", manifests=[], core=None, io={"core_declared_env_names": R([])})
case("manifest_secret_envs", manifests=[["d", {"requires_env": [{"name": "X_KEY", "category": "other"}]}]], core=[])
case("manifest_secret_envs", manifests=[["d", {"requires_env": ["B_SECRET", "A_TOKEN"]}]], core=[])
case("manifest_secret_envs", manifests=[["d", {"requires_env": ["A_TOKEN"]}]], core=None, io={"core_declared_env_names": X("RuntimeError", "x")})

# ── platform_manifest_secret_envs ────────────────────────────────────────
case("platform_manifest_secret_envs", source="user", io={"platform_plugin_manifests": R([["d", {"requires_env": ["FOO_TOKEN"]}]]), "core_declared_env_names": R([])})
case("platform_manifest_secret_envs", source="bundled", state={"BUNDLED_PLATFORM_SECRET_ENVS": ["A_TOKEN", "B"]}, io={"core_declared_env_names": R(["B"])})
case("platform_manifest_secret_envs", source="bundled", state={"BUNDLED_PLATFORM_SECRET_ENVS": None}, io={"platform_plugin_manifests": R([]), "core_declared_env_names": R([])})
case("platform_manifest_secret_envs", source="bundled", state={"BUNDLED_PLATFORM_SECRET_ENVS": []}, io={"core_declared_env_names": R([])})
case("platform_manifest_secret_envs", source="user", io={"platform_plugin_manifests": X("OSError", "x")})
case("platform_manifest_secret_envs", source="user", io={})

# ── platform_manifest_secret_scan ────────────────────────────────────────
case("platform_manifest_secret_scan", state={}, io={"platform_plugin_manifests": {"ret": [["d", {"requires_env": ["FOO_TOKEN"]}]], "state": {"skipped": []}}, "core_declared_env_names": R([])})
case("platform_manifest_secret_scan", state={}, io={"platform_plugin_manifests": {"ret": [], "state": {"skipped": ["p1"]}}, "core_declared_env_names": R([])})
case("platform_manifest_secret_scan", state={}, io={"platform_plugin_manifests": {"ret": [["d", {"requires_env": ["A_TOKEN"]}]], "state": {"skipped": ["p1"]}}, "core_declared_env_names": R([])})
case("platform_manifest_secret_scan", state={}, io={"platform_plugin_manifests": X("OSError", "x")})
case("platform_manifest_secret_scan", state={}, io={})

# ── _ask_yes_no ──────────────────────────────────────────────────────────
case("ask_yes_no", prompt="Q?", io={"input": R("y")})
case("ask_yes_no", prompt="Q?", io={"input": R("YES")})
case("ask_yes_no", prompt="Q?", io={"input": R("  yes  ")})
case("ask_yes_no", prompt="Q?", io={"input": R("n")})
case("ask_yes_no", prompt="Q?", io={"input": R("maybe")})
case("ask_yes_no", prompt="Q?", io={"input": X("EOFError", "")})
case("ask_yes_no", prompt="Q?", io={"input": X("KeyboardInterrupt", "")})
case("ask_yes_no", prompt="Q?", io={"input": X("RuntimeError", "x")})
case("ask_yes_no", prompt="Q?", io={})

# ── _offer_list ──────────────────────────────────────────────────────────
case("offer_list", heading="H", items=["a", "b"], question="Q?", io={"input": R("y")})
case("offer_list", heading="H", items=["a", "b"], question="Q?", io={"input": R("n")})
case("offer_list", heading="H", items=[], question="Q?", io={"input": R("y")})
case("offer_list", heading="H", items=["a"], question="Q?", io={"input": X("EOFError", "")})
case("offer_list", heading="H", items=["a"], question="Q?", io={"input": X("RuntimeError", "x")})

# ── _prompt_and_save_env ─────────────────────────────────────────────────
case("prompt_and_save_env", name="K", info={"password": False}, prompt="P", state={"results": {"env_added": []}}, io={"line_input": R("val"), "save_env_value": R(None)})
case("prompt_and_save_env", name="K", info={"password": True}, prompt="P", state={"results": {"env_added": []}}, io={"masked_secret_prompt": R("secret"), "save_env_value": R(None)})
case("prompt_and_save_env", name="K", info={"password": False}, prompt="P", state={"results": {"env_added": []}}, io={"line_input": R("")})
case("prompt_and_save_env", name="K", info={"password": False}, prompt="P", state={"results": {"env_added": []}}, io={"line_input": R("   ")})
case("prompt_and_save_env", name="K", info={"password": False}, prompt="P", state={"results": {"env_added": []}}, io={"line_input": R("v"), "save_env_value": X("ValueError", "bad")})
case("prompt_and_save_env", name="K", info={"password": False}, prompt="P", state={"results": {"env_added": []}}, io={"line_input": X("RuntimeError", "x")})
case("prompt_and_save_env", name="K", info={"password": True}, prompt="P", state={"results": {"env_added": []}}, io={"masked_secret_prompt": R("")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func coverage report
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"gen_config_flow2: {len(CASES)} cases, {len(counts)} funcs")
    low = {k: v for k, v in counts.items() if v < 5}
    if low:
        print("WARNING funcs with <5 cases:", low)
    print("wrote", OUT)


if __name__ == "__main__":
    main()