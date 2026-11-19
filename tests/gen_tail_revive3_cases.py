#!/usr/bin/env python3
"""gen_tail_revive3_cases.py — regenerate tests/fixtures/tail_revive3.jsonl.

Case shape: {"func": <c_name>, "in": {...}}. Scripted seams live in in.io:
a plain JSON value is reused on every call; a bare LIST is a per-call
queue; list RETURNS must be wrapped {"__script__": true, "ret": [...]};
raises are {"__script__": true, "raise": {"type": T, "message": M}}.
Missing/exhausted scripts fail closed (RuntimeError) on both sides.

Run:  python tests/gen_tail_revive3_cases.py
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "tail_revive3.jsonl"


def script_ret(v):
    return {"__script__": True, "ret": v}


def script_raise(ty, msg=""):
    return {"__script__": True, "raise": {"type": ty, "message": msg}}


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── is_lightpanda_process ───────────────────────────────────────────────────
LP_OK = {"psutil_process": None, "proc_name": "LightPanda.exe",
         "proc_cmdline": script_ret(["lightpanda", "serve", "--port", "9222"])}
add("is_lightpanda_process", pid=4242, port=9222, start_time=None, io=dict(LP_OK))
add("is_lightpanda_process", pid=4242, port=9222, start_time=123.5,
    io=dict(LP_OK, get_process_start_time=123.5))
add("is_lightpanda_process", pid=4242, port=9222, start_time=123.5,
    io=dict(LP_OK, get_process_start_time=999.0))
add("is_lightpanda_process", pid=4242, port=9222, start_time=None,
    io=dict(LP_OK, proc_name="chrome.exe"))
add("is_lightpanda_process", pid=4242, port=9222, start_time=None,
    io=dict(LP_OK, proc_cmdline=script_ret(["lightpanda", "--port", "9222"])))
add("is_lightpanda_process", pid=4242, port=9333, start_time=None, io=dict(LP_OK))
add("is_lightpanda_process", pid=4242, port=9222, start_time=None,
    io={"psutil_process": script_raise("NoSuchProcess", "gone")})
add("is_lightpanda_process", pid=4242, port=9222, start_time=None,
    io={"psutil_process": script_raise("SystemExit", "bye")})
add("is_lightpanda_process", pid=4242, port=9222, start_time=0,
    io=dict(LP_OK, get_process_start_time=1.0))
add("is_lightpanda_process", pid=4242, port="9222", start_time=None,
    io=dict(LP_OK, proc_cmdline=script_ret(["serve", "9222"])))
add("is_lightpanda_process", pid=4242, port=9222, start_time=None,
    io=dict(LP_OK, proc_cmdline="lightpanda serve 9222"))
add("is_lightpanda_process", pid=4242, port=9222, start_time=True,
    io=dict(LP_OK, get_process_start_time=1))
add("is_lightpanda_process", pid=4242, port=9222, start_time=None,
    io=dict(LP_OK, proc_name=5))
add("is_lightpanda_process", pid=4242, port=9222, start_time=100,
    io=dict(LP_OK, get_process_start_time=100.0))
add("is_lightpanda_process", pid=4242, port=9222, start_time=None,
    io=dict(LP_OK, proc_cmdline=script_raise("PermissionError", "no")))
add("is_lightpanda_process", pid=4242, port=None, start_time=None,
    io=dict(LP_OK, proc_cmdline=script_ret(["serve", "None"])))

# ── desktop_loopback_auth_exempt ────────────────────────────────────────────
add("desktop_loopback_auth_exempt", host="127.0.0.1",
    env={"HERMES_DESKTOP": "1", "HERMES_DASHBOARD_SESSION_TOKEN": "t"})
add("desktop_loopback_auth_exempt", host="localhost",
    env={"HERMES_DESKTOP": "1"}, ssh_session_token="s")
add("desktop_loopback_auth_exempt", host="::1",
    env={"HERMES_DESKTOP": "1"}, ssh_owner_nonce="n")
add("desktop_loopback_auth_exempt", host="example.com",
    env={"HERMES_DESKTOP": "1", "HERMES_DASHBOARD_SESSION_TOKEN": "t"})
add("desktop_loopback_auth_exempt", host="127.0.0.1", env={})
add("desktop_loopback_auth_exempt", host="127.0.0.1",
    env={"HERMES_DESKTOP": "0", "HERMES_DASHBOARD_SESSION_TOKEN": "t"})
add("desktop_loopback_auth_exempt", host="127.0.0.1",
    env={"HERMES_DESKTOP": "1", "HERMES_DASHBOARD_SESSION_TOKEN": ""},
    ssh_session_token=None, ssh_owner_nonce=None)
add("desktop_loopback_auth_exempt", host=None,
    env={"HERMES_DESKTOP": "1", "HERMES_DASHBOARD_SESSION_TOKEN": "t"})
add("desktop_loopback_auth_exempt", host="127.0.0.1",
    env={"HERMES_DESKTOP": "1"}, ssh_session_token="", ssh_owner_nonce="")
add("desktop_loopback_auth_exempt", host="127.0.0.1",
    env={"HERMES_DESKTOP": "1", "HERMES_DASHBOARD_SESSION_TOKEN": ""},
    ssh_owner_nonce="n")

# ── is_openai_family_main ───────────────────────────────────────────────────
def ft_io(provider, model):
    return {"read_main_provider": provider, "read_main_model": model}


add("is_openai_family_main", io=ft_io("openai", "gpt-4o"))
add("is_openai_family_main", io=ft_io("OpenAI-Codex", ""))
add("is_openai_family_main", io=ft_io("openrouter", "openai/gpt-4o"))
add("is_openai_family_main", io=ft_io("nous", "anthropic/claude"))
add("is_openai_family_main", io=ft_io("azure-openai", "whatever"))
add("is_openai_family_main", io=ft_io("", "o3-mini"))
add("is_openai_family_main", io=ft_io("openrouter", "gpt.5"))
add("is_openai_family_main", io=ft_io("openrouter", "chatgpt-latest"))
add("is_openai_family_main", io=ft_io("openrouter", "codex-1"))
add("is_openai_family_main", io=ft_io("openrouter", "openai/o1"))
add("is_openai_family_main", io=ft_io(None, None))
add("is_openai_family_main", io={"read_main_provider": 5})
add("is_openai_family_main",
    io={"read_main_provider": "openai",
        "read_main_model": script_raise("ValueError", "bad")})
add("is_openai_family_main",
    io={"read_main_provider": script_raise("SystemExit", "stop")})
add("is_openai_family_main", io=ft_io("  OpenAI  ", "x"))
add("is_openai_family_main", io=ft_io("openrouter", "meta/llama"))
add("is_openai_family_main", io=ft_io(False, "gpt-4"))
add("is_openai_family_main", io=ft_io("codex", "x"))
add("is_openai_family_main", io=ft_io("openrouter", "o4-mini"))
add("is_openai_family_main", io=ft_io("openrouter", "o5"))
add("is_openai_family_main", io=ft_io("openrouter", "o1x"))
add("is_openai_family_main", io=ft_io("openrouter", "xgpt-4"))

# ── looks_like_opaque_credential ────────────────────────────────────────────
for v in ["***", "\u00abredacted:token\u00bb", "abcdef0123456789",
          "abcdefghijkl", "abcdefghijklmnopqrst", "CPU", "local",
          "Abcdef123456", "abc123", "aaaaaaaaaaaa1", "AAAAAAAAAAAA",
          "aB" + "\u00e9" * 9, "aB" + "\u00e9" * 10,
          "sk-ant-oat01-" + "1234567" + "abcdef",
          "ghp_" + "1234567890" + "abcdefghij",
          "abcdefghij123456789", "abcdefghijklmnopqrs",
          "hello world123", "1234567890123456", "zzzzzzzzzzzzzzzz",
          "ABCDEF0123456789ABC", "token_./+=-abcdefghij"]:
    add("looks_like_opaque_credential", value=v)

# ── has_top_level_alternation / has_nested_unbounded_repeat ─────────────────
PATTERNS = ["ab|cd", "ab(?:x|y)", "(a+)+", "(a{2,})+", "(a{2,3})+",
            "(a|aa)+", "a*b", "[a|b]+", "\\|x", "(a(b+)+)", "a{2,}",
            "(?:a+)*", "((a+)*)+", "a|b|c", "(a|b)", "[]", "[)]+",
            "(a{2,})*", "x{abc,}", "(a+){2}", "(a+)*x|y", "", "(a", "a)",
            "\\(a+)+", "(a{,})+", "[\\]]+", "(a+|b)+"]
for p in PATTERNS:
    add("has_top_level_alternation", pattern=p)
    add("has_nested_unbounded_repeat", pattern=p)

# ── in_excluded_root_dir ────────────────────────────────────────────────────
for p in ["models/foo", "runtimes/x", "node/x", "installs/x", "tools/x",
          "cache/foo", "cache/images/x", "cache/generated/x", "cache/audio",
          "browser_profiles/x", "bws_cache.json", "op_cache.json",
          "photon-sidecar.json", "bws_cache.enc.json",
          "platforms/whatsapp/bridge.log", "platforms/whatsapp/bridge.log.1",
          "whatsapp/bridge.log", "whatsapp/bridge.log.1", "whatsapp/other.log",
          "profiles/p1/models/x", "profiles/p1/cache/x",
          "profiles/p1/cache/images/x", "profiles/p1/whatsapp/bridge.log",
          "profiles/p1/skills/x", "skills/x", "", ".", "a/./b",
          "//models//x/", "/models/x", "profiles/p1", "cache",
          "profiles/p1/browser_profiles/x", "cache/documents",
          "cache/citations/x", "cache/screenshots", "cache/videos",
          "cache/other", "profiles/p1/tools/x", "cache/images",
          "platforms/whatsapp/bridge.log.2", "a/../models"]:
    add("in_excluded_root_dir", path=p)

# ── is_oauth_pool_payload ───────────────────────────────────────────────────
for e in [{"auth_type": "oauth"}, {"auth_type": " OAuth "},
          {"auth_type": "OAUTH", "refresh_token": ""},
          {"refresh_token": "r"}, {"refresh_token": "   "},
          {"access_token": "sk-ant-oat01-" + "1234567"},
          {"access_token": "sk-ant-api03-xxx"}, {},
          {"auth_type": "api_key"}, None, [], "oauth", 5,
          {"auth_type": None, "refresh_token": None, "access_token": None},
          {"auth_type": 0}, {"refresh_token": True}, {"refresh_token": 123},
          {"refresh_token": ["x"]}, {"refresh_token": {}},
          {"auth_type": {"a": 1}}, {"access_token": "sk-ant-oat"},
          {"auth_type": "oauthx"},
          {"auth_type": False, "access_token": "sk-ant-oat01-x"},
          {"refresh_token": 0}, {"refresh_token": False},
          {"auth_type": "oauth", "refresh_token": "r"}]:
    add("is_oauth_pool_payload", entry=e)

# ── query_is_concrete ───────────────────────────────────────────────────────
for q in ["a=b", "a", "", "a&", "&a", "a=b&c=d", "a=b&c", "=v", "a=",
          "x.md", "?x", "%41", "%4", "%GG", "a~b_c-d", "a.b=c",
          "h\u00e9llo", "a&b&c", "a=b&&c=d", "%41%42", "a%41", "a%4",
          "x?=y", "a+b", "a/b", "flag", "a=b=c", "%zz", "~-_"]:
    add("query_is_concrete", query=q)

# ── content_type_is_sse ─────────────────────────────────────────────────────
for h in [{"Content-Type": "text/event-stream"},
          {"content-type": "TEXT/EVENT-STREAM; charset=utf-8"},
          {"Content-Type": "application/json"}, {}, None,
          {"Content-Type": "", "content-type": "text/event-stream"},
          {"Content-Type": 0, "content-type": "text/event-stream"},
          {"Content-Type": False, "content-type": "text/event-stream"},
          {"Content-Type": "text/event-stream",
           "content-type": "application/json"},
          {"Content-Type": "application/json",
           "content-type": "text/event-stream"},
          [], "text/event-stream", 5, True,
          {"Content-Type": "Text/Event-Stream"},
          {"Content-Type": "text/event-stream "},
          {"content-type": None}, {"Content-Type": 123},
          {"Content-Type": "text/event-streamx"},
          {"content-type": "application/json"}]:
    add("content_type_is_sse", headers=h)

# ── is_managed_endpoint ─────────────────────────────────────────────────────
add("is_managed_endpoint", base_url="http://127.0.0.1:8080/",
    io={"state_endpoint": {"base_url": "http://127.0.0.1:8080"}})
add("is_managed_endpoint", base_url="http://x/",
    io={"state_endpoint": {"base_url": "http://y"}})
add("is_managed_endpoint", base_url=None, io={"state_endpoint": {"base_url": ""}})
add("is_managed_endpoint", base_url="http://x", io={"state_endpoint": None})
add("is_managed_endpoint", base_url="http://x",
    io={"state_endpoint": script_raise("ValueError", "bad")})
add("is_managed_endpoint", base_url="http://x",
    io={"state_endpoint": script_raise("SystemExit", "halt")})
add("is_managed_endpoint", base_url="http://x", io={"state_endpoint": []})
add("is_managed_endpoint", base_url="http://x",
    io={"state_endpoint": {"base_url": None}})
add("is_managed_endpoint", base_url="None",
    io={"state_endpoint": {"base_url": None}})
add("is_managed_endpoint", base_url="http://x///",
    io={"state_endpoint": {"base_url": "http://x/"}})
add("is_managed_endpoint", base_url="http://x", io={"state_endpoint": {}})
add("is_managed_endpoint", base_url="", io={"state_endpoint": {}})
add("is_managed_endpoint", base_url="123", io={"state_endpoint": {"base_url": 123}})
add("is_managed_endpoint", base_url="http://x", io={"state_endpoint": "str"})
add("is_managed_endpoint", base_url="http://x",
    io={"state_endpoint": {"base_url": True}})
add("is_managed_endpoint", base_url="True",
    io={"state_endpoint": {"base_url": True}})

# ── uses_room_run_auth ──────────────────────────────────────────────────────
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": "tok"})
add("uses_room_run_auth", path="/api/v1/runs", io={"room_grant_token": "tok"})
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": ""})
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": None})
add("uses_room_run_auth", path="/v1/runs/", io={"room_grant_token": "tok"})
add("uses_room_run_auth", path="/v1/runsX", io={"room_grant_token": "tok"})
add("uses_room_run_auth", path="", io={"room_grant_token": "tok"})
add("uses_room_run_auth", path="v1/runs", io={"room_grant_token": "tok"})
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": 0})
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": ["x"]})
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": {}})
add("uses_room_run_auth", path="/v1/runs",
    io={"room_grant_token": script_raise("PermissionError", "denied")})
add("uses_room_run_auth", path="/v1/runs",
    io={"room_grant_token": script_raise("SystemExit", "exit")})
add("uses_room_run_auth", path="/v1/run", io={"room_grant_token": "tok"})
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": False})
add("uses_room_run_auth", path="/v1/runs", io={"room_grant_token": {"a": 1}})

# ── memory_provider_tools_exposed ───────────────────────────────────────────
add("memory_provider_tools_exposed",
    agent={"tools": [{"function": {"name": "memory"}}]})
add("memory_provider_tools_exposed", agent={})
add("memory_provider_tools_exposed", agent=None)
add("memory_provider_tools_exposed", agent={"enabled_toolsets": []})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ["memory"]})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ["web"]},
    io={"resolve_toolset": script_ret(["web", "memory"])})
add("memory_provider_tools_exposed",
    agent={"enabled_toolsets": ["web", "core"]},
    io={"resolve_toolset": [script_ret(["web"]), script_ret(["core", "memory"])]})
add("memory_provider_tools_exposed",
    agent={"tools": [{"function": {"name": "memory"}}],
           "disabled_toolsets": ["memory"]})
add("memory_provider_tools_exposed",
    agent={"disabled_toolsets": ["all"], "enabled_toolsets": ["memory"]})
add("memory_provider_tools_exposed", agent={"disabled_toolsets": ["*"]})
add("memory_provider_tools_exposed",
    agent={"disabled_toolsets": [], "enabled_toolsets": None})
add("memory_provider_tools_exposed", agent={"tools": [{"function": None}]})
add("memory_provider_tools_exposed", agent={"tools": [{"function": 5}]})
add("memory_provider_tools_exposed",
    agent={"tools": [{"function": {"name": "memory"}}, {"function": None}]})
add("memory_provider_tools_exposed",
    agent={"tools": ["x", 5], "enabled_toolsets": ["web"]},
    io={"resolve_toolset": script_ret(["web"])})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ["web"]},
    io={"resolve_toolset": script_raise("ValueError", "bad")})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ["web"]},
    io={"resolve_toolset": script_raise("SystemExit", "stop")})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ["web"]},
    io={"resolve_toolset": 5})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": "memory"})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": "web"},
    io={"resolve_toolset": [script_ret(["memory"])]})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": {"web": 1}},
    io={"resolve_toolset": script_ret(["memory"])})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": {"memory": 1}})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": 5})
add("memory_provider_tools_exposed", agent={"disabled_toolsets": 5})
add("memory_provider_tools_exposed",
    agent={"disabled_toolsets": "memory", "enabled_toolsets": None})
add("memory_provider_tools_exposed", agent={"disabled_toolsets": {"memory": 1}})
add("memory_provider_tools_exposed",
    agent={"tools": [{"function": {"name": "other"}}],
           "enabled_toolsets": ["web"]},
    io={"resolve_toolset": script_ret(["memory"])})
add("memory_provider_tools_exposed",
    agent={"tools": {}, "enabled_toolsets": []})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ["web"]},
    io={"resolve_toolset": "xmemoryx"})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ["web"]},
    io={"resolve_toolset": {"memory": 1}})
add("memory_provider_tools_exposed",
    agent={"tools": [{"function": {"name": "memory"}}],
           "enabled_toolsets": ["web"]})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": False})
add("memory_provider_tools_exposed", agent={"enabled_toolsets": ""})
add("memory_provider_tools_exposed",
    agent={"tools": [{"function": "x"}]})
add("memory_provider_tools_exposed",
    agent={"tools": [{"function": {"name": 5}}], "enabled_toolsets": []})

# ── metadata (_merge_request_overrides) ─────────────────────────────────────
add("metadata", runtime_overrides=None, slot_extra_body=None)
add("metadata", runtime_overrides={"a": 1}, slot_extra_body=None)
add("metadata", runtime_overrides={"a": 1}, slot_extra_body={"b": 2})
add("metadata", runtime_overrides={"extra_body": {"x": 1}},
    slot_extra_body={"y": 2})
add("metadata", runtime_overrides={"extra_body": {"x": 1}},
    slot_extra_body={"x": 9, "y": 2})
add("metadata", runtime_overrides={"extra_body": None},
    slot_extra_body={"y": 2})
add("metadata", runtime_overrides={"extra_body": {}}, slot_extra_body={"y": 2})
add("metadata", runtime_overrides={}, slot_extra_body={})
add("metadata", runtime_overrides={"a": 1}, slot_extra_body={})
add("metadata", runtime_overrides={"a": 1}, slot_extra_body=5)
add("metadata", runtime_overrides={"a": 1}, slot_extra_body="x")
add("metadata", runtime_overrides={"a": 1}, slot_extra_body=[])
add("metadata", runtime_overrides={"a": 1}, slot_extra_body=True)
add("metadata", runtime_overrides="x", slot_extra_body=None)
add("metadata", runtime_overrides=5, slot_extra_body=None)
add("metadata", runtime_overrides=True, slot_extra_body=None)
add("metadata", runtime_overrides={"extra_body": 5}, slot_extra_body={"y": 2})
add("metadata", runtime_overrides={"extra_body": "x"}, slot_extra_body={"y": 2})
add("metadata", runtime_overrides=0, slot_extra_body={"b": 2})
add("metadata", runtime_overrides="", slot_extra_body={"b": 2})
add("metadata", runtime_overrides=False, slot_extra_body=None)
add("metadata", runtime_overrides=[], slot_extra_body=None)
add("metadata", runtime_overrides={"extra_body": {"x": 1}, "a": 2},
    slot_extra_body={"y": 3})
add("metadata", runtime_overrides={"extra_body": False},
    slot_extra_body={"y": 2})
add("metadata", runtime_overrides={"extra_body": {"x": 1}},
    slot_extra_body=None)
add("metadata", runtime_overrides={"extra_body": []}, slot_extra_body={"y": 2})

# ── uses_hermes_python_environment ──────────────────────────────────────────
add("uses_hermes_python_environment", python_path="/usr/bin/python3",
    sys_executable="/usr/bin/python3", sys_prefix="/usr")
add("uses_hermes_python_environment", python_path="/a/python",
    sys_executable="/b/python", sys_prefix="/usr",
    io={"realpath": ["/real/python", "/real/python"]})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": ["/x", "/y", "/p"], "python_environment_prefix": "/p"})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": ["/x", "/y", "/p"], "python_environment_prefix": "/q"})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": script_raise("OSError", "bad")})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": ["/x", script_raise("SystemExit", "halt")]})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": ["/x", "/y"],
        "python_environment_prefix": script_raise("ValueError", "probe")})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": ["/x", "/y", script_raise("FileNotFoundError", "gone")],
        "python_environment_prefix": "/p"})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr", io={"realpath": [5, 5]})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr", io={"realpath": [None, None]})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": ["/x", "/y", None], "python_environment_prefix": None})
add("uses_hermes_python_environment", python_path="/a", sys_executable="/a",
    sys_prefix="/usr")
add("uses_hermes_python_environment", python_path="/a", sys_executable="/b",
    sys_prefix="/usr",
    io={"realpath": ["/x", "/y", "/q"], "python_environment_prefix": "/p"})

# ── strict_path_exists ──────────────────────────────────────────────────────
add("strict_path_exists", path="/x/lock", label="lock", io={"stat": {}})
add("strict_path_exists", path="/x/lock", label="lock", io={"stat": None})
add("strict_path_exists", path="/x/lock", label="lock",
    io={"stat": script_raise("FileNotFoundError", "missing")})
add("strict_path_exists", path="/x/lock", label="lock",
    io={"stat": script_raise("PermissionError", "denied")})
add("strict_path_exists", path="/x/lock", label="gateway runtime lock",
    io={"stat": script_raise("OSError", "bad")})
add("strict_path_exists", path="/x/lock", label="lock",
    io={"stat": script_raise("SystemExit", "halt")})
add("strict_path_exists", path="/x/lock", label="lock",
    io={"stat": script_raise("ValueError", "v")})
add("strict_path_exists", path="/x/lock", label="x",
    io={"stat": script_raise("TimeoutError", "t")})
add("strict_path_exists", path="/x/lock", label="",
    io={"stat": script_raise("PermissionError", "")})
add("strict_path_exists", path="/x/lock", label="lock",
    io={"stat": script_raise("KeyboardInterrupt", "")})
add("strict_path_exists", path="/x/lock", label="lock",
    io={"stat": {"st_mode": 0}})
add("strict_path_exists", path="/x/lock", label="L",
    io={"stat": script_raise("NotADirectoryError", "nd")})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    assert not short, f"funcs with <5 cases: {short}"
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()