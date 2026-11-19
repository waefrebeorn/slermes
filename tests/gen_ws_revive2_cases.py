#!/usr/bin/env python3
"""gen_ws_revive2_cases.py — fixture generator for the ws_revive2 oracle.

Writes tests/fixtures/ws_revive2.jsonl: {"func", "in"} lines, >=5 cases per
ported function. Scripted seam results follow the universal-seam contract:
plain value = reused, bare array = per-call queue, {"__script__": true,
"ret": ...} = a single result (REQUIRED for list/dict-returning seams whose
value would otherwise read as a queue), {"__script__": true, "raise":
{type, message[, status_code]}} = a scripted exception.
"""
import json
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "tests" / "fixtures" / "ws_revive2.jsonl"

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_(ty, msg, **extra):
    return {"__script__": True, "raise": {"type": ty, "message": msg, **extra}}


F = "ws_u_validate_reference_timeout"
add(F, value=None)
add(F, value="")
add(F, value=5)
add(F, value="2.5")
add(F, value="  +1_2.5e1  ")
add(F, value=True)
add(F, value=False)
add(F, value="nan")
add(F, value="-Infinity")
add(F, value=0)
add(F, value=-3)
add(F, value="abc")
add(F, value=[])
add(F, value={})
add(F, value="1_")
add(F, value=".5")
add(F, value="1e999")
add(F, value="1.")

F = "ws_u_is_command_provider_block"
add(F, value={"command": "say hi"})
add(F, value={"type": "command", "command": "x"})
add(F, value={"type": "  COMMAND  ", "command": "x"})
add(F, value={"type": "openai", "command": "x"})
add(F, value={"command": "   "})
add(F, value={"command": 5})
add(F, value={})
add(F, value="x")
add(F, value=None)
add(F, value={"type": None, "command": "x"})
add(F, value={"type": "", "command": "x"})
add(F, value={"type": {"a": 1}, "command": "x"})
add(F, value={"command": "\u00a0x\u00a0"})
add(F, value={"type": ["command"], "command": "x"})

F = "ws_u_models_from_custom_endpoint_entry"
add(F, entry={"models": ["a", "b"], "model": "c"})
add(F, entry={"models": {"m1": 1, "m2": 2}})
add(F, entry={"models": ["a", "a", "", " b "], "default_model": "a"})
add(F, entry={})
add(F, entry={"model": 0, "default_model": "d"})
add(F, entry={"models": ["x"], "model": " x "})
add(F, entry={"models": [1, 2.5, None, True]})
add(F, entry={"models": "notlist"})
add(F, entry={"model": "  "})
add(F, entry={"models": {"k": 1}, "model": "k"})

F = "ws_u_webhook_route_summary"
add(F, name="gh", base_url="https://x.test",
    route={"description": "d", "events": ["push"], "deliver": "agent",
           "deliver_only": 1, "prompt": "p", "script": "s", "skills": ["a"],
           "created_at": "2026-01-01", "signature": "sig", "secret": "s3cr3t",
           "enabled": True})
add(F, name="empty", base_url="http://h", route={})
add(F, name="off", base_url="", route={"enabled": False})
add(F, name="zero", base_url="", route={"enabled": 0})
add(F, name="dictev", base_url="b", route={"events": {"a": 1, "b": 2}})
add(F, name="strev", base_url="b", route={"events": "ab"})
add(F, name="bad", base_url="b", route={"events": 5})
add(F, name="nullsk", base_url="b", route={"skills": None, "created_at": None})
add(F, name="num", base_url=7, route={"deliver": None, "prompt": None})
add(F, name="x", base_url="b", route={"enabled": "false", "secret": ""})

F = "ws_u_field_value"
add(F, field={"key": "k", "kind": "secret"}, data={"k": "v"}, io={})
add(F, field={"key": "k", "kind": "text"}, data={"k": "hello"}, io={})
add(F, field={"key": "k", "kind": "text", "_env_key": "EK"}, data={},
    io={"env_lookup": "envval"})
add(F, field={"key": "k", "kind": "text", "_env_key": "EK", "default": "dv"},
    data={"k": ""}, io={"env_lookup": ""})
add(F, field={"key": "k", "kind": "select",
              "options": [{"value": "a"}, {"value": "b"}], "default": "a"},
    data={"k": "b"}, io={})
add(F, field={"key": "k", "kind": "select",
              "options": [{"value": "a"}], "default": "a"},
    data={"k": "zzz"}, io={})
add(F, field={"key": "k", "kind": "boolean"}, data={"k": "yes"}, io={})
add(F, field={"key": "k", "kind": "boolean"}, data={"k": "maybe"}, io={})
add(F, field={"key": "k", "kind": "boolean", "default": "on"}, data={},
    io={"env_lookup": ""})
