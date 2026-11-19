#!/usr/bin/env python3
"""gen_tail_revive4_cases.py — emit tests/fixtures/tail_revive4.jsonl.

>=5 cases per implemented function of the tail group-4 revival. Inputs only;
the oracle computes expected results from the live quarry defs. Run:
  python tests/gen_tail_revive4_cases.py
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_=None):
    s = {"__script__": True}
    if raise_ is not None:
        s["raise"] = raise_
    else:
        s["ret"] = ret
    return s


def raiser(ty, msg=""):
    return {"type": ty, "message": msg}


# ── is_post_tool_replay ─────────────────────────────────────────────────────
add("is_post_tool_replay", messages=None)
add("is_post_tool_replay", messages=[])
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "call_1"}]},
    {"role": "tool", "tool_call_id": "call_1"},
])
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "fc_abc"}]},
    {"role": "tool", "tool_call_id": "call_abc|fc_abc"},
])
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "call_1"}]},
    {"role": "tool", "tool_call_id": ""},
])
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "call_1"}]},
    {"role": "user", "content": "hi"},
])
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "c1"}]},
    {"role": "tool", "tool_call_id": "c1"},
    {"role": "system", "content": "s"},
])
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "fc_x", "call_id": "call_x"}]},
    {"role": "tool", "tool_call_id": "call_x"},
])
add("is_post_tool_replay", messages=5)
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "call_1"}]},
    {"role": "tool", "tool_call_id": "   "},
])
add("is_post_tool_replay", messages=[
    {"role": "assistant", "tool_calls": [{"id": "call_1"}, {"id": "call_2"}]},
    {"role": "tool", "tool_call_id": "call_9"},
    {"role": "tool", "tool_call_id": "call_2"},
])

# ── is_opencode_responses_backend ───────────────────────────────────────────
OC_OK = {"import_opencode_provider_family": None,
         "import_base_url_hostname": None}
add("is_opencode_responses_backend",
    params={"provider": "opencode-zen"},
    io={**OC_OK, "opencode_provider_family": "opencode-zen"})
add("is_opencode_responses_backend",
    params={"base_url": "https://opencode.ai/zen/v1"},
    io={**OC_OK, "opencode_provider_family": None,
        "base_url_hostname": "opencode.ai"})
add("is_opencode_responses_backend",
    params={"base_url": "https://opencode.ai"},
    io={**OC_OK, "opencode_provider_family": None,
        "base_url_hostname": "OPENCODE.AI"})
add("is_opencode_responses_backend",
    params={"base_url": "https://other.ai"},
    io={**OC_OK, "opencode_provider_family": None,
        "base_url_hostname": "other.ai"})
add("is_opencode_responses_backend",
    params={"base_url": "https://opencode.ai"},
    io={"import_opencode_provider_family": script(raise_=raiser("ImportError", "no models")),
        "import_base_url_hostname": None, "base_url_hostname": "opencode.ai"})
add("is_opencode_responses_backend",
    params={"provider": "x", "base_url": "https://x.ai"},
    io={**OC_OK,
        "opencode_provider_family": script(raise_=raiser("RuntimeError", "boom")),
        "base_url_hostname": "x.ai"})
add("is_opencode_responses_backend",
    params={"base_url": "https://opencode.ai"},
    io={**OC_OK, "opencode_provider_family": None,
        "import_base_url_hostname": script(raise_=raiser("ImportError", "no utils"))})
add("is_opencode_responses_backend",
    params={"base_url": "https://opencode.ai"},
    io={**OC_OK, "opencode_provider_family": None,
        "base_url_hostname": script(raise_=raiser("ValueError", "bad url"))})
add("is_opencode_responses_backend", params=None, io=OC_OK)
add("is_opencode_responses_backend",
    params={"base_url": "https://opencode.ai"},
    io={**OC_OK, "opencode_provider_family": None, "base_url_hostname": 123})

# ── is_azure_foundry_responses ──────────────────────────────────────────────
AZ_IMP = {"import_base_url_host_matches": None}
add("is_azure_foundry_responses", params={"provider": "azure-foundry"}, io=AZ_IMP)
add("is_azure_foundry_responses", params={"provider": "  Azure-Foundry  "}, io=AZ_IMP)
add("is_azure_foundry_responses",
    params={"provider": "other", "base_url": "https://x.services.ai.azure.com/v1"},
    io={**AZ_IMP, "base_url_host_matches": True})
add("is_azure_foundry_responses",
    params={"provider": "other", "base_url": "https://x.example.com"},
    io={**AZ_IMP, "base_url_host_matches": False})
add("is_azure_foundry_responses",
    params={"base_url": "https://x.services.ai.azure.com"},
    io={**AZ_IMP, "base_url_host_matches": script(raise_=raiser("RuntimeError", "probe down"))})
add("is_azure_foundry_responses", params={"provider": "azure-foundry"},
    io={"import_base_url_host_matches": script(raise_=raiser("ImportError", "no utils"))})
add("is_azure_foundry_responses", params=None, io=AZ_IMP)
add("is_azure_foundry_responses", params={"provider": 0, "base_url": None},
    io={**AZ_IMP, "base_url_host_matches": False})

# ── native_compaction_active ────────────────────────────────────────────────
add("native_compaction_active", context_management=None)
add("native_compaction_active", context_management=[])
add("native_compaction_active", context_management=[{}])
add("native_compaction_active", context_management=[1, 2])
add("native_compaction_active", context_management="x")
add("native_compaction_active", context_management=5)

# ── unit_is_active ──────────────────────────────────────────────────────────
SCOPE = ["systemctl", "--user"]
add("unit_is_active", scope=SCOPE, unit="hermes-gateway.service",
    io={"run": {"stdout": "active", "returncode": 0}})
add("unit_is_active", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "inactive", "returncode": 0}})
add("unit_is_active", scope=SCOPE, unit="u.service", require_rc0=True,
    io={"run": {"stdout": "active", "returncode": 3}})
add("unit_is_active", scope=SCOPE, unit="u.service", require_rc0=False,
    io={"run": {"stdout": "active", "returncode": 3}})
add("unit_is_active", scope=SCOPE, unit="u.service",
    io={"run": script(raise_=raiser("OSError", "no systemctl"))})
add("unit_is_active", scope=SCOPE, unit="u.service",
    io={"run": script(raise_=raiser("TimeoutExpired", "timed out"))})
add("unit_is_active", scope=SCOPE, unit="u.service",
    io={"run": script(raise_=raiser("RuntimeError", "unexpected"))})
add("unit_is_active", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "  active  ", "returncode": 0}})
add("unit_is_active", scope=SCOPE, unit="u.service", io={"run": None})
add("unit_is_active", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": 123, "returncode": 0}})
add("unit_is_active", scope=SCOPE, unit="u.service",
    io={"run": {"returncode": 0}})
add("unit_is_active", scope=SCOPE, unit="u.service", require_rc0=True,
    io={"run": {"stdout": "active"}})
add("unit_is_active", scope=SCOPE, unit="u.service", require_rc0=True,
    io={"run": {"stdout": "active", "returncode": False}})
add("unit_is_active", scope=SCOPE, unit="u.service", io={})  # fail-closed: no script
add("unit_is_active", scope=SCOPE, unit="u.service", io={"run": []})  # exhausted

# ── unit_main_pid ───────────────────────────────────────────────────────────
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "1234", "returncode": 0}})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "0", "returncode": 0}})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "", "returncode": 0}})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "abc", "returncode": 0}})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "1234", "returncode": 1}})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": script(raise_=raiser("OSError", "spawn failed"))})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": " 42 ", "returncode": 0}})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": "1_0", "returncode": 0}})
add("unit_main_pid", scope=SCOPE, unit="u.service",
    io={"run": {"stdout": 123, "returncode": 0}})

# ── serve_unit_replaced ─────────────────────────────────────────────────────
add("serve_unit_replaced", scope=SCOPE, unit="u.service", previous_pid=1234,
    io={"run": [{"stdout": "active", "returncode": 0},
                {"stdout": "4321", "returncode": 0}]})
add("serve_unit_replaced", scope=SCOPE, unit="u.service", previous_pid=1234,
    io={"run": [{"stdout": "inactive", "returncode": 0},
                {"stdout": "active", "returncode": 0},
                {"stdout": "4321", "returncode": 0}],
        "sleep": None})
add("serve_unit_replaced", scope=SCOPE, unit="u.service", previous_pid=0,
    io={"run": [{"stdout": "active", "returncode": 0}, {"stdout": "0", "returncode": 0}] * 10,
        "sleep": None})
add("serve_unit_replaced", scope=SCOPE, unit="u.service", previous_pid=1234,
    io={"run": script(raise_=raiser("RuntimeError", "run broke"))})
add("serve_unit_replaced", scope=SCOPE, unit="u.service", previous_pid=1234,
    io={"run": [{"stdout": "inactive", "returncode": 0}],
        "sleep": script(raise_=raiser("OSError", "sleep broke"))})
add("serve_unit_replaced", scope=SCOPE, unit="u.service", previous_pid=4321,
    io={"run": [{"stdout": "active", "returncode": 0},
                {"stdout": "4321", "returncode": 0},
                {"stdout": "active", "returncode": 0},
                {"stdout": "9999", "returncode": 0}],
        "sleep": None})

# ── systemd_verified_active ─────────────────────────────────────────────────
add("systemd_verified_active", profile="default", io={"shutil.which": None})
add("systemd_verified_active", profile="default",
    io={"shutil.which": "/usr/bin/systemctl",
        "run": [{"stdout": "active", "returncode": 0}]})
add("systemd_verified_active", profile="default",
    io={"shutil.which": "/usr/bin/systemctl",
        "run": [{"stdout": "inactive", "returncode": 0},
                {"stdout": "inactive", "returncode": 0},
                {"stdout": "active", "returncode": 0}]})
add("systemd_verified_active", profile="work",
    io={"shutil.which": "/usr/bin/systemctl",
        "run": [{"stdout": "inactive", "returncode": 0},
                {"stdout": "inactive", "returncode": 0}]})
add("systemd_verified_active", profile="default",
    io={"shutil.which": "/usr/bin/systemctl",
        "run": script(raise_=raiser("RuntimeError", "probe broke"))})
add("systemd_verified_active", profile="default", io={"shutil.which": ""})
add("systemd_verified_active", profile=None,
    io={"shutil.which": "/usr/bin/systemctl",
        "run": [{"stdout": "active", "returncode": 0}]})

# ── n_vocab ─────────────────────────────────────────────────────────────────
add("n_vocab", metadata={"general.architecture": "llama", "llama.vocab_size": 32000})
add("n_vocab", metadata={"general.architecture": "llama", "llama.vocab_size": "128000"})
add("n_vocab", metadata={"general.architecture": "llama", "llama.vocab_size": 0,
                         "tokenizer.ggml.tokens": ["a", "b", "c", "d", "e"]})
add("n_vocab", metadata={"general.architecture": "llama",
                         "tokenizer.ggml.tokens": "not-a-list"})
add("n_vocab", metadata={})
add("n_vocab", metadata={"general.architecture": "llama", "llama.vocab_size": "abc"})
add("n_vocab", metadata={"general.architecture": "llama", "llama.vocab_size": None})
add("n_vocab", metadata=None)
add("n_vocab", metadata={"general.architecture": "llama", "llama.vocab_size": 32000.9})
add("n_vocab", metadata={"general.architecture": 7, "7.vocab_size": 5})

# ── n_head ──────────────────────────────────────────────────────────────────
add("n_head", metadata={"general.architecture": "llama", "llama.attention.head_count": 32})
add("n_head", metadata={"general.architecture": "llama",
                        "llama.attention.head_count": [8, 16, 32]})
add("n_head", metadata={"general.architecture": "llama", "llama.attention.head_count": []})
add("n_head", metadata={"general.architecture": "llama",
                        "llama.attention.head_count": [1, "a"]})
add("n_head", metadata={"general.architecture": "llama"})
add("n_head", metadata={"general.architecture": "llama", "llama.attention.head_count": "16"})
add("n_head", metadata={"general.architecture": "llama",
                        "llama.attention.head_count": [2.9, 1]})

# ── head_dim_k ──────────────────────────────────────────────────────────────
add("head_dim_k", metadata={"general.architecture": "llama", "llama.attention.key_length": 128})
add("head_dim_k", metadata={"general.architecture": "llama",
                            "llama.embedding_length": 4096, "llama.attention.head_count": 32})
add("head_dim_k", metadata={"general.architecture": "llama",
                            "llama.attention.head_count": 0, "llama.embedding_length": "bad"})
add("head_dim_k", metadata={"general.architecture": "llama",
                            "llama.embedding_length": 100, "llama.attention.head_count": 3})
add("head_dim_k", metadata={"general.architecture": "llama",
                            "llama.embedding_length": -100, "llama.attention.head_count": 3})
add("head_dim_k", metadata={"general.architecture": "llama", "llama.attention.key_length": "abc"})
add("head_dim_k", metadata={"general.architecture": "llama", "llama.attention.head_count": "x"})

# ── head_dim_v ──────────────────────────────────────────────────────────────
add("head_dim_v", metadata={"general.architecture": "llama", "llama.attention.value_length": 64})
add("head_dim_v", metadata={"general.architecture": "llama", "llama.attention.key_length": 128})
add("head_dim_v", metadata={"general.architecture": "llama",
                            "llama.embedding_length": 4096, "llama.attention.head_count": 32})
add("head_dim_v", metadata={"general.architecture": "llama",
                            "llama.attention.value_length": 0, "llama.attention.key_length": 96})
add("head_dim_v", metadata={"general.architecture": "llama", "llama.attention.value_length": "bad"})

# ── effective_defer_tools ───────────────────────────────────────────────────
add("effective_defer_tools", defer_tools=None)
add("effective_defer_tools", defer_tools=[])
add("effective_defer_tools", defer_tools=["a", "b"])
add("effective_defer_tools")
add("effective_defer_tools", defer_tools=["x", "x"])

# ── schema_has_external_ref ─────────────────────────────────────────────────
add("schema_has_external_ref", node={"$ref": "https://x/y"})
add("schema_has_external_ref", node={"$ref": "#/defs/a"})
add("schema_has_external_ref", node={"$ref": ""})
add("schema_has_external_ref", node={"properties": {"a": {"$ref": "http://x"}}})
add("schema_has_external_ref", node={"enum": [{"$ref": "http://x"}]})
add("schema_has_external_ref", node={"const": {"$ref": "http://x"},
                                     "default": {"$ref": "y"}})
add("schema_has_external_ref", node=[{"$ref": "ext"}, {}])
add("schema_has_external_ref", node="str")
add("schema_has_external_ref", node=None)
add("schema_has_external_ref", node={"$ref": 123})

# ── stream_is_message ───────────────────────────────────────────────────────
add("stream_is_message", adapter={"probe": "callable"}, chat_id=123,
    io={"probe": True})
add("stream_is_message", adapter={"probe": "callable"}, chat_id="c1",
    io={"probe": 1})
add("stream_is_message", adapter={"probe": "callable"}, chat_id="c1",
    io={"probe": script(raise_=raiser("RuntimeError", "probe failed"))})
add("stream_is_message", adapter={"probe": "absent", "draft_stream_is_message": True})
add("stream_is_message", adapter={"draft_stream_is_message": 1})
add("stream_is_message", adapter={"probe": "noncallable", "draft_stream_is_message": True})
add("stream_is_message", adapter=None)
add("stream_is_message", adapter={"probe": "callable"}, chat_id=None,
    io={"probe": True})

# ── is_flood_error ──────────────────────────────────────────────────────────
add("is_flood_error", result={"error": "Flood wait 30s"})
add("is_flood_error", result={"error": "Retry After 10"})
add("is_flood_error", result={"error": "rate limited"})
add("is_flood_error", result={"error": "network"})
add("is_flood_error", result={"error": None})
add("is_flood_error", result=None)
add("is_flood_error", result={"error": 123})
add("is_flood_error", result={})

# ── _env_cwd ────────────────────────────────────────────────────────────────
add("_env_cwd", io={"import_terminal_env": None, "terminal_env": "/home/u/proj"})
add("_env_cwd", io={"import_terminal_env": script(raise_=raiser("ImportError", "no scope")),
                    "environ_get": "/tmp/x"})
add("_env_cwd", io={"import_terminal_env": script(raise_=raiser("ModuleNotFoundError", "no mod")),
                    "environ_get": ""})
add("_env_cwd", io={"import_terminal_env": script(raise_=raiser("RuntimeError", "boom"))})
add("_env_cwd", io={"import_terminal_env": None,
                    "terminal_env": script(raise_=raiser("ValueError", "bad env"))})

# ── defer_max_age_s ─────────────────────────────────────────────────────────
add("defer_max_age_s", task_cfg=None)
add("defer_max_age_s", task_cfg={})
add("defer_max_age_s", task_cfg={"defer_max_age_s": 60})
add("defer_max_age_s", task_cfg={"defer_max_age_s": "0.5"})
add("defer_max_age_s", task_cfg={"defer_max_age_s": -5})
add("defer_max_age_s", task_cfg={"defer_max_age_s": "abc"})
add("defer_max_age_s", task_cfg={"defer_max_age_s": None})
add("defer_max_age_s", task_cfg={"defer_max_age_s": "nan"})
add("defer_max_age_s", task_cfg="x")
add("defer_max_age_s", task_cfg={"defer_max_age_s": True})

# ── _quiet_for ──────────────────────────────────────────────────────────────
add("_quiet_for", io={"quiet_period": [12.5, 100]})
add("_quiet_for", io={"quiet_period": [0, None]})
add("_quiet_for", io={"quiet_period": []})
add("_quiet_for", io={"quiet_period": None})
add("_quiet_for", io={"quiet_period": 5})
add("_quiet_for", io={"quiet_period": "ab"})
add("_quiet_for", io={"quiet_period": script(raise_=raiser("RuntimeError", "clock broke"))})

# ── envelope_tool_part_cache_markers_supported ─────────────────────────────
add("envelope_tool_part_cache_markers_supported", provider="nous", base_url="https://x",
    io={"import_is_litellm_route": None, "is_litellm_route": False})
add("envelope_tool_part_cache_markers_supported", provider="litellm", base_url="https://x",
    io={"import_is_litellm_route": None, "is_litellm_route": True})
add("envelope_tool_part_cache_markers_supported", provider="  Nous  ", base_url=None,
    io={"import_is_litellm_route": None, "is_litellm_route": False})
add("envelope_tool_part_cache_markers_supported", provider="p", base_url="u",
    io={"import_is_litellm_route": None,
        "is_litellm_route": script(raise_=raiser("RuntimeError", "route broke"))})
add("envelope_tool_part_cache_markers_supported", provider="p", base_url="u",
    io={"import_is_litellm_route": script(raise_=raiser("ImportError", "no helpers"))})
add("envelope_tool_part_cache_markers_supported", provider=None, base_url=None,
    io={"import_is_litellm_route": None, "is_litellm_route": None})

# ── is_qwen_model ───────────────────────────────────────────────────────────
add("is_qwen_model", model="Qwen3-32B")
add("is_qwen_model", model="qwen")
add("is_qwen_model", model="gpt-5")
add("is_qwen_model", model=None)
add("is_qwen_model", model="")
add("is_qwen_model", model=123)
add("is_qwen_model", model=0)

# ── is_container_backend ────────────────────────────────────────────────────
add("is_container_backend", env_type="docker")
add("is_container_backend", env_type="modal")
add("is_container_backend", env_type="daytona")
add("is_container_backend", env_type="vercel_sandbox")
add("is_container_backend", env_type="local", io={"plugin_env_flag": True})
add("is_container_backend", env_type="local", io={"plugin_env_flag": False})
add("is_container_backend", env_type="local", io={"plugin_env_flag": "yes"})
add("is_container_backend", env_type=[])
add("is_container_backend", env_type="local",
    io={"plugin_env_flag": script(raise_=raiser("RuntimeError", "registry broke"))})
add("is_container_backend", env_type=None, io={"plugin_env_flag": False})

# ── in_delegated_child_context ──────────────────────────────────────────────
add("in_delegated_child_context",
    io={"import_is_delegated_child_context": None, "is_delegated_child_context": True})
add("in_delegated_child_context",
    io={"import_is_delegated_child_context": None, "is_delegated_child_context": False})
add("in_delegated_child_context",
    io={"import_is_delegated_child_context": None,
        "is_delegated_child_context": script(raise_=raiser("RuntimeError", "ctx broke"))})
add("in_delegated_child_context",
    io={"import_is_delegated_child_context": script(raise_=raiser("ImportError", "no ctx"))})
add("in_delegated_child_context",
    io={"import_is_delegated_child_context": None, "is_delegated_child_context": None})

# ── composite_percent ───────────────────────────────────────────────────────
add("composite_percent", stages=None, current="x", value=0.5)
add("composite_percent", stages=["download"], current="download", value=0.5)
add("composite_percent", stages=["download", "text_model"], current="download", value=0.5)
add("composite_percent", stages=["download", "text_model"], current="text_model", value=0.5)
add("composite_percent", stages=["a", "b", "text_model"], current="b", value=1.0)
add("composite_percent", stages=["a", "b"], current="missing", value=0.42)
add("composite_percent", stages=["a", "b"], current="a", value=None)
add("composite_percent", stages=5, current="a", value=0.5)
add("composite_percent", stages=["text_model", "text_model"], current="text_model", value=1.0)
add("composite_percent", stages=["a", "b"], current="a", value=1.5)
add("composite_percent", stages=None, current="a", value=1.5)
add("composite_percent", stages=["a", "b", "c"], current="c", value=0.5)

# ── observed_lifetime_seconds ───────────────────────────────────────────────
add("observed_lifetime_seconds",
    io={"get_provider_auth_state": {"expires_in": 3600, "agent_key_expires_in": 7200}})
add("observed_lifetime_seconds", io={"get_provider_auth_state": {"expires_in": "1800.9"}})
add("observed_lifetime_seconds",
    io={"get_provider_auth_state": {"expires_in": 0, "agent_key_expires_in": -5}})
add("observed_lifetime_seconds", io={"get_provider_auth_state": None})
add("observed_lifetime_seconds",
    io={"get_provider_auth_state": {"expires_in": "abc", "agent_key_expires_in": "60"}})
add("observed_lifetime_seconds", io={"get_provider_auth_state": {"expires_in": "nan"}})
add("observed_lifetime_seconds", io={"get_provider_auth_state": {"expires_in": "inf"}})
add("observed_lifetime_seconds",
    io={"get_provider_auth_state": script(raise_=raiser("RuntimeError", "auth broke"))})
add("observed_lifetime_seconds", io={"get_provider_auth_state": "x"})

# ── is_official_codex_base_url ──────────────────────────────────────────────
for u in ("https://chatgpt.com/backend-api/codex",
          "https://chatgpt.com:443/backend-api/codex/",
          "https://chatgpt.com:/backend-api/codex",
          "https://chatgpt.com:8443/backend-api/codex",
          "https://chatgpt.com:abc/backend-api/codex",
          "https://chatgpt.com:99999/backend-api/codex",
          "http://chatgpt.com/backend-api/codex",
          "https://chatgpt.com.evil.com/backend-api/codex",
          "https://CHATGPT.COM/backend-api/codex?x=1",
          "https://user:pass@chatgpt.com/backend-api/codex",
          "https://chatgpt.com/backend-api/codexevil",
          "https://chatgpt.com/backend-api/codex///",
          "https://chatgpt.com:0443/backend-api/codex",
          "https://chatgpt.com/backend-api/codex/sub/path",
          "//chatgpt.com/backend-api/codex"):
    add("is_official_codex_base_url", base_url=u)
add("is_official_codex_base_url", base_url=None)
add("is_official_codex_base_url", base_url=123)

# ── connection_is_open ──────────────────────────────────────────────────────
LIVE_SELF = {"_state": "running", "_proc": {"returncode": None, "stdin": {}},
             "_reader_task": {}}
add("connection_is_open", self=LIVE_SELF,
    io={"stdin_is_closing": False, "reader_done": False})
add("connection_is_open", self={**LIVE_SELF, "_state": "closed"})
add("connection_is_open",
    self={**LIVE_SELF, "_proc": {"returncode": 0, "stdin": {}}})
add("connection_is_open",
    self={**LIVE_SELF, "_proc": {"returncode": None, "stdin": None}})
add("connection_is_open", self=LIVE_SELF,
    io={"stdin_is_closing": True})
add("connection_is_open", self={**LIVE_SELF, "_reader_task": None},
    io={"stdin_is_closing": False})
add("connection_is_open", self=LIVE_SELF,
    io={"stdin_is_closing": False, "reader_done": True})
add("connection_is_open", self=LIVE_SELF,
    io={"stdin_is_closing": script(raise_=raiser("RuntimeError", "pipe broke"))})
add("connection_is_open", self={**LIVE_SELF, "_state": []})
add("connection_is_open", self={"_state": "starting", "_proc": None})

# ── detect_upstream_elision ─────────────────────────────────────────────────
PAD = "x" * 1200
add("detect_upstream_elision", content=PAD + "... 12 more items")
add("detect_upstream_elision", content="... 5 more items")
add("detect_upstream_elision", content=PAD + '"has_more" : true')
add("detect_upstream_elision", content=PAD + "Saved To Sandbox")
add("detect_upstream_elision", content=PAD + "DATA_PREVIEW")
add("detect_upstream_elision", content="x" * 70000 + "... 3 more items")
add("detect_upstream_elision", content="x" * 65530 + "... 3 more items")
add("detect_upstream_elision", content=5)
add("detect_upstream_elision", content=PAD + "... 12more items")
add("detect_upstream_elision", content=PAD + "... 7 more item")
add("detect_upstream_elision", content=PAD + '"HAS_MORE":\tTrue')

# ── selection_attempt_tokens ────────────────────────────────────────────────
add("selection_attempt_tokens", text="2")
add("selection_attempt_tokens", text="-3")
add("selection_attempt_tokens", text="+4")
add("selection_attempt_tokens", text="1 3")
add("selection_attempt_tokens", text="1, 2, 3")
add("selection_attempt_tokens", text="yes, no", choices=["yes please", "no thanks"])
add("selection_attempt_tokens", text="alpha beta, gamma", choices=None)
add("selection_attempt_tokens", text="hello world")
add("selection_attempt_tokens", text="")
add("selection_attempt_tokens", text="   ")
add("selection_attempt_tokens", text=None)
add("selection_attempt_tokens", text=123)
add("selection_attempt_tokens", text="1_0")
add("selection_attempt_tokens", text=",")
add("selection_attempt_tokens", text="2, two", choices=["one", "two three"])

# ── _is_importlib ───────────────────────────────────────────────────────────
add("_is_importlib", name="importlib")
add("_is_importlib", name="importlib.util")
add("_is_importlib", name="importlibx")
add("_is_importlib", name="os")
add("_is_importlib", name=None)
add("_is_importlib", name=5)

# ── _human_wait_seconds ─────────────────────────────────────────────────────
add("_human_wait_seconds", self={"_session_key": "s1"},
    io={"import_human_wait_seconds": None, "human_wait_seconds": 12.5})
add("_human_wait_seconds", self={"_session_key": "s1"},
    io={"import_human_wait_seconds": None,
        "human_wait_seconds": script(raise_=raiser("RuntimeError", "wait broke"))})
add("_human_wait_seconds", self={"_session_key": "s1"},
    io={"import_human_wait_seconds": script(raise_=raiser("ImportError", "no module"))})
add("_human_wait_seconds", self=None,
    io={"import_human_wait_seconds": None})
add("_human_wait_seconds", self={},
    io={"import_human_wait_seconds": None})
add("_human_wait_seconds", self={"_session_key": "s1"},
    io={"import_human_wait_seconds": None, "human_wait_seconds": None})

# ── profile_rejects_tool_media ──────────────────────────────────────────────
PR_IMP = "import_routed_model_rejects_vision_tool_messages"
PR_CALL = "routed_model_rejects_vision_tool_messages"
add("profile_rejects_tool_media", provider="xiaomi", model="mimo",
    io={PR_IMP: None, PR_CALL: True})
add("profile_rejects_tool_media", provider="openai", model="gpt-5",
    io={PR_IMP: None, PR_CALL: False})
add("profile_rejects_tool_media", provider="p", model="m",
    io={PR_IMP: None, PR_CALL: script(raise_=raiser("ValueError", "profile broke"))})
add("profile_rejects_tool_media", provider="p", model="m",
    io={PR_IMP: script(raise_=raiser("ImportError", "no providers"))})
add("profile_rejects_tool_media", provider="p",
    io={PR_IMP: None, PR_CALL: False})
add("profile_rejects_tool_media", provider="p", model=None,
    io={PR_IMP: None, PR_CALL: False})

# ── exceeds_size_cap ────────────────────────────────────────────────────────
add("exceeds_size_cap", max_file_size_mb=1, path="f", io={"stat": {"st_size": 2000000}})
add("exceeds_size_cap", max_file_size_mb=1, path="f", io={"stat": {"st_size": 1048576}})
add("exceeds_size_cap", max_file_size_mb=0, path="f")
add("exceeds_size_cap", max_file_size_mb=-1, path="f")
add("exceeds_size_cap", max_file_size_mb=1, path="f",
    io={"stat": script(raise_=raiser("FileNotFoundError", "gone"))})
add("exceeds_size_cap", max_file_size_mb=1, path="f",
    io={"stat": script(raise_=raiser("RuntimeError", "stat broke"))})
add("exceeds_size_cap", max_file_size_mb=None, path="f")
add("exceeds_size_cap", max_file_size_mb=1, path=None)
add("exceeds_size_cap", max_file_size_mb=1, path="f", io={"stat": {"st_size": None}})
add("exceeds_size_cap", max_file_size_mb=0.5, path="f", io={"stat": {"st_size": 524289}})

# ── serve_unit_recovery_available ───────────────────────────────────────────
add("serve_unit_recovery_available",
    io={"sys_platform": "linux", "shutil.which": "/usr/bin/systemctl"})
add("serve_unit_recovery_available", io={"sys_platform": "win32"})
add("serve_unit_recovery_available", io={"sys_platform": "linux", "shutil.which": None})
add("serve_unit_recovery_available", io={"sys_platform": "linux", "shutil.which": ""})
add("serve_unit_recovery_available",
    io={"sys_platform": script(raise_=raiser("RuntimeError", "platform broke"))})
add("serve_unit_recovery_available", io={"sys_platform": "darwin"})


def main():
    out = HERE / "fixtures" / "tail_revive4.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases for {len(counts)} funcs -> {out}")
    if thin:
        print("THIN (<5):", thin)
        raise SystemExit(1)


if __name__ == "__main__":
    main()