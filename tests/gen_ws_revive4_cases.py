#!/usr/bin/env python3
"""gen_ws_revive4_cases.py — regenerate tests/fixtures/ws_revive4.jsonl.

Case design for the ws group-4 revival differential oracle. Every ported
function gets >=5 cases: happy paths, falsy/short-circuit edges, scripted
plain raises (swallowed where the live code has except Exception),
BaseException propagation (SystemExit/KeyboardInterrupt), fail-closed
missing/exhausted scripts, attribute-missing errors, and state mutation.

Script semantics (shared with the harness dispatcher and oracle Script):
- bare value: reused verbatim; null -> None
- bare LIST: per-call queue (wrap list RETURNS as {"__script__": true,
  "ret": [...]} or [[...]])
- {"__script__": true, "raise": {"type", "message"}}: scripted exception
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script_ret(v):
    return {"__script__": True, "ret": v}


def script_raise(ty, msg=""):
    return {"__script__": True, "raise": {"type": ty, "message": msg}}


# ── ws_u_normalize_memory_provider_name ─────────────────────────────────────
F = "ws_u_normalize_memory_provider_name"
add(F, name="mem0", io={"is_core_memory_provider": False})
add(F, name="builtin", io={"is_core_memory_provider": True})
add(F, name="  spaced  ", io={"is_core_memory_provider": False})
add(F, name=None, io={"is_core_memory_provider": False})
add(F, name="", io={"is_core_memory_provider": False})
add(F, name="x", io={"is_core_memory_provider": script_raise("ValueError", "bad")})
add(F, name="x", io={"is_core_memory_provider": script_raise("SystemExit", "bye")})
add(F, name="x", io={})
add(F, name="\u00a0mem0\u2003", io={"is_core_memory_provider": False})
add(F, name="x", io={"is_core_memory_provider": 1})
add(F, io={"is_core_memory_provider": False})  # absent name -> ""

# ── ws_u_env_lookup ─────────────────────────────────────────────────────────
F = "ws_u_env_lookup"
add(F, env_key="A", io={"load_env": {"A": "va"}})
add(F, env_key="A", env={"A": "ve"}, io={"load_env": {"A": ""}})
add(F, env_key="A", env={}, io={"load_env": {}})
add(F, env_key=None, io={})
add(F, env_key="A", env={"A": "ve"}, io={"load_env": {"A": 0}})
add(F, env_key="A", env={"A": "ve"}, io={"load_env": {"A": "va"}})
add(F, env_key=5, io={"load_env": {}})
add(F, env_key="A", io={"load_env": None})
add(F, env_key="A", io={"load_env": script_raise("ValueError", "env")})
add(F, env_key="A", io={"load_env": {"A": 7}})
add(F, env_key="", io={})

# ── ws_u_field_default ──────────────────────────────────────────────────────
F = "ws_u_field_default"
add(F, field={"kind": "boolean", "default": "yes"})
add(F, field={"kind": "boolean", "default": "OFF"})
add(F, field={"kind": "boolean"})
add(F, field={"kind": "boolean", "default": None})
add(F, field={"kind": "boolean", "default": 0})
add(F, field={"kind": "boolean", "default": 3})
add(F, field={"kind": "boolean", "default": True})
add(F, field={"kind": "boolean", "default": "maybe"})
add(F, field={"kind": "text", "default": "x"})
add(F, field={"default": "x"})
add(F, field=None)
add(F, field={"kind": "boolean", "default": "  ON  "})
add(F, field={"kind": "boolean", "default": "0"})
add(F, field={"kind": "text", "default": {"a": 1}})

# ── ws_u_load_memory_provider ───────────────────────────────────────────────
F = "ws_u_load_memory_provider"
add(F, name="mem0", io={"load_memory_provider": {"ok": 1}})
add(F, name="mem0", register_skills=True, io={"load_memory_provider": None})
add(F, name="mem0", io={"load_memory_provider": script_raise("ValueError", "nope")})
add(F, name="mem0", io={"load_memory_provider": script_raise("SystemExit", "bye")})
add(F, name="mem0", io={})
add(F, name="mem0", register_skills=False, io={"load_memory_provider": {"x": 2}})
add(F, name="mem0", io={"load_memory_provider": [{"a": 1}]})

# ── ws_u_memory_provider_manifest ───────────────────────────────────────────
F = "ws_u_memory_provider_manifest"
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_ret([{"a": 1}, []])})
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_ret("ab")})
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_ret({"k1": 1, "k2": 2})})
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_ret([1])})
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_ret([1, 2, 3])})
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_ret("abc")})
add(F, name="mem0", io={"memory_provider_dependency_inputs": None})
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_raise("ValueError", "x")})
add(F, name="mem0", io={"memory_provider_dependency_inputs": script_raise("SystemExit", "bye")})

# ── ws_u_trim_setup_output ──────────────────────────────────────────────────
F = "ws_u_trim_setup_output"
add(F, value="short")
add(F, value=None)
add(F, value=0)
add(F, value="xxxxxxxxxx", limit=5)
add(F, value="héllo", limit=4)
add(F, value="abc", limit=0)
add(F, value="abc", limit=-1)
add(F, value="  pad  ")
add(F, value="y" * 4001)
add(F, value="abc", limit=3)
add(F, value=False)
add(F)  # absent value -> ""

# ── ws_u_require_valid_memory_provider_name ─────────────────────────────────
F = "ws_u_require_valid_memory_provider_name"
add(F, name="mem0")
add(F, name="a" * 64)
add(F, name="a" * 65)
add(F, name="")
add(F, name=None)
add(F, name="_bad")
add(F, name="a b")
add(F, name="é")
add(F, name=5)
add(F, name="abc\n")
add(F, name="a-b_C9")
add(F)  # absent name -> HTTPException "... None"

# ── ws_u_write_provider_flat ────────────────────────────────────────────────
F = "ws_u_write_provider_flat"
FLAT_IO = {
    "read_flat_json": {"a": "1"},
    "save_submitted_secrets": None,
    "apply_field_values": None,
    "flat_json_path": "/h/p/config.json",
    "write_json_0600": None,
}
add(F, provider="p", values={"k": "v"}, io=dict(FLAT_IO))
add(F, provider="p", values={"k": "v"},
    io={**FLAT_IO, "read_flat_json": script_raise("ValueError", "read")})
add(F, provider="p", values={"k": "v"},
    io={**FLAT_IO, "save_submitted_secrets": script_raise("ValueError", "save")})
add(F, provider="p", values={"k": "v"},
    io={**FLAT_IO, "apply_field_values": script_raise("ValueError", "apply")})
add(F, provider="p", values={"k": "v"},
    io={**FLAT_IO, "flat_json_path": script_raise("ValueError", "path")})
add(F, provider="p", values={"k": "v"},
    io={**FLAT_IO, "write_json_0600": script_raise("SystemExit", "bye")})
add(F, provider="p", values={"k": "v"}, io={**FLAT_IO, "read_flat_json": None})
add(F, provider="p", values=None, io=dict(FLAT_IO))

# ── ws_u_flat_json_path ─────────────────────────────────────────────────────
F = "ws_u_flat_json_path"
add(F, provider={"name": "mem0"}, io={"get_hermes_home": "/h"})
add(F, provider={"name": "mem0"}, io={"get_hermes_home": ""})
add(F, provider={"name": "mem0"}, io={"get_hermes_home": "/"})
add(F, provider={"name": "a/b"}, io={"get_hermes_home": "/h/"})
add(F, provider={"name": ".."}, io={"get_hermes_home": "/h"})
add(F, provider={"name": "/abs"}, io={"get_hermes_home": "/h"})
add(F, provider={}, io={"get_hermes_home": "/h"})
add(F, provider={"name": 5}, io={"get_hermes_home": "/h"})
add(F, provider={"name": "m"}, io={"get_hermes_home": script_raise("ValueError", "home")})
add(F, provider={"name": "m"}, io={"get_hermes_home": "//h"})
add(F, provider={"name": "."}, io={"get_hermes_home": "/h"})

# ── ws_u_custom_endpoint_id ─────────────────────────────────────────────────
F = "ws_u_custom_endpoint_id"
add(F, raw="My Endpoint!")
add(F, raw="  --__  ")
add(F, raw=None)
add(F, raw="a__b--c")
add(F, raw="ABC")
add(F, raw="!!!", fallback="fb")
add(F, raw="é")
add(F, raw="a b!!c")
add(F, raw=0)
add(F, raw=" -x- ")

# ── ws_u_config_api_key_is_env_ref ──────────────────────────────────────────
F = "ws_u_config_api_key_is_env_ref"
add(F, endpoint_id="e1", io={"_raw_provider_api_key": "${MY_KEY}"})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": "literal"})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": "${}"})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": "x${a}y"})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": "${a${b}"})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": None})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": 5})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": script_raise("ValueError", "k")})
add(F, endpoint_id="e1", io={"_raw_provider_api_key": "${a}\n${b}"})

# ── ws_u_pool_entry_summary ─────────────────────────────────────────────────
F = "ws_u_pool_entry_summary"
ENTRY = {"access_token": "sk-abcdef123456", "id": "c1", "label": "L",
         "auth_type": "oauth", "source": "pool", "priority": 1,
         "last_status": "ok", "request_count": 3, "refresh_token": "r"}
add(F, entry=dict(ENTRY), index=2, io={"redact_key": "sk-abc…3456"})
add(F, entry={**ENTRY, "access_token": ""}, index=1, io={})
add(F, entry={**ENTRY, "access_token": None}, index=1, io={})
add(F, entry={**ENTRY, "refresh_token": None}, index=1, io={"redact_key": "masked"})
add(F, entry={k: v for k, v in ENTRY.items() if k != "refresh_token"},
    index=1, io={"redact_key": "masked"})
add(F, entry={k: v for k, v in ENTRY.items() if k != "id"}, index=1, io={})
add(F, entry=dict(ENTRY), index=1, io={"redact_key": script_raise("ValueError", "r")})
add(F, entry={**ENTRY, "access_token": 5}, index=1, io={"redact_key": "masked"})

# ── ws_u_strip_session_list_rows ────────────────────────────────────────────
F = "ws_u_strip_session_list_rows"
add(F, sessions=[{"id": "s1", "system_prompt": "sp", "model_config": {"m": 1}, "x": 2},
                 {"id": "s2"}])
add(F, sessions=[])
add(F, sessions=[{"id": "s1"}, 5])
add(F, sessions=None)
add(F, sessions=[{"system_prompt": "a", "model_config": "b"}])
add(F, sessions=[{"id": "s3", "model_config": None}])

# ── ws_u_strip_dashboard_manifest ───────────────────────────────────────────
F = "ws_u_strip_dashboard_manifest"
add(F, p={"_x": 1, "a": 2})
add(F, p={})
add(F, p={"_a": 1, "_b": 2})
add(F, p={"b": 1, "_a": 2, "c": 3})
add(F, p=None)
add(F, p={"a": {"_z": 1}, "_b": 2})

# ── ws_u_managed_response_meta ──────────────────────────────────────────────
F = "ws_u_managed_response_meta"
add(F, policy={"locked_root": "/x", "can_change_path": True})
add(F, policy={"locked_root": None, "can_change_path": False})
add(F, policy={"can_change_path": True})
add(F, policy={"locked_root": "/x"})
add(F, policy={"locked_root": 5, "can_change_path": True})
add(F, policy={"locked_root": "/y", "can_change_path": False})

# ── ws_u_skill_meta_to_payload ──────────────────────────────────────────────
F = "ws_u_skill_meta_to_payload"
M = {"name": "n", "description": "d", "source": "hub", "identifier": "i",
     "trust_level": "trusted", "repo": "r", "tags": ["a", "b"]}
add(F, m=dict(M))
add(F, m={**M, "tags": None})
add(F, m={**M, "tags": ""})
add(F, m={**M, "tags": "ab"})
add(F, m={**M, "tags": {"k1": 1, "k2": 2}})
add(F, m={**M, "tags": 5})
add(F, m={k: v for k, v in M.items() if k != "name"})
add(F, m={**M, "tags": []})
add(F, m={**M, "tags": "héllo"})

# ── ws_u_clear_skills_prompt_cache ──────────────────────────────────────────
F = "ws_u_clear_skills_prompt_cache"
add(F, io={"clear_skills_system_prompt_cache": None})
add(F, io={"clear_skills_system_prompt_cache": script_raise("ValueError", "c")})
add(F, io={"clear_skills_system_prompt_cache": script_raise("SystemExit", "bye")})
add(F, io={"clear_skills_system_prompt_cache": script_raise("KeyboardInterrupt", "")})
add(F, io={})

# ── ws_u_aux_usage_rows ─────────────────────────────────────────────────────
F = "ws_u_aux_usage_rows"
ROWS = [{"model": "m", "task": "t", "tokens": 5}]
add(F, cutoff=100, io={"auxiliary_usage_by_model_task": script_ret(ROWS)})
add(F, cutoff=100, io={"auxiliary_usage_by_model_task": script_raise("ValueError", "db")})
add(F, cutoff=100, io={"auxiliary_usage_by_model_task": script_raise("SystemExit", "bye")})
add(F, cutoff=100, io={"auxiliary_usage_by_model_task": None})
add(F, cutoff=100, io={})
add(F, cutoff=123.5, io={"auxiliary_usage_by_model_task": script_ret([])})

# ── ws_u_write_platform_enabled ─────────────────────────────────────────────
F = "ws_u_write_platform_enabled"
add(F, platform_id="telegram", enabled=True, io={"write_platform_config_field": None})
add(F, platform_id="telegram", enabled=False, io={"write_platform_config_field": None})
add(F, platform_id="telegram", enabled=True,
    io={"write_platform_config_field": script_raise("ValueError", "w")})
add(F, platform_id="telegram", enabled=True,
    io={"write_platform_config_field": script_raise("SystemExit", "bye")})
add(F, platform_id="telegram", io={"write_platform_config_field": None})
add(F, enabled=True, io={"write_platform_config_field": None})

# ── ws_u_whatsapp_session_path ──────────────────────────────────────────────
F = "ws_u_whatsapp_session_path"
add(F, io={"get_hermes_dir": "/h/platforms/whatsapp/session"})
add(F, io={"get_hermes_dir": "/h/whatsapp/session"})
add(F, io={"get_hermes_dir": script_raise("ValueError", "d")})
add(F, io={"get_hermes_dir": script_raise("SystemExit", "bye")})
add(F, io={})

# ── ws_u_whatsapp_onboarding_payload ────────────────────────────────────────
F = "ws_u_whatsapp_onboarding_payload"
REC = {"status": "pending", "qr_payload": "qr", "expires_at": 123, "mode": "pair",
       "allowed_users": ["u1"], "account_id": None, "account_name": None,
       "account_phone": None, "error": None}
add(F, pairing_id="p1", record=dict(REC))
add(F, pairing_id="p1", record={k: v for k, v in REC.items() if k != "status"})
add(F, pairing_id="p1", record={k: v for k, v in REC.items() if k != "error"})
add(F, pairing_id=None, record=dict(REC))
add(F, pairing_id="p2", record={})
add(F, pairing_id="p3", record={**REC, "expires_at": 999, "allowed_users": []})

# ── ws_u_telegram_onboarding_base_url ───────────────────────────────────────
F = "ws_u_telegram_onboarding_base_url"
add(F, env={})
add(F, env={"TELEGRAM_ONBOARDING_URL": "https://x/"})
add(F, env={"TELEGRAM_ONBOARDING_URL": "  https://x//  "})
add(F, env={"TELEGRAM_ONBOARDING_URL": ""})
add(F, env={"TELEGRAM_ONBOARDING_URL": None})
add(F, env={"TELEGRAM_ONBOARDING_URL": "/"})
add(F, env={"TELEGRAM_ONBOARDING_URL": "\u00a0https://x\u2003"})
add(F, env={"TELEGRAM_ONBOARDING_URL": "https://x///"})

# ── ws_u_profile_setup_command ──────────────────────────────────────────────
F = "ws_u_profile_setup_command"
add(F, name="default", io={"_resolve_profile_dir": None})
add(F, name="work", io={"_resolve_profile_dir": None})
add(F, name="work", io={"_resolve_profile_dir": script_raise("HTTPException", "404: unknown profile")})
add(F, name="work", io={"_resolve_profile_dir": script_raise("SystemExit", "bye")})
add(F, name="work", io={})
add(F, name="", io={"_resolve_profile_dir": None})
add(F, name=None, io={"_resolve_profile_dir": None})
add(F, io={"_resolve_profile_dir": None})  # absent name -> "None setup"

# ── ws_u_validate_oauth_profile ─────────────────────────────────────────────
F = "ws_u_validate_oauth_profile"
add(F, profile="work", io={"_resolve_profile_dir": None})
add(F, profile=None, io={})
add(F, profile="", io={})
add(F, profile="   ", io={})
add(F, profile="current", io={})
add(F, profile="CURRENT", io={})
add(F, profile=" Current ", io={})
add(F, profile="work", io={"_resolve_profile_dir": script_raise("HTTPException", "404: no")})
add(F, profile=5, io={})
add(F, profile="x", io={})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert len(counts) == 24, counts
    for f, n in counts.items():
        assert n >= 5, (f, n)
    out = HERE / "fixtures" / "ws_revive4.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {out}: {len(CASES)} cases across {len(counts)} funcs")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()