add(F, field={"key": "k", "kind": "text", "_env_key": "EK"}, data={},
    io={"env_lookup": raise_("ValueError", "env boom")})
add(F, field={"key": "k", "kind": "text"}, data={"k": 5}, io={})
add(F, field={"key": "k", "kind": "text", "default": 7}, data={"k": None},
    io={"env_lookup": None})
add(F, field={"key": "k", "kind": "select", "options": [{"value": 1}],
              "default": 1}, data={"k": 1}, io={})

F = "ws_u_field_visible"
add(F, field={"key": "k", "kind": "text"}, data={}, io={})
add(F, field={"key": "k", "kind": "text", "when": {}}, data={}, io={})
add(F, field={"key": "k", "kind": "text", "when": {"mode": "advanced"}},
    data={"mode": "advanced"}, io={})
add(F, field={"key": "k", "kind": "text", "when": {"mode": "advanced"}},
    data={"mode": "basic"}, io={})
add(F, field={"key": "k", "kind": "text", "when": {"dep": "x"}},
    data={}, fields_by_key={"dep": {"key": "dep", "kind": "text", "default": "x"}},
    io={})
add(F, field={"key": "k", "kind": "text", "when": {"flag": True}},
    data={"flag": "yes"}, io={})
add(F, field={"key": "k", "kind": "text", "when": {"dep": "x"}},
    data={}, fields_by_key={}, io={"env_lookup": ""})
add(F, field={"key": "k", "kind": "text", "when": "notdict"}, data={}, io={})
add(F, field={"key": "k", "kind": "text", "when": {"dep": ""}},
    data={}, io={"env_lookup": ""})
add(F, field={"key": "k", "kind": "text", "when": {"a": "1", "b": "2"}},
    data={"a": "1", "b": "nope"}, io={})

F = "ws_u_coerce_schema_field"
add(F, field={"key": "k", "kind": "boolean"}, raw="yes")
add(F, field={"key": "k", "kind": "boolean", "default": "on"}, raw=None)
add(F, field={"key": "k", "kind": "integer"}, raw="5")
add(F, field={"key": "k", "kind": "number"}, raw="2.5")
add(F, field={"key": "k", "kind": "integer"}, raw="2.5")
add(F, field={"key": "k", "kind": "integer", "minimum": 10}, raw=5)
add(F, field={"key": "k", "kind": "number", "maximum": 2}, raw="2.5")
add(F, field={"key": "k", "kind": "text", "default": "dv"}, raw=None)
add(F, field={"key": "k", "kind": "text"}, raw="  hi  ")
add(F, field={"key": "k", "kind": "select", "options": [{"value": "a"}]}, raw="a")
add(F, field={"key": "k", "kind": "select", "options": [{"value": "a"}]}, raw="b")
add(F, field={"key": "k", "kind": "select", "options": [{"value": "a"}],
              "default": "a"}, raw="")
add(F, field={"key": "k", "kind": "integer"}, raw=True)
add(F, field={"key": "k", "kind": "number", "minimum": "x"}, raw=1)
add(F, field={"key": "k", "kind": "integer", "default": "3"}, raw="")
add(F, field={"key": "k", "kind": "number"}, raw="nan")

F = "ws_u_aux_task_summary"
add(F, aux_rows=[])
add(F, aux_rows=[{"task": "t1", "input_tokens": 5, "output_tokens": 2,
                  "estimated_cost": 0.1, "api_calls": 1, "model": "m1"},
                 {"task": "t1", "input_tokens": 3, "output_tokens": 1,
                  "estimated_cost": 0.2, "api_calls": 2, "model": "m1"},
                 {"task": "t2", "input_tokens": 100, "output_tokens": 0,
                  "estimated_cost": 0, "api_calls": 1, "model": "m2"}])
add(F, aux_rows=[{"task": "a", "input_tokens": 1, "model": "m"},
                 {"task": "b", "input_tokens": 1, "model": "m"}])
add(F, aux_rows=[{"input_tokens": None, "output_tokens": "", "model": None}])
add(F, aux_rows=[{"task": "t", "input_tokens": "x"}])
add(F, aux_rows=["notdict"])
add(F, aux_rows=[{"task": 5, "input_tokens": 2}, {"task": "5", "input_tokens": 3}])
add(F, aux_rows=[{"task": "t", "input_tokens": 1.5, "output_tokens": 2.5}])

