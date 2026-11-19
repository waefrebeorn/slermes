#!/usr/bin/env python3
"""gen_ws_revive3_cases.py — regenerate tests/fixtures/ws_revive3.jsonl.

Case shape: {"func": <cfn>, "in": {...}} with scripted seam results in
in.io (plain value reused / bare array = per-call queue /
{"__script__": true, "ret": ..., "raise": {type, message}}).
List-valued seam RETURNS are wrapped in __script__ (bare arrays are queues).

Fixture scope (documented in ws_revive3.c / oracle_ws_revive3.py):
- numbers: integral literals are ints; no integral float literals.
- str(value) inputs: scalars only (no non-empty containers).
- .lower() / regex \\D inputs: ASCII (plus non-ASCII that both sides
  replace identically in the slug functions).
"""
import hashlib
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "ws_revive3.jsonl"
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_spec(ty, msg):
    return {"__script__": True, "raise": {"type": ty, "message": msg}}


def sha8(s):
    return hashlib.sha1(s.encode("utf-8")).hexdigest()[:8]


# ── ws_u_normalize_whatsapp_allowed_users ──
F = "ws_u_normalize_whatsapp_allowed_users"
case(F, value="a, b ,c")
case(F, value="  ,  , ")
case(F, value="")
case(F, value=None)
case(F, value=0)
case(F, value=False)
case(F, value="12345, 67890")
case(F, value=" a b , c d ")
case(F, value="\u00a0x\u00a0,\u2003y")
case(F, value="a,,b")
case(F, value="solo")
case(F, value="\t,\n")

# ── ws_u_whatsapp_phone_from_identifier ──
F = "ws_u_whatsapp_phone_from_identifier"
case(F, value="1234567890@s.whatsapp.net")
case(F, value="+1 (555) 010-1234:5")
case(F, value="abc")
case(F, value="")
case(F, value=None)
case(F, value="  4915112345678:0  ")
case(F, value="user:123@host")
case(F, value="007")
case(F, value=12345)
case(F, value="12@34")

# ── ws_u_normalize_whatsapp_onboarding_mode ──
F = "ws_u_normalize_whatsapp_onboarding_mode"
case(F, value="bot")
case(F, value=" Self-Chat ")
case(F, value="BOT")
case(F, value=None)
case(F, value="")
case(F, value=0)
case(F, value=False)
case(F, value="self")
case(F, value="chat")

# ── ws_u_string_list ──
F = "ws_u_string_list"
case(F, value=["a", " b ", "", "  ", "c"])
case(F, value="notlist")
case(F, value=None)
case(F, value=[1, 2.5, True, None, " x "])
case(F, value=[])
case(F, value={"a": 1})
case(F, value=["\u00a0", "\u2003z\u00a0"])
case(F, value=[0, False])

# ── ws_u_resolve_toolset_model_plugin ──
F = "ws_u_resolve_toolset_model_plugin"
case(F, ts_key="image_gen", provider_row={"image_gen_plugin_name": "plug"})
case(F, ts_key="image_gen", provider_row={"image_gen_plugin_name": "", "imagegen_backend": "fal"})
case(F, ts_key="image_gen", provider_row={"image_gen_plugin_name": None, "imagegen_backend": 0})
case(F, ts_key="image_gen", provider_row={})
case(F, ts_key="video_gen", provider_row={"video_gen_plugin_name": "v"})
case(F, ts_key="video_gen", provider_row={})
case(F, ts_key="other", provider_row={"image_gen_plugin_name": "x"})
case(F, ts_key="image_gen", provider_row={"image_gen_plugin_name": 0, "imagegen_backend": "nous"})

# ── ws_u_chat_image_extension ──
F = "ws_u_chat_image_extension"
case(F, data_hex="89504e470d0a1a0a0000000d49484452")
case(F, data_hex="ffd8ffe000104a464946")
case(F, data_hex="474946383761")
case(F, data_hex="474946383961")
case(F, data_hex="424d0000")
case(F, data_hex="524946462400000057454250565038")
case(F, data_hex="524946462400000057415645")  # RIFF but WAVE
case(F, data_hex="")
case(F, data_hex="8950")
case(F, data_hex="000102")

