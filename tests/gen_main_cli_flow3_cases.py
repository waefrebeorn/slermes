#!/usr/bin/env python3
"""Generator for tests/fixtures/main_cli_flow3.jsonl.

Emits >=5 cases per ported function of hermes_cli/main.py (the FINAL UNCLAIMED
remainder ported as src/cli/main_cli_flow3.c): _skills_dir_is_unseeded,
_skills_sync_bg, _cmd, _custody_refusal, _build_cli_parser, main. Each case is
{"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...}}}.

Script shapes (shared with the C universal seam mc3_call and the oracle):
  - absent key            -> RuntimeError("no script for <name>")  [FAIL-CLOSED]
  - present null          -> None
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  - any other JSON value  -> reused for every call

Domain restrictions (documented in the port header):
  - ASCII operands; home a normalized absolute POSIX path; path.rglob scripts
    are lists; import_module dicts lack the forwarded attr; build_top_level_parser
    scripts are 3-lists (or null/short/long to exercise unpack raises); rc is
    int/bool/str/null/non-integral float; exception markers name builtins
    constructible with one str message.
  - env.import_fails gates in-function imports; env._stdio_repaired models the
    hermes_cli._stdio_repaired attribute; env.argv feeds sys.argv[1:].
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "main_cli_flow3.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg="", **extra):
    d = {"__raise__": cls, "msg": msg}
    d.update(extra)
    return d


def merge(base, **over):
    d = dict(base)
    d.update(over)
    return d


# ── _skills_dir_is_unseeded ─────────────────────────────────────────────────
case("_skills_dir_is_unseeded", io=io(get_hermes_home="/h", **{"path.is_dir": False}))
case("_skills_dir_is_unseeded", io=io(get_hermes_home="/h", **{"path.is_dir": True, "path.rglob": []}))
case("_skills_dir_is_unseeded", io=io(get_hermes_home="/h", **{"path.is_dir": True,
                                                               "path.rglob": ["/h/skills/a/SKILL.md"]}))
case("_skills_dir_is_unseeded", io=io(get_hermes_home=raise_("RuntimeError", "x")))
case("_skills_dir_is_unseeded", io=io(get_hermes_home=raise_("SystemExit", "bye")))
case("_skills_dir_is_unseeded")  # fail-closed get_hermes_home swallowed -> False
case("_skills_dir_is_unseeded", io=io(get_hermes_home="/h", **{"path.is_dir": True,
                                                               "path.rglob": raise_("OSError", "no")}))
case("_skills_dir_is_unseeded", env={"import_fails": ["hermes_cli.config"]})
case("_skills_dir_is_unseeded", io=io(get_hermes_home=None))  # Path(None) TypeError swallowed
case("_skills_dir_is_unseeded", io=io(get_hermes_home="/h", **{"path.is_dir": True, "path.rglob": [None]}))
case("_skills_dir_is_unseeded", io=io(get_hermes_home=raise_("KeyboardInterrupt")))

# ── _skills_sync_bg ─────────────────────────────────────────────────────────
case("_skills_sync_bg", io=io(_sync_bundled_skills_for_startup=None))
case("_skills_sync_bg", io=io(_sync_bundled_skills_for_startup=raise_("RuntimeError", "x")))
case("_skills_sync_bg", io=io(_sync_bundled_skills_for_startup=raise_("SystemExit", "bye")))
case("_skills_sync_bg", io=io(_sync_bundled_skills_for_startup=raise_("KeyboardInterrupt")))
case("_skills_sync_bg")  # fail-closed swallowed -> None
case("_skills_sync_bg", io=io(_sync_bundled_skills_for_startup=raise_("ValueError", "bad")))

# ── _cmd ────────────────────────────────────────────────────────────────────
case("_cmd", module="m", attr="f", forward_return=False, args={},
     io=io(import_module={}, **{"forward.call": None}))
case("_cmd", module="m", attr="f", forward_return=True, args={"x": 1},
     io=io(import_module={}, **{"forward.call": 3}))
case("_cmd", module="m", attr="f", forward_return=True, args={},
     io=io(import_module={}, **{"forward.call": raise_("OSError", "no")}))
case("_cmd", module="m", attr="f", forward_return=False, args={},
     io=io(import_module=raise_("RuntimeError", "x")))
case("_cmd", module="m", attr="f", forward_return=False, args={})  # fail-closed import_module
case("_cmd", module="m", attr="f", forward_return=True, args={},
     io=io(import_module={}, **{"forward.call": "ok"}))
case("_cmd", module="m", attr="f", forward_return=False, args={},
     env={"import_fails": ["importlib"]})
case("_cmd", module="m", attr="f", forward_return=False, args={},
     io=io(import_module={}, **{"forward.call": {"x": 1}}))
case("_cmd", module="m", attr="f", forward_return=True, args={},
     io=io(import_module={}, **{"forward.call": raise_("SystemExit")}))
case("_cmd", module="m", attr="f", forward_return=True, args={},
     io=io(import_module={}, **{"forward.call": None}))

# ── _custody_refusal ────────────────────────────────────────────────────────
case("_custody_refusal", io=io(**{"sys.modules.get": None}))
case("_custody_refusal", io=io(**{"sys.modules.get": {}, "custody.refusal_notice": "refused"}))
case("_custody_refusal", io=io(**{"sys.modules.get": {}, "custody.refusal_notice": None}))
case("_custody_refusal", io=io(**{"sys.modules.get": raise_("RuntimeError", "x")}))
case("_custody_refusal", io=io(**{"sys.modules.get": {}, "custody.refusal_notice": raise_("OSError", "no")}))
case("_custody_refusal")  # fail-closed sys.modules.get propagates
case("_custody_refusal", io=io(**{"sys.modules.get": "x"}))  # AttributeError refusal_notice
case("_custody_refusal", io=io(**{"sys.modules.get": {}, "custody.refusal_notice": raise_("SystemExit")}))

# ── _build_cli_parser ───────────────────────────────────────────────────────
# the full registration chain scripted silent (null results); the three
# parser doubles are empty records; kanban/project return records so their
# set_defaults fires.
BUILD_IO = {
    "build_top_level_parser": [{}, {}, {}],
    "parser.set_defaults": None,
    "lsp.register_subparser": None,
    "register_send_subparser": None,
    "build_peer_parser": None,
    "portal.add_parser": None,
    "kanban.build_parser": {},
    "projects.build_parser": {},
    "_register_plugin_cli_commands": None,
    "logger.debug": None,
}
for _nm in [
    "build_model_parser", "build_moa_parser", "build_fallback_parser", "build_worktree_parser",
    "build_browser_parser", "build_secrets_parser", "build_egress_parser", "build_migrate_parser",
    "build_codex_runtime_parser", "build_gateway_parser", "build_setup_parser",
    "build_whatsapp_parser", "build_whatsapp_cloud_parser", "build_slack_parser",
    "build_login_parser", "build_logout_parser", "build_auth_parser", "build_status_parser",
    "build_pause_parser", "build_cron_parser", "build_webhook_parser", "build_hooks_parser",
    "build_doctor_parser", "build_verify_parser", "build_security_parser",
    "build_approvals_parser", "build_dump_parser", "build_debug_parser", "build_backup_parser",
    "build_checkpoints_parser", "build_import_cmd_parser", "build_config_parser",
    "build_skin_parser", "build_console_parser", "build_pairing_parser", "build_skills_parser",
    "build_bundles_parser", "build_plugins_parser", "build_curator_parser", "build_pets_parser",
    "build_clipboard_parser", "build_journey_parser", "build_memory_parser", "build_tools_parser",
    "build_computer_use_parser", "build_mcp_parser", "build_connectors_parser",
    "build_sessions_parser", "build_insights_parser", "build_usage_parser",
    "build_monitoring_parser", "build_vault_parser", "build_update_parser",
    "build_uninstall_parser", "build_acp_parser", "build_profile_parser",
    "build_completion_parser", "build_dashboard_parser", "build_webapp_parser",
    "build_gui_parser", "build_logs_parser", "build_prompt_size_parser",
]:
    BUILD_IO[_nm] = None

case("_build_cli_parser", io=io(**BUILD_IO))
case("_build_cli_parser", io=io(**merge(BUILD_IO, build_top_level_parser=[{"prog": "hermes"}, {"n": 1}, {"c": 2}])))
case("_build_cli_parser", io=io(**merge(BUILD_IO, **{"lsp.register_subparser": raise_("RuntimeError", "lsp")})))
case("_build_cli_parser", io=io(**merge(BUILD_IO, **{"lsp.register_subparser": raise_("SystemExit")})))
case("_build_cli_parser", env={"import_fails": ["agent.lsp.cli"]}, io=io(**BUILD_IO))
case("_build_cli_parser", io=io(**merge(BUILD_IO, **{"kanban.build_parser": None})))
case("_build_cli_parser", io=io(**merge(BUILD_IO, build_model_parser=raise_("ValueError", "x"))))
case("_build_cli_parser")  # fail-closed build_top_level_parser
case("_build_cli_parser", io=io(**merge(BUILD_IO, build_top_level_parser=[{}, {}, None])))
case("_build_cli_parser", env={"import_fails": ["hermes_cli.send_cmd"]}, io=io(**BUILD_IO))
case("_build_cli_parser", io=io(**merge(BUILD_IO, **{"lsp.register_subparser": raise_("RuntimeError", "lsp"),
                                                      "logger.debug": raise_("OSError", "log")})))
case("_build_cli_parser", io=io(**merge(BUILD_IO, **{"projects.build_parser": {"p": 1}})))
case("_build_cli_parser", io=io(**merge(BUILD_IO, build_top_level_parser=[{}, {}])))
case("_build_cli_parser", io=io(**merge(BUILD_IO, build_top_level_parser=[{}, {}, {}, {}])))
case("_build_cli_parser", io=io(**merge(BUILD_IO, build_top_level_parser=None)))
case("_build_cli_parser", io=io(**merge(BUILD_IO, _register_plugin_cli_commands=raise_("RuntimeError", "x"))))

# ── main ────────────────────────────────────────────────────────────────────
MAIN_IO = merge(BUILD_IO, **{
    "_set_process_title": None, "_warn_if_unsupervised_pid1": None, "_advertise_agent_env": None,
    "configure_windows_stdio": None, "install_truststore": None, "_cleanup_quarantined_exes": None,
    "_sweep_stale_bytecode_if_checkout_changed": None, "_recover_update_debts_on_startup": None,
    "_first_positional_argv": "chat", "install_root": "/r", "maybe_run_boot_bootstrap": None,
    "check_runtime": None,
    "_try_termux_fast_tui_launch": False, "_try_termux_fast_cli_launch": False,
    "_try_fast_serve_launch": False, "_try_fast_chat_launch": False,
    "get_container_exec_info": None,
    "_parse_cli_args": {"version": False, "command": "chat", "func": "cmd_chat"},
    "_enable_cpu_profile": None, "cmd_version": None, "_prepare_agent_startup": None,
    "_run_oneshot_from_args": None, "set_process_surface": None, "_default_to_chat": None,
    "args.func": None, "parser.print_help": None,
    "os.environ.setdefault": None, "os.environ.set": None, "sys.exit": None,
    "print": None,
})
ARGS_BASE = {"version": False, "command": "chat", "func": "cmd_chat"}

case("main", io=io(**MAIN_IO))
case("main", io=io(**merge(MAIN_IO, **{"args.func": 2})))
case("main", io=io(**merge(MAIN_IO, **{"args.func": 0})))
case("main", io=io(**merge(MAIN_IO, _parse_cli_args={"version": True, "command": "chat", "func": "cmd_chat"})))
case("main", io=io(**merge(MAIN_IO, _parse_cli_args={"version": False, "command": None, "func": "cmd_chat"})))
case("main", io=io(**merge(MAIN_IO, _parse_cli_args={"version": False, "command": "x"})))
case("main", env={"argv": ["gateway", "run"]},
     io=io(**merge(MAIN_IO, get_container_exec_info={"root": "/c"})))
case("main", io=io(**merge(MAIN_IO, _try_fast_chat_launch=True)))
case("main", io=io(**merge(MAIN_IO, check_runtime="problem")))
case("main", io=io(**merge(MAIN_IO, check_runtime=raise_("RuntimeError", "pm"))))
case("main", io=io(**merge(MAIN_IO, configure_windows_stdio=raise_("RuntimeError", "stdio"))))
case("main", env={"_stdio_repaired": True}, io=io(**MAIN_IO))
case("main", io=io(**merge(MAIN_IO, _parse_cli_args={"version": False, "yolo": True,
                                                     "command": "chat", "func": "cmd_chat"})))
case("main", io=io(**merge(MAIN_IO, _parse_cli_args={"version": False, "oneshot": "run",
                                                     "command": "chat", "func": "cmd_chat"})))
case("main")  # fail-closed _set_process_title
case("main", io=io(**merge(MAIN_IO, _parse_cli_args={"version": False})))  # missing command
case("main", io=io(**merge(MAIN_IO, _parse_cli_args={"version": False, "command": "chat", "func": None})))
case("main", io=io(**merge(MAIN_IO, **{"args.func": True})))
case("main", io=io(**merge(MAIN_IO, _first_positional_argv="update")))
case("main", env={"import_fails": ["hermes_cli.venv_sync"]}, io=io(**MAIN_IO))
case("main", env={"argv": ["gateway", "run"]},
     io=io(**merge(MAIN_IO, get_container_exec_info={"root": "/c"},
                   exec_in_container=raise_("OSError", "exec"))))
case("main", io=io(**merge(MAIN_IO, check_runtime="problem", print=raise_("OSError", "print"))))
case("main", io=io(**merge(MAIN_IO, _parse_cli_args=None)))  # args None -> version AttributeError
case("main", io=io(**merge(MAIN_IO, **{"args.func": raise_("SystemExit", "bye")})))
case("main", env={"import_fails": ["hermes_cli.stdio"]}, io=io(**MAIN_IO))
case("main", io=io(**merge(MAIN_IO, _try_termux_fast_tui_launch=raise_("KeyboardInterrupt"))))
case("main", io=io(**merge(MAIN_IO, **{"args.func": 1.5})))  # non-integral float: not an int
case("main", io=io(**merge(MAIN_IO, **{"args.func": "2"})))  # str rc: no exit


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"Wrote {len(CASES)} cases across {len(counts)} funcs to {OUT}")
    if short:
        print(f"WARNING: funcs with <5 cases: {short}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()