F = "ws_u_merge_aux_into_by_model"
add(F, by_model=[{"model": "m", "input_tokens": 10, "sessions": 2}], aux_rows=[])
add(F, by_model=[{"model": "m", "input_tokens": 10, "output_tokens": 0,
                  "estimated_cost": 0, "sessions": 2, "api_calls": 0}],
    aux_rows=[{"model": "m", "task": "t", "input_tokens": 5, "api_calls": 1}])
add(F, by_model=[], aux_rows=[{"model": "new", "task": "vision",
                               "input_tokens": 7, "output_tokens": 3}])
add(F, by_model=[{"model": "m", "aux_tasks": [{"task": "old"}]}],
    aux_rows=[{"model": "m", "task": "new", "input_tokens": 1}])
add(F, by_model=[{"model": "m", "aux_tasks": None}],
    aux_rows=[{"model": "m", "task": "t"}])
add(F, by_model=[{"model": "m", "input_tokens": 1},
                 {"model": "m", "input_tokens": 9}],
    aux_rows=[{"model": "m", "input_tokens": 1}])
add(F, by_model=[{"model": "a", "input_tokens": 1},
                 {"model": "b", "input_tokens": 50}],
    aux_rows=[{"model": "c", "input_tokens": 20}])
add(F, by_model=[{"model": "m", "input_tokens": "x"}],
    aux_rows=[{"model": "m", "input_tokens": 1}])

F = "ws_u_http_response_error_detail"
add(F, resp={"text": "fallback"},
    io={"resp.json": script({"error": {"message": "bad", "code": "E1",
                                        "type": "", "error_description": "  "}})})
add(F, resp={"text": "fallback"}, io={"resp.json": script({"error": "  plain  "})})
add(F, resp={"text": "fallback"}, io={"resp.json": script({"detail": " det "})})
add(F, resp={"text": "fallback"}, io={"resp.json": script({"message": "msg"})})
add(F, resp={"text": " fallback text "}, io={"resp.json": raise_("ValueError", "no json")})
add(F, resp={"text": "x" * 600}, io={"resp.json": script(None)})
add(F, resp={"text": "\u00a0 pad \u00a0"}, io={"resp.json": script([])})
add(F, resp={"text": ""}, io={"resp.json": script({"error": {"message": ""}})})
add(F, resp={"text": "t"}, io={"resp.json": raise_("SystemExit", "bye")})
add(F, resp={"text": "t"},
    io={"resp.json": script({"error": {"message": 5, "code": ["a"]}})})
add(F, resp={"text": None}, io={"resp.json": script({"error_description": "ed"})})
add(F, resp={}, io={})

F = "ws_u_require_memory_provider_ready"
add(F, name="", io={})
add(F, name="mem0", io={"discover_memory_provider_statuses": script(
    [{"name": "mem0", "status": "ready"}])})
add(F, name="mem0", io={"discover_memory_provider_statuses": script(
    [{"name": "mem0", "status": "needs_setup"}])})
add(F, name="nope", io={"discover_memory_provider_statuses": script(
    [{"name": "other", "status": "ready"}])})
add(F, name="mem0", io={"discover_memory_provider_statuses": raise_("RuntimeError", "disc boom")})
add(F, name="mem0", io={"discover_memory_provider_statuses": script([{"status": "ready"}])})
add(F, name="mem0", io={"discover_memory_provider_statuses": script(
    [{"name": "mem0", "status": 5}])})
add(F, name=None, io={})

F = "ws_u_profile_to_dict"
add(F, info={"name": "p1", "path": "/h/p1", "is_default": True, "model": "m",
             "provider": "nous", "has_env": 1, "skill_count": 3,
             "gateway_running": "yes", "description": "d", "description_auto": 1,
             "display_name": "P1", "bot_title": "Bot", "ui_meta": {"a": 1},
             "distribution_name": "dn", "distribution_version": "1.0",
             "distribution_source": "ds", "alias_path": "/h/alias"})
add(F, info={})
add(F, info=None)
add(F, info={"skill_count": "5"})
add(F, info={"skill_count": 2.9})
add(F, info={"skill_count": "abc"})
add(F, info={"alias_path": None, "ui_meta": {}, "description": None})
add(F, info={"skill_count": 0, "path": 5, "display_name": 0, "bot_title": False})
add(F, info={"skill_count": []})

F = "ws_u_managed_files_policy"
add(F, io={"environ.get": "  /forced/root  ",
           "ensure_managed_root": "/resolved/root"})
add(F, create_root=False, io={"environ.get": "/forced/root",
                              "canonical_path": "/canon/root"})
add(F, io={"environ.get": "", "default_hermes_root_is_opt_data": True,
           "ensure_managed_root": "/opt/data"})