# ── ws_u_api_key_display ──
F = "ws_u_api_key_display"
case(F, entry={"api_key": "sk-abc"}, io={"redacted_credential_preview": "PREVIEW"})
case(F, entry={"api_key": "  "})
case(F, entry={"key_env": "OPENAI_API_KEY"})
case(F, entry={"api_key": 0, "key_env": "X"})
case(F, entry={"api_key": "k", "key_env": "X"},
     io={"redacted_credential_preview": raise_spec("RuntimeError", "nope")})
case(F, entry={"api_key": "k"}, io={"redacted_credential_preview": None})
case(F, entry={"api_key": "\u00a0k\u00a0"}, io={"redacted_credential_preview": "P2"})
case(F, entry={})
case(F, entry={"api_key": "k"},
     io={"redacted_credential_preview": raise_spec("SystemExit", "bye")})

# ── ws_u_oauth_provider_disconnect_hint ──
F = "ws_u_oauth_provider_disconnect_hint"
case(F, provider={"flow": "external", "id": "claude-code"}, status={})
case(F, provider={"flow": "external", "id": "other"}, status={})
case(F, provider={"flow": "external", "id": "anthropic"}, status={"source": "env_var"})
case(F, provider={"flow": "pkce", "id": "x"}, status={"source": "env_var"})
case(F, provider={"flow": "pkce"}, status={"source": "file"})
case(F, provider={"flow": "external"}, status={})
case(F, provider={"flow": "external", "id": "anthropic"}, status={})
case(F, provider={}, status={"source": "env_var"})

# ── ws_u_codex_device_code_start_error ──
F = "ws_u_codex_device_code_start_error"
case(F, resp={"status_code": 400, "text": "fallback"},
     io={"resp.json": {"error": {"message": "Device code authorization required",
                                  "code": "device_denied"}}})
case(F, resp={"status_code": 500}, io={"resp.json": {"error": "plain failure"}})
case(F, resp={}, io={"resp.json": None})
case(F, resp={"text": "  boom  "},
     io={"resp.json": raise_spec("ValueError", "bad json")})
case(F, resp={"status_code": 403}, io={"resp.json": {"detail": "Enable device flow"}})
case(F, resp={"status_code": 429, "text": "x" * 600}, io={"resp.json": {}})
case(F, resp={"status_code": 400},
     io={"resp.json": {"error": {"message": "  ", "code": ""}}})
case(F, resp={"status_code": 400}, io={"resp.json": {"error": {"message": None}}})
case(F, resp={}, io={"resp.json": {"message": "device authorization disabled"}})
case(F, resp={}, io={"resp.json": raise_spec("SystemExit", "bye")})
case(F, resp={"status_code": 401, "text": "t"},
     io={"resp.json": {"error": {"error_description": "enable device authorization"}}})

# ── ws_u_read_field ──
F = "ws_u_read_field"
FIELD = {"key": "api_key", "aliases": ["key"], "env_fallbacks": ["FB"]}
case(F, field=FIELD, sources=[{"api_key": "v"}], env={})
case(F, field=FIELD, sources=[{"api_key": None}, {"key": "legacy"}], env={})
case(F, field=FIELD, sources=[{}, {"key": 0}], env={})
case(F, field=FIELD, sources=[{"api_key": False}], env={})
case(F, field=FIELD, sources=[], env={"FB": "envval"})
case(F, field=FIELD, sources=[], env={"FB": ""})
case(F, field=FIELD, sources=[{"other": 1}], env={})
case(F, field={"key": "k", "aliases": ["alias1", "alias2"], "env_fallbacks": []},
     sources=[{"alias2": "second", "alias1": "first"}], env={})
case(F, field={"key": "k", "aliases": [], "env_fallbacks": ["FB1", "FB2"]},
     sources=[], env={"FB2": "two", "FB1": "one"})

