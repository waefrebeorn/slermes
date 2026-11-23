#!/usr/bin/env python3
"""Differential oracle for tui_gateway/server.py (gate w6_server_8d971d).

server.py has ~25 heavyweight imports and import-time side effects; every one
is stubbed so the LIVE module object is loaded from the real source and the
ported pure leaves are compared against its own functions.
"""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


class Any:
    def __init__(self, *a, **k): pass
    def __call__(self, *a, **k): return Any()
    def __getattr__(self, n): return Any()
    def __iter__(self): return iter(())
    def __bool__(self): return False
    def register(self, *a, **k): return None


def pkg(name, path):
    m = types.ModuleType(name); m.__path__ = [path]; sys.modules[name] = m; return m


pkg("agent", Q + "/agent")
pkg("hermes_cli", Q + "/hermes_cli")
pkg("tui_gateway", Q + "/tui_gateway")
pkg("utils", Q + "/utils")
pkg("session_rules", Q + "/session_rules")
pkg("tools", Q + "/tools")
pkg("persistence", Q + "/persistence")
pkg("persistence.gateway", Q + "/persistence/gateway")


def stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def envf(name, default, cast=float):
    import os
    raw = os.environ.get(name)
    if raw is None:
        return cast(default)
    try:
        return cast(raw)
    except Exception:
        return cast(default)


stub("tui_gateway._env", env_float=envf, env_int=lambda n, d: envf(n, d, int))
stub("hermes_constants", get_hermes_home=lambda: Q + "/.hermes",
     get_hermes_home_override=lambda: None, get_process_hermes_home=lambda: Q + "/.hermes",
     profile_name_for_home=lambda h: "default", get_scratch_dir=lambda: Q + "/.scratch",
     get_skills_dir=lambda: Q + "/skills", is_wsl=lambda: False,
     reset_hermes_home_override=lambda: None, set_hermes_home_override=lambda *a: None,
     translate_cwd_for_wsl_backend=lambda s: s, windows_path_to_wsl=lambda s: None)
stub("hermes_cli._startup_fast", project_root_str=lambda: Q)
stub("hermes_cli.env_loader", load_hermes_dotenv=lambda **k: None)
stub("utils", is_truthy_value=lambda v, default=False: default)
stub("utils.env", is_truthy_value=lambda v, default=False: default)
stub("utils.base_url_origin", base_url_origin=lambda s: ("scheme", "host"))
stub("persistence.gateway.routing", BARE_BILLING_PROVIDERS=())
for n in ["agent.secret_scope", "hermes_cli.config_backend", "hermes_cli.process_identity",
          "tui_gateway.pet_sheet", "session_rules.session_ids", "tools.environments.local",
          "agent.fast_mode", "agent.replay_cleanup", "agent.reasoning_effort",
          "agent.voice_turn_route", "agent.compaction_display", "session_rules.turns",
          "agent.conversation_loop", "tui_gateway.git_probe", "tui_gateway.stream_segments",
          "tui_gateway.checkpoints", "tui_gateway.turn_marker", "tui_gateway.contracts",
          "tui_gateway.contracts.registry", "tui_gateway.user_messages",
          "tui_gateway.transport", "tui_gateway.mcp_rpc_helpers", "hermes_cli.commands"]:
    sys.modules[n] = Any()
_split = """session_registry agent_factory slash_worker_client methods_voice methods_browser methods_slash
methods_complete_helpers session_auto_continue plugin_inject store_handles rpc_dispatch agent_callbacks
session_history prompt_attachments session_notifications tool_progress change_watcher session_compression
model_switch compute_host_bridge session_workdir session_lifecycle session_reaper session_transports
prompt_turn billing_view methods_bot_relay methods_complete methods_config methods_config_set methods_images
methods_profiles methods_prompt methods_session methods_tools methods_tools_plugin_servers methods_projects
methods_session_foreign methods_session_control methods_subagents methods_vault methods_free_tier
methods_connectors methods_connectors_account methods_display methods_display_watch methods_onboarding
methods_i18n methods_machine methods_shared_metrics methods_plugin_catalog""".split()
for n in _split:
    sys.modules["tui_gateway." + n] = Any()

SRC = Q + "/tui_gateway/server.py"
spec = importlib.util.spec_from_file_location("tui_gateway.server", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.server"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "ok":
            py = live._ok(r.get("rid"), r.get("result"))
        elif func == "err":
            py = live._err(r.get("rid"), r.get("code", 0), r.get("msg", ""), data=r.get("data"))
        elif func == "normalize_request":
            res = live._normalize_request(r.get("req"))
            if isinstance(res, dict):
                py = res
            else:
                rid, method, params = res
                py = {"ok": True, "rid": rid, "method": method, "params": params}
        elif func == "is_model_switch_marker":
            py = live._is_model_switch_marker(r.get("entry"))
        elif func == "is_pivot_marker":
            py = live._is_pivot_marker(r.get("entry"))
        elif func == "coerce_int":
            py = live._coerce_int_config_value(r.get("value"), r.get("default", 0),
                                               min_value=r.get("min_value", 0))
        elif func == "cli_exec_blocked":
            py = live._cli_exec_blocked(r.get("argv", []))
        elif func == "turn_started_at":
            py = live._turn_started_at(r.get("session"))
        elif func == "event_frame":
            py = live._event_frame(r.get("event", ""), r.get("sid", ""), r.get("payload"))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_server_8d971d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