add(F, create_root=False, io={"environ.get": "",
                              "default_hermes_root_is_opt_data": True})
add(F, io={"environ.get": "", "default_hermes_root_is_opt_data": False,
           "path_home": "/home/u", "canonical_path": "/home/u"})
add(F, io={"environ.get": raise_("OSError", "env boom")})
add(F, io={"environ.get": "/r",
           "ensure_managed_root": raise_("HTTPException", "Managed files root is unavailable: disk full",
                                         status_code=500)})
add(F, io={"environ.get": "   ", "default_hermes_root_is_opt_data": False,
           "path_home": "/h", "canonical_path": raise_("HTTPException", "Invalid path")})
add(F, create_root=False, io={"environ.get": "",
                              "default_hermes_root_is_opt_data": False,
                              "path_home": "/h", "canonical_path": "/h"})

F = "ws_u_multiplex_port_binding_conflict"
add(F, platform_id="telegram", requested_profile="p2", io={})
add(F, platform_id="api_server", requested_profile=None,
    io={"get_active_profile_name": "default"})
add(F, platform_id="api_server", requested_profile="p2",
    io={"resolve_profile_dir": "/h/p2", "default_gateway_multiplexes": True})
add(F, platform_id="webhook", requested_profile="p2",
    io={"resolve_profile_dir": "/h/p2", "default_gateway_multiplexes": False})
add(F, platform_id="api_server", requested_profile="CURRENT",
    io={"get_active_profile_name": "p9", "default_gateway_multiplexes": True})
add(F, platform_id="api_server", requested_profile="   ",
    io={"get_active_profile_name": "custom"})
add(F, platform_id="api_server", requested_profile="ghost",
    io={"resolve_profile_dir": raise_("HTTPException", "Profile not found", status_code=404)})
add(F, platform_id="webhook", requested_profile="default", io={})
add(F, platform_id=[], requested_profile=None, io={})
add(F, platform_id="api_server", requested_profile="p2",
    io={"resolve_profile_dir": "/h/p2",
        "default_gateway_multiplexes": raise_("RuntimeError", "gw boom")})

F = "ws_u_infer_provider_on_model_change"
add(F, model_val="   ", prev_provider="nous", io={})
add(F, model_val="gpt-5", prev_provider="openai",
    io={"detect_provider_for_model": script(["nous", "gpt-5"])})
add(F, model_val="vendor/model", prev_provider="ollama-local",
    io={"detect_provider_for_model": script(None),
        "normalize_provider": "ollama-local",
        "provider_has_credentials": True})
add(F, model_val="plainmodel", prev_provider="nous",
    io={"detect_provider_for_model": script(None)})
add(F, model_val="vendor/model", prev_provider="openrouter",
    io={"detect_provider_for_model": script(None),
        "normalize_provider": "openrouter"})
add(F, model_val="vendor/model", prev_provider="x", models_import=False, io={})
add(F, model_val="vendor/model", prev_provider="x", detect_import=False,
    io={"detect_provider_for_model": script(None)})
add(F, model_val="vendor/model", prev_provider="x",
    io={"detect_provider_for_model": script(None),
        "normalize_provider": "x", "provider_has_credentials": False})
add(F, model_val="vendor/model", prev_provider="x",
    io={"detect_provider_for_model": script([]),
        "normalize_provider": "x", "provider_has_credentials": "yes"})
add(F, model_val="ab", prev_provider="x",
    io={"detect_provider_for_model": "ab"})
add(F, model_val="m", prev_provider="x",
    io={"detect_provider_for_model": script(["only"])})
add(F, model_val="vendor/model", prev_provider="x",
    io={"detect_provider_for_model": script(None),
        "normalize_provider": raise_("ValueError", "norm boom")})
add(F, model_val="m", prev_provider="x",
    io={"detect_provider_for_model": raise_("SystemExit", "halt")})
add(F, model_val="vendor/model", prev_provider="x",
    io={"detect_provider_for_model": raise_("KeyboardInterrupt", "ki")})

F = "ws_u_split_text_for_speak_stream"
add(F, text="Hi. There? Yes!", cap=4000)
add(F, text="One.  Two.Three", cap=4000)
add(F, text="A.\n\n\nB", cap=4000)
add(F, text="abcdefghijklmnopqrstuvwxyz", cap=5)
add(F, text="Short. Sentence here.", cap=0)
add(F, text="Short. Sentence here.", cap=None)
add(F, text="", cap=10)
add(F, text="   \n\t  ", cap=10)
add(F, text="a.\u00a0b", cap=4000)
add(F, text="Ab. Cd", cap=1)
add(F, text="H\u00e9llo w\u00f6rld. \u00dcber cool!", cap=7)
add(F, text="End.", cap=-5)