# ── ws_u_declared_field_is_set ──
F = "ws_u_declared_field_is_set"
FIELD = {"key": "k", "env_key": "EK", "env_fallbacks": ["FB"], "aliases": ["a"]}
case(F, field=FIELD, sources=[], env={"EK": "1"})
case(F, field=FIELD, sources=[{"k": 0}], env={"EK": ""})
case(F, field=FIELD, sources=[{"a": "x"}], env={})
case(F, field=FIELD, sources=[], env={"FB": "yes"})
case(F, field=FIELD, sources=[], env={"EK": None})
case(F, field=FIELD, sources=[{"k": False}, {"a": []}], env={})
case(F, field=FIELD, sources=[{"k": {}}], env={})
case(F, field=FIELD, sources=[{"k": [1]}], env={})
case(F, field={"key": "k", "env_key": None, "env_fallbacks": [], "aliases": []},
     sources=[{"k": "v"}], env={})

# ── ws_u_serialize_field_value ──
F = "ws_u_serialize_field_value"
case(F, field={"kind": "text", "default": "d"}, value=None)
case(F, field={"kind": "bool", "default": ""}, value=" YES ")
case(F, field={"kind": "bool", "default": ""}, value="off")
case(F, field={"kind": "bool", "default": ""}, value=0)
case(F, field={"kind": "bool", "default": ""}, value=[])
case(F, field={"kind": "bool", "default": ""}, value=[1])
case(F, field={"kind": "bool", "default": "x"}, value=None)
case(F, field={"kind": "json", "default": ""}, value={"a": 1, "b": [1, 2]})
case(F, field={"kind": "json", "default": ""}, value="str")
case(F, field={"kind": "text", "default": ""}, value=42)
case(F, field={"kind": "text", "default": ""}, value=2.5)
case(F, field={"kind": "text", "default": ""}, value=True)
case(F, field={"kind": "json", "default": ""}, value={"u": "\u00e9"})
case(F, field={"kind": "bool", "default": ""}, value="1")

# ── ws_u_command_result ──
F = "ws_u_command_result"
case(F, kind="setup", name="prov", status="ok")
case(F, kind="setup", name="prov", status="ok", command="pip install x",
     completed={"returncode": 0, "stdout": "out", "stderr": "err"})
case(F, kind="setup", name="prov", status="failed",
     completed={"returncode": 1, "stdout": "", "stderr": ""}, error="boom")
case(F, kind="setup", name="prov", status="failed",
     completed={"returncode": 1, "stdout": "o", "stderr": "e"}, error="")
case(F, kind="setup", name="prov", status="ok",
     completed={"returncode": 0, "stdout": "y" * 4500, "stderr": ""})
case(F, kind="setup", name="prov", status="ok",
     completed={"returncode": None, "stdout": None, "stderr": None})
case(F, kind="setup", name="prov", status="ok",
     completed={"returncode": 0, "stdout": "", "stderr": "e"}, error=0)
case(F, kind="setup", name="prov", status="ok", error="solo error")

# ── ws_u_terminal_cfg_value ──
F = "ws_u_terminal_cfg_value"
case(F, terminal_cfg={"ssh_host": "h"}, key="ssh_host", env_var="TERMINAL_SSH_HOST")
case(F, terminal_cfg={"ssh_host": "  "}, key="ssh_host", env_var="E",
     io={"get_env_value": " env "})
case(F, terminal_cfg={}, key="k", env_var="E", io={"get_env_value": None})
case(F, terminal_cfg={"ssh_host": None}, key="ssh_host", env_var="E",
     io={"get_env_value": "x"})
case(F, terminal_cfg={"ssh_host": 0}, key="ssh_host", env_var="E")
case(F, terminal_cfg={"ssh_host": False}, key="ssh_host", env_var="E")
case(F, terminal_cfg={}, key="k", env_var="E",
     io={"get_env_value": raise_spec("ValueError", "bad")})
case(F, terminal_cfg={}, key="k", env_var="E",
     io={"get_env_value": raise_spec("SystemExit", "stop")})
case(F, terminal_cfg={}, key="k", env_var="E")
case(F, terminal_cfg={"ssh_host": 2.5}, key="ssh_host", env_var="E")

# ── ws_u_probe_ssh_backend ──
F = "ws_u_probe_ssh_backend"
case(F, terminal_cfg={"ssh_host": "h", "ssh_user": "u"})
case(F, terminal_cfg={}, io={"get_env_value": ["eh", "eu"]})
case(F, terminal_cfg={"ssh_host": "h"}, io={"get_env_value": ""})
case(F, terminal_cfg={}, io={"get_env_value": ["", ""]})
case(F, terminal_cfg={"ssh_host": " ", "ssh_user": "u"}, io={"get_env_value": ""})
case(F, terminal_cfg={}, io={"get_env_value": raise_spec("RuntimeError", "x")})
case(F, terminal_cfg={},
     io={"get_env_value": raise_spec("SystemExit", "stop")})
case(F, terminal_cfg={"ssh_host": "\u00a0h\u00a0", "ssh_user": "u"})

# ── ws_u_probe_modal_backend ──
F = "ws_u_probe_modal_backend"
case(F, io={"has_direct_modal_credentials": True})
case(F, io={"has_direct_modal_credentials": False, "get_env_value": ["id", "secret"]})
case(F, io={"has_direct_modal_credentials": False, "get_env_value": ["id", ""]})
case(F, io={"has_direct_modal_credentials": False, "get_env_value": ""})
case(F, io={"has_direct_modal_credentials": raise_spec("ValueError", "no"),
            "get_env_value": ["a", "b"]})
case(F, io={"has_direct_modal_credentials": raise_spec("SystemExit", "bye")})
case(F, io={"has_direct_modal_credentials": None, "get_env_value": ["a", "b"]})
case(F)

# ── ws_u_find_toolset_provider_row ──
F = "ws_u_find_toolset_provider_row"
ROWS = script([{"name": "a", "x": 1}, {"name": "b", "x": 2}])
case(F, ts_key="image_gen", config={"c": 1}, provider="b",
     io={"_category_providers": ROWS})
case(F, ts_key="image_gen", config={}, provider="zz",
     io={"_category_providers": ROWS})
case(F, ts_key="image_gen", config={}, provider=None,
     io={"_category_providers": ROWS, "_is_provider_active": [False, True]})
case(F, ts_key="image_gen", config={}, provider="",
     io={"_category_providers": ROWS, "_is_provider_active": [True]})
case(F, ts_key="image_gen", config={}, provider=0,
     io={"_category_providers": ROWS, "_is_provider_active": [False, True]})
case(F, ts_key="image_gen", config={}, provider="x",
     io={"_category_providers": script([])})
case(F, ts_key="image_gen", config={}, provider=None,
     io={"_category_providers": ROWS, "_is_provider_active": [False, False]})
case(F, ts_key="image_gen", config={}, provider=None,
     io={"_category_providers": ROWS,
         "_is_provider_active": raise_spec("RuntimeError", "active fail")})
case(F, ts_key="image_gen", config={}, provider="a",
     io={"_category_providers": ROWS})

# ── ws_u_gc_oauth_sessions ──
F = "ws_u_gc_oauth_sessions"
case(F, oauth_sessions={"a": {"created_at": 500}, "b": {"created_at": 1500}},
     io={"time.time": 2000})
case(F, oauth_sessions={"a": {"created_at": 1100}}, io={"time.time": 2000})
case(F, oauth_sessions={}, io={"time.time": 2000})
case(F, oauth_sessions={"a": {}}, io={"time.time": 2000})
case(F, oauth_sessions={"a": {"created_at": 1}},
     io={"time.time": raise_spec("SystemExit", "bye")})
case(F, oauth_sessions={"a": {"created_at": "x"}}, io={"time.time": 2000})
case(F, oauth_sessions={"a": {"created_at": 1100.2}, "b": {"created_at": 1100.7}},
     io={"time.time": 2000.5})
case(F, oauth_sessions={"a": {"created_at": 1}})
case(F, oauth_sessions={"a": {"created_at": 10}, "b": {"created_at": 20},
                        "c": {"created_at": 1900}}, io={"time.time": 2000})