F = "ws_u_get_dashboard_plugins"
add(F, io={"secondary_profile_home": "/h/p2",
           "profile_dashboard_plugins": script([{"name": "pl"}])})
add(F, cache=None, io={"secondary_profile_home": None,
                       "discover_dashboard_plugins": script([{"_dir": "/d1"}])})
add(F, cache=[{"_dir": "/d1"}, {"_dir": "/d2"}],
    io={"secondary_profile_home": None, "path_is_dir": [True, True]})
add(F, cache=[{"_dir": "/d1"}, {"_dir": "/d2"}],
    io={"secondary_profile_home": None, "path_is_dir": [True, False],
        "discover_dashboard_plugins": script([{"_dir": "/new"}])})
add(F, force_rescan=True, cache=[{"_dir": "/d1"}],
    io={"secondary_profile_home": None,
        "discover_dashboard_plugins": script([])})
add(F, cache=[{"name": "no-dir"}], io={"secondary_profile_home": None})
add(F, io={"secondary_profile_home": "",
           "profile_dashboard_plugins": script([])})
add(F, cache=None, io={"secondary_profile_home": None,
                       "discover_dashboard_plugins": raise_("OSError", "scan boom")})
add(F, cache=[{"_dir": 5}], io={"secondary_profile_home": None})

F = "ws_u_update_memory_provider_config"
add(F, provider={"name": "mem0", "storage": "flat_json"}, values={"k": "v"},
    io={"write_provider_flat": None,
        "load_config": script({"memory": {"provider": "other"}}),
        "save_config": None})
add(F, provider={"name": "mem0", "storage": "provider"}, values={},
    io={"write_provider_own": None,
        "load_config": script({"memory": {"provider": "mem0"}})})
add(F, provider={"name": "mem0", "storage": "bogus"}, values={}, io={})
add(F, provider={"name": "mem0", "storage": None}, values={}, io={})
add(F, provider={"name": "mem0", "storage": "flat_json"}, values={"a": 1},
    io={"write_provider_flat": None,
        "load_config": script({"memory": "notdict"}), "save_config": None})
add(F, provider={"name": "mem0", "storage": "flat_json"}, values={},
    io={"write_provider_flat": raise_("OSError", "write boom")})
add(F, provider={"name": "mem0", "storage": "flat_json"}, values={},
    io={"write_provider_flat": None, "load_config": raise_("RuntimeError", "load boom")})
add(F, provider={"name": "mem0", "storage": ["flat_json"]}, values={}, io={})
add(F, provider={"name": "mem0", "storage": "provider"}, values={"x": "y"},
    io={"write_provider_own": None,
        "load_config": script({}), "save_config": raise_("OSError", "save boom")})

F = "ws_u_whatsapp_linked_account_from_session"
add(F, session_path="/h/wa", io={"read_text": script(json.dumps(
    {"me": {"id": "12345:6@s.whatsapp.net", "name": "Me Name"},
     "account": {"id": "999", "name": "Acct"}}))})
add(F, session_path="/h/wa", io={"read_text": script(json.dumps(
    {"account": {"jid": "777@s.whatsapp.net", "pushName": "Push"}}))})
add(F, session_path="/h/wa", io={"read_text": script(json.dumps(
    {"id": "555:1@lid", "verifiedName": "Root Name"}))})
add(F, session_path="/h/wa", io={"read_text": "\ufeff" + json.dumps(
    {"me": {"lid": "888", "notify": "Bom"}})})
add(F, session_path="/h/wa", io={"read_text": "{not json"})
add(F, session_path="/h/wa", io={"read_text": raise_("OSError", "no file")})
add(F, session_path="/h/wa", io={"read_text": script("[1, 2]")})
add(F, session_path="/h/wa/", io={"read_text": script(json.dumps(
    {"me": {"id": "abc-no-digits"}}))})
add(F, session_path="/h/wa", io={"read_text": script(json.dumps({"me": {}}))})
add(F, session_path="/h/wa", io={"read_text": raise_("SystemExit", "stop")})
add(F, session_path="/h/wa", io={"read_text": script(json.dumps(
    {"me": {"id": "12é45", "name": "Uni"}}))})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn, n in sorted(counts.items()):
        flag = "OK " if n >= 5 else "LOW"
        print(f"  {flag} {n:3d}  {fn}")
    assert all(n >= 5 for n in counts.values()), "every function needs >=5 cases"
    assert len(counts) == 19


if __name__ == "__main__":
    main()