# ── ws_u_gc_mcp_oauth_flows ──
F = "ws_u_gc_mcp_oauth_flows"
case(F, mcp_oauth_flows={"a": {"created_at": 500}, "b": {"created_at": 1500}, "c": {}},
     io={"time.time": 2000})
case(F, mcp_oauth_flows={}, io={"time.time": 2000})
case(F, mcp_oauth_flows={"a": {"created_at": 0}, "b": {}}, io={"time.time": 100})
case(F, mcp_oauth_flows={"a": {"created_at": "x"}}, io={"time.time": 2000})
case(F, mcp_oauth_flows={"a": {"created_at": 1}},
     io={"time.time": raise_spec("KeyboardInterrupt", "")})
case(F, mcp_oauth_flows={"a": {"created_at": 1100}}, io={"time.time": 2000})
case(F, mcp_oauth_flows={"a": {"created_at": 5}}, io={"time.time": 2000.5})

# ── ws_u_prune_telegram_onboarding_pairings ──
F = "ws_u_prune_telegram_onboarding_pairings"
case(F, telegram_onboarding_pairings={
        "a": {"expires_at_ts": 999}, "b": {"expires_at_ts": 1000},
        "c": {"expires_at_ts": 1001}}, io={"time.time": 1000})
case(F, telegram_onboarding_pairings={}, io={"time.time": 1000})
case(F, telegram_onboarding_pairings={"a": {}}, io={"time.time": 1000})
case(F, telegram_onboarding_pairings={"a": {"expires_at_ts": "x"}},
     io={"time.time": 1000})
case(F, telegram_onboarding_pairings={"a": {"expires_at_ts": 5}},
     io={"time.time": "now"})
case(F, telegram_onboarding_pairings={}, io={"time.time": "now"})
case(F, telegram_onboarding_pairings={"a": {"expires_at_ts": 1}},
     io={"time.time": raise_spec("GeneratorExit", "")})
case(F, telegram_onboarding_pairings={"a": {"expires_at_ts": 999.5}},
     io={"time.time": 1000})

# ── ws_u_mcp_install_action_name ──
F = "ws_u_mcp_install_action_name"
case(F, name="My Server!", action_log_files={})
case(F, name="!!!", action_log_files={})
case(F, name="a" * 60, action_log_files={})
_a = f"mcp-install-x-{sha8('x')}"
case(F, name="x", action_log_files={_a: "custom.log"})
case(F, name="\u00dcn\u00efcode", action_log_files={})
case(F, name="", action_log_files={})
case(F, name="a-b_c", action_log_files={})
case(F, name="x")

# ── ws_u_hub_action_name ──
F = "ws_u_hub_action_name"
case(F, verb="install", key="My Skill", profile=None, action_log_files={})
case(F, verb="update", key="s", profile="  DEFAULT ", action_log_files={})
case(F, verb="install", key="s", profile="", action_log_files={})
case(F, verb="install", key="s", profile=0, action_log_files={})
case(F, verb="uninstall", key="!!!", profile="p", action_log_files={})
_n = f"skills-install-s-{sha8('work:s')}"
case(F, verb="install", key="s", profile="Work", action_log_files={_n: "keep.log"})
case(F, verb="install", key="s", profile="Work", action_log_files={})

# ── ws_u_profile_cli_args ──
F = "ws_u_profile_cli_args"
case(F, profile=None)
case(F, profile="current")
case(F, profile=" CURRENT ")
case(F, profile="")
case(F, profile="work",
     io={"_resolve_profile_dir": "/h/work", "normalize_profile_name": "work"})
case(F, profile="work",
     io={"_resolve_profile_dir": raise_spec("ValueError", "bad profile")})
case(F, profile="work",
     io={"_resolve_profile_dir": "/h/work",
         "normalize_profile_name": raise_spec("RuntimeError", "norm fail")})
case(F, profile="Work",
     io={"_resolve_profile_dir": "/h/Work", "normalize_profile_name": "work"})
case(F, profile="work")

# ── ws_u_pairing_store ──
F = "ws_u_pairing_store"
case(F, profile=None, io={"PairingStore": script({"store": "default"})})
case(F, profile="current", io={"PairingStore": script({"store": "default"})})
case(F, profile="work",
     io={"_resolve_profile_dir": "/h/work",
         "PairingStore": script({"store": "work"})})
case(F, profile="work",
     io={"_resolve_profile_dir": raise_spec("ValueError", "unknown profile")})
case(F, profile="  ", io={"PairingStore": script({"store": "default"})})
case(F, profile=0, io={"PairingStore": script({"store": "default"})})
case(F, profile=None, io={"PairingStore": raise_spec("RuntimeError", "store fail")})

# ── ws_u_memory_provider_options ──
F = "ws_u_memory_provider_options"
case(F, io={"list_memory_provider_names": script(["a", "b"])})
case(F, io={"list_memory_provider_names": script(["a", "a", ""])})
case(F, io={"list_memory_provider_names": raise_spec("ValueError", "disc fail")})
case(F, io={"list_memory_provider_names": script([])})
case(F, io={"list_memory_provider_names": "ab"})
case(F, io={"list_memory_provider_names": script({"k1": 1, "k2": 2})})
case(F, io={"list_memory_provider_names": 5})
case(F, io={"list_memory_provider_names": raise_spec("SystemExit", "bye")})
case(F)

# ── ws_u_validate_dashboard_cron_context_from ──
F = "ws_u_validate_dashboard_cron_context_from"
case(F, refs=None, profile_name="p1")
case(F, refs=[], profile_name="p1")
case(F, refs=["self", " SELF "], profile_name="p1")
case(F, refs=["job1"], profile_name="p1",
     io={"_call_cron_for_profile": {"id": "job1"}})
case(F, refs=["job1"], profile_name="p1", io={"_call_cron_for_profile": None})
case(F, refs=["ok", "bad"], profile_name="p1",
     io={"_call_cron_for_profile": [{"id": "ok"}, None]})
case(F, refs=[5], profile_name="p1", io={"_call_cron_for_profile": None})
case(F, refs=[None], profile_name="p1", io={"_call_cron_for_profile": None})
case(F, refs=["bad"], profile_name="p1",
     io={"_call_cron_for_profile": raise_spec("SystemExit", "bye")})
case(F, refs=["bad"], profile_name=None, io={"_call_cron_for_profile": None})
case(F, refs="ab", profile_name="p1", io={"_call_cron_for_profile": [None, None]})
case(F, refs=["self", "j2"], profile_name="p1",
     io={"_call_cron_for_profile": {"id": "j2"}})

# ── ws_u_channel_managed_env_keys ──
F = "ws_u_channel_managed_env_keys"
case(F, io={"_messaging_platform_catalog": script(
    [{"env_vars": ["A", "B"]}, {"env_vars": ["B", "C"]}])})
case(F, io={"_messaging_platform_catalog": script([])})
case(F, io={"_messaging_platform_catalog": script([{}])})
case(F, io={"_messaging_platform_catalog": script([{"env_vars": "AB"}])})
case(F, io={"_messaging_platform_catalog": script([{"env_vars": {"K1": 1}}])})
case(F, io={"_messaging_platform_catalog": script([5])})
case(F, io={"_messaging_platform_catalog": script([{"env_vars": 5}])})
case(F, io={"_messaging_platform_catalog": script([{"env_vars": [["x"]]}])})
case(F, io={"_messaging_platform_catalog": raise_spec("ValueError", "cat fail")})
case(F, io={"_messaging_platform_catalog": raise_spec("GeneratorExit", "")})
case(F, io={"_messaging_platform_catalog": "x"})
case(F, io={"_messaging_platform_catalog": None})
case(F, io={"_messaging_platform_catalog": script(
    [{"env_vars": ["Z", "A"]}, {"env_vars": ["A", "M"]}])})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert len(counts) == 27, f"expected 27 funcs, got {len(counts)}"
    for fn, n in sorted(counts.items()):
        assert n >= 5, f"{fn}: only {n} cases"
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases for {len(counts)} funcs -> {OUT}")


if __name__ == "__main__":
    main()
