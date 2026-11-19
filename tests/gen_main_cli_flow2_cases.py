#!/usr/bin/env python3
"""Generator for tests/fixtures/main_cli_flow2.jsonl.

Emits >=5 cases per ported function of hermes_cli/main.py (the UNCLAIMED
remainder ported as src/cli/main_cli_flow2.c). Each case is
{"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...}}}.

Script shapes (shared with the C universal seam mc2_call and the oracle):
  - absent key            -> RuntimeError("no script for <name>")  [FAIL-CLOSED]
  - present null          -> None
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m, ...} -> raises a REAL exception of that class
  - any other JSON value  -> reused for every call

Domain restrictions (documented in the port header):
  - ASCII operands; object-double fields are strings/numbers/bools/null/records.
  - setup_fn/handler_fn are string markers or null; command/argv tokens ASCII.
  - integral floats collapse to ints on both sides.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "main_cli_flow2.jsonl")

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


# ── _warn_if_unsupervised_pid1 ──────────────────────────────────────────────
case("_warn_if_unsupervised_pid1", pid=2)
case("_warn_if_unsupervised_pid1", pid=None, io=io(**{"os.getpid": 2}))
case("_warn_if_unsupervised_pid1", pid=None, io=io(**{"os.getpid": 1, "print": None}))
case("_warn_if_unsupervised_pid1", pid=1, io=io(print=None))
case("_warn_if_unsupervised_pid1", pid=1, io=io(print=raise_("SystemExit", "bye")))
case("_warn_if_unsupervised_pid1", pid=None, io=io(**{"os.getpid": raise_("KeyboardInterrupt")}))
case("_warn_if_unsupervised_pid1", pid=None)  # fail-closed os.getpid

# ── _read_git_revision_fingerprint ──────────────────────────────────────────
case("_read_git_revision_fingerprint", repo_root="/repo",
     io=io(**{"path.is_file": False, "path.exists": False, "path.read_text": "abc123\n"}))
case("_read_git_revision_fingerprint", repo_root="/repo",
     io=io(**{"path.is_file": False, "path.exists": seq(False, True),
              "path.read_text": seq("ref: refs/heads/main\n", "sha1\n")}))
case("_read_git_revision_fingerprint", repo_root="/repo",
     io=io(**{"path.is_file": True, "path.read_text": seq("gitdir: /real/git\n", "ref: refs/heads/x\n"),
              "path.resolve": "/real/git", "path.exists": seq(False, False, False),
              "_read_packed_ref": "packedsha"}))
case("_read_git_revision_fingerprint", repo_root="/repo",
     io=io(**{"path.is_file": False, "path.exists": seq(False, False, False),
              "path.read_text": "ref: refs/heads/y\n", "_read_packed_ref": None}))
case("_read_git_revision_fingerprint", repo_root="/repo",
     io=io(**{"path.is_file": False, "path.exists": False, "path.read_text": raise_("OSError", "no")}))
case("_read_git_revision_fingerprint", repo_root="/repo",
     io=io(**{"path.is_file": False, "path.exists": seq(True, False, True),
              "path.read_text": seq("../common\n", "ref: refs/heads/z\n", "zsha\n"),
              "path.resolve": "/common"}))
case("_read_git_revision_fingerprint", repo_root="/repo",
     io=io(**{"path.is_file": False, "path.exists": False, "path.read_text": raise_("SystemExit")}))
case("_read_git_revision_fingerprint", repo_root="/repo")  # fail-closed

# ── _auth_store_logged_in ───────────────────────────────────────────────────
REG = {"nous": {"auth_type": "oauth"}}
REG_API = {"nous": {"auth_type": "api_key"}}
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG, strict_profile_scope=False,
     io=io(**{"path.exists": False}))
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG, strict_profile_scope=False,
     io=io(**{"path.exists": True, "path.read_text": '{"active_provider":"nous"}',
              "json.loads": {"active_provider": "nous"}, "get_auth_status": {"logged_in": True}}))
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG_API, strict_profile_scope=True,
     io=io(**{"path.exists": True, "path.read_text": '{"active_provider":"nous"}',
              "json.loads": {"active_provider": "nous"}}))
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG, strict_profile_scope=False,
     io=io(**{"path.exists": True, "path.read_text": "{}", "json.loads": {"active_provider": None}}))
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG, strict_profile_scope=False,
     io=io(**{"path.exists": True, "path.read_text": "bad", "json.loads": raise_("JSONDecodeError", "bad")}))
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG, strict_profile_scope=False,
     io=io(**{"path.exists": True, "path.read_text": '{"active_provider":"nous"}',
              "json.loads": {"active_provider": "nous"}, "get_auth_status": {"logged_in": False}}))
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG, strict_profile_scope=False,
     io=io(**{"path.exists": True, "path.read_text": '{"active_provider":"nous"}',
              "json.loads": {"active_provider": "nous"}, "get_auth_status": raise_("SystemExit")}))
case("_auth_store_logged_in", auth_file="/a/auth.json", registry=REG, strict_profile_scope=False,
     io=io(**{"path.exists": True, "path.read_text": '{"active_provider":"nous"}'}))  # fail-closed json.loads swallowed

# ── _resolve_workspace_key ──────────────────────────────────────────────────
case("_resolve_workspace_key", io=io(**{"subprocess.run": {"returncode": 0, "stdout": "/repo\n"},
                                       "os.path.abspath": "/repo"}))
case("_resolve_workspace_key", io=io(**{"subprocess.run": {"returncode": 1, "stdout": ""},
                                       "os.getcwd": "/cwd"}))
case("_resolve_workspace_key", io=io(**{"subprocess.run": raise_("RuntimeError", "x"), "os.getcwd": "/cwd"}))
case("_resolve_workspace_key", io=io(**{"subprocess.run": {"returncode": 0, "stdout": "  "},
                                       "os.getcwd": "/cwd"}))
case("_resolve_workspace_key", io=io(**{"subprocess.run": {"returncode": 1, "stdout": ""},
                                       "os.getcwd": raise_("OSError", "x")}))
case("_resolve_workspace_key", io=io(**{"subprocess.run": {"returncode": 0}, "os.getcwd": "/cwd"}))
case("_resolve_workspace_key")  # fail-closed both swallowed -> None

# ── _session_db ─────────────────────────────────────────────────────────────
SDB_OK = {"get_hermes_home": "/home", "open_persistence_reader": {"id": "s1"},
          "store.ready": None, "SurfaceLabelReader": {"persistence": {"sessions": {}}},
          "store.close": None}
case("_session_db", io=io(**SDB_OK))
case("_session_db", io=io(get_hermes_home="/home", open_persistence_reader=raise_("RuntimeError", "x")))
case("_session_db", io=io(get_hermes_home="/home", open_persistence_reader={"id": "s1"},
                          **{"store.ready": raise_("ValueError", "x"), "store.close": None}))
case("_session_db", body_raise=raise_("ValueError", "body"), io=io(**SDB_OK))
case("_session_db", body_raise=raise_("SystemExit", "bye"), io=io(**SDB_OK))
case("_session_db", io=io(get_hermes_home="/home", open_persistence_reader={"id": "s1"},
                          **{"store.ready": None, "SurfaceLabelReader": raise_("RuntimeError", "x"),
                             "store.close": None}))
case("_session_db")  # fail-closed get_hermes_home swallowed -> reader None

# ── _resolve_chat_session_args ──────────────────────────────────────────────
CHAT_BASE = {"_apply_in_dir": None, "_resolve_continue_arg": None,
             "import_foreign_resume": None, "recreate_resumed_tui_worktree": None}
SDB_CHAT = {"get_hermes_home": "/home", "open_persistence_reader": {"id": "s"},
            "store.ready": None, "SurfaceLabelReader": {"persistence": {"sessions": {}}},
            "store.close": None}
case("_resolve_chat_session_args", args={"resume": None, "command": "chat"}, use_tui=False,
     io=io(**CHAT_BASE))
case("_resolve_chat_session_args", args={"resume": "latest"}, use_tui=False,
     io=io(**merge(CHAT_BASE, _latest_session_id="sid1", _resolve_session_by_name_or_id="sid1",
                   **SDB_CHAT, **{"sessions.get": {"cwd": "/saved"}, "os.path.isdir": True,
                                  "os.path.realpath": seq("/saved", "/cwd"), "os.getcwd": "/cwd",
                                  "os.chdir": None, "print": None})))
case("_resolve_chat_session_args", args={"resume": "latest"}, use_tui=True,
     io=io(**merge(CHAT_BASE, _latest_session_id=None, print=seq(None, None), **{"sys.exit": None})))
case("_resolve_chat_session_args", args={"resume": "mytitle"}, use_tui=False,
     io=io(**merge(CHAT_BASE, _resolve_session_by_name_or_id=None, **SDB_CHAT,
                   **{"sessions.get": None})))
case("_resolve_chat_session_args", args={"resume": "sid", "no_restore_cwd": True}, use_tui=False,
     io=io(**merge(CHAT_BASE, _resolve_session_by_name_or_id="sid")))
case("_resolve_chat_session_args", args={"resume": "sid", "worktree": True}, use_tui=False,
     io=io(**merge(CHAT_BASE, _resolve_session_by_name_or_id="sid")))
case("_resolve_chat_session_args", args={"resume": "sid"}, use_tui=False,
     io=io(**merge(CHAT_BASE, _resolve_session_by_name_or_id="sid",
                   get_hermes_home="/home", open_persistence_reader=raise_("RuntimeError", "x"))))
case("_resolve_chat_session_args", args={"resume": "sid"}, use_tui=False,
     io=io(**merge(CHAT_BASE, _resolve_session_by_name_or_id="sid", **SDB_CHAT,
                   **{"sessions.get": {"cwd": "/gone"}, "os.path.isdir": False,
                      "os.getcwd": "/cwd", "print": None})))
case("_resolve_chat_session_args", args={"resume": None})  # fail-closed _apply_in_dir propagates

# ── _warn_retired_xai_models ────────────────────────────────────────────────
XENV = {"MIGRATION_GUIDE_URL": "https://guide", "RETIREMENT_DATE": "2026-12-01"}
case("_warn_retired_xai_models", env=XENV, io=io(load_config={}, find_retired_xai_refs=[]))
case("_warn_retired_xai_models", env=XENV,
     io=io(load_config={"x": 1}, find_retired_xai_refs=["grok-1", "grok-2"],
           format_issue=seq("issue1", "issue2"), **{"sys.stderr.write": seq(None, None, None, None)}))
case("_warn_retired_xai_models", env=XENV, io=io(load_config=raise_("RuntimeError", "x")))
case("_warn_retired_xai_models", env=XENV,
     io=io(load_config={}, find_retired_xai_refs=raise_("SystemExit")))
case("_warn_retired_xai_models", env=XENV)  # fail-closed load_config swallowed
case("_warn_retired_xai_models", env=XENV,
     io=io(load_config={}, find_retired_xai_refs=["a"], format_issue=raise_("ValueError", "x")))

# ── _start_chat_background_prefetch ─────────────────────────────────────────
SK_SEEDED = {"get_hermes_home": "/home", "path.is_dir": True,
             "path.rglob": ["/home/skills/x/SKILL.md"], "thread.start": None}
case("_start_chat_background_prefetch",
     io=io(_termux_should_prefetch_update_check=False, **SK_SEEDED))
case("_start_chat_background_prefetch",
     io=io(_termux_should_prefetch_update_check=True, prefetch_update_check=None,
           prefetch_banner_data=None, **SK_SEEDED))
case("_start_chat_background_prefetch",
     io=io(_termux_should_prefetch_update_check=False, get_hermes_home="/home",
           **{"path.is_dir": False, "_sync_bundled_skills_for_startup": None, "banner.set_cache": None}))
case("_start_chat_background_prefetch",
     io=io(_termux_should_prefetch_update_check=False, get_hermes_home="/home",
           **{"path.is_dir": True, "path.rglob": [], "_sync_bundled_skills_for_startup": None,
              "banner.set_cache": None}))
case("_start_chat_background_prefetch",
     io=io(_termux_should_prefetch_update_check=True, prefetch_update_check=raise_("RuntimeError", "x"),
           **SK_SEEDED))
case("_start_chat_background_prefetch", io=io(_termux_should_prefetch_update_check=raise_("SystemExit")))
case("_start_chat_background_prefetch")  # fail-closed _termux propagates

# ── _forward_command ────────────────────────────────────────────────────────
case("_forward_command", name="cmd_kanban", module="hermes_cli.kanban", attr="cmd_kanban",
     forward_return=True, doc="d", call_args={"x": 1},
     io=io(import_module={}, **{"forward.call": {"rc": 0}}))
case("_forward_command", name="cmd_x", module="m", attr="a", forward_return=False, doc="",
     call_args={}, io=io(import_module={}, **{"forward.call": {"rc": 0}}))
case("_forward_command", name="cmd_x", module="m", attr="a", forward_return=True, doc="hi",
     call_args={}, io=io(import_module={}, **{"forward.call": None}))
case("_forward_command", name="cmd_x", module="m", attr="a", forward_return=True, doc="",
     call_args={}, io=io(import_module=raise_("RuntimeError", "x")))
case("_forward_command", name="cmd_x", module="m", attr="a", forward_return=True, doc="",
     call_args={}, io=io(import_module={}, **{"forward.call": raise_("SystemExit")}))
case("_forward_command", name="cmd_x", module="m", attr="a", forward_return=True, doc="",
     call_args={})  # fail-closed import_module

# ── _resolve_active_provider ────────────────────────────────────────────────
case("_resolve_active_provider", config={}, model_cfg=None, effective_provider="auto",
     custom_provider_map={}, io=io(resolve_provider="nous"))
case("_resolve_active_provider", config={}, model_cfg=None, effective_provider="auto",
     custom_provider_map={},
     io=io(resolve_provider=raise_("AuthError", "none", code="no_provider_configured"), print=None))
case("_resolve_active_provider", config={}, model_cfg=None, effective_provider="auto",
     custom_provider_map={},
     io=io(resolve_provider=raise_("AuthError", "x", code="other"),
           format_auth_error="formatted", print=None))
case("_resolve_active_provider", config={}, model_cfg={"base_url": "http://x"},
     effective_provider="custom", custom_provider_map={"mycust": {"base_url": "http://x"}},
     io=io(_norm_base_url=seq("http://x", "http://x")))
case("_resolve_active_provider", config={"providers": {}}, model_cfg=None,
     effective_provider="nous", custom_provider_map={"cust1": {"name": "Nous", "provider_key": "nous"}},
     io=io(get_compatible_custom_providers={}, resolve_provider_full={"id": "nous", "source": "user-config"},
           custom_provider_aliases=["nous"]))
case("_resolve_active_provider", config={}, model_cfg=None, effective_provider="bogus",
     custom_provider_map={},
     io=io(resolve_provider_full=None, provider_install_hint=None, print=None,
           resolve_provider="auto_resolved"))
case("_resolve_active_provider", config={}, model_cfg=None, effective_provider="auto",
     custom_provider_map={}, io=io(resolve_provider="openrouter", get_env_value="http://x"))
case("_resolve_active_provider", config={}, model_cfg=None, effective_provider="auto",
     custom_provider_map={})  # fail-closed resolve_provider propagates

# ── _pick_provider ──────────────────────────────────────────────────────────
case("_pick_provider", config={}, active=None, provider_labels={}, custom_provider_map={},
     io=io(_build_provider_picker_rows=[[["nous", "Nous", []]], 0], _prompt_provider_choice=None))
case("_pick_provider", config={}, active=None, provider_labels={}, custom_provider_map={},
     io=io(_build_provider_picker_rows=[[["cancel", "Cancel", []]], 0], _prompt_provider_choice=0))
case("_pick_provider", config={}, active=None, provider_labels={}, custom_provider_map={},
     io=io(_build_provider_picker_rows=[[["nous", "Nous", []], ["openai", "OpenAI", []]], 0],
           _prompt_provider_choice=1))
case("_pick_provider", config={}, active="m2", provider_labels={"m1": "Member 1", "m2": "Member 2"},
     custom_provider_map={},
     io=io(_build_provider_picker_rows=[[["group", "Group \u25b8 Sub", ["m1", "m2"]]], 0],
           _prompt_provider_choice=seq(0, 1)))
case("_pick_provider", config={}, active=None, provider_labels={}, custom_provider_map={},
     io=io(_build_provider_picker_rows=[[["group", "Group \u25b8 Sub", ["m1", "m2"]]], 0],
           _prompt_provider_choice=seq(0, None)))
case("_pick_provider", config={}, active=None, provider_labels={}, custom_provider_map={},
     io=io(_build_provider_picker_rows=[[["nous", "Nous", []]], 0], _prompt_provider_choice=5))
case("_pick_provider", config={}, active=None, provider_labels={}, custom_provider_map={})  # fail-closed

# ── _dashboard_prepare_runtime ──────────────────────────────────────────────
DASH_BASE = {"setup_logging": None, "_require_dashboard_web_deps": None,
             "_sync_bundled_skills_quietly": None, "apply_terminal_config_to_env": None,
             "_resolve_dashboard_web_dist": None, "discover_plugins": None,
             "_is_desktop_owned_backend": False, "defer_background_mcp_discovery": None}
case("_dashboard_prepare_runtime", args={"x": 1}, headless_backend=False, io=io(**DASH_BASE))
case("_dashboard_prepare_runtime", args={}, headless_backend=True,
     io=io(**merge(DASH_BASE, _is_desktop_owned_backend=True)))
case("_dashboard_prepare_runtime", args={}, headless_backend=False,
     io=io(**merge(DASH_BASE, _is_desktop_owned_backend=True,
                   start_background_mcp_discovery=None)))
case("_dashboard_prepare_runtime", args={}, headless_backend=False,
     io=io(**merge(DASH_BASE, apply_terminal_config_to_env=raise_("RuntimeError", "x"),
                   **{"logger.debug": None})))
case("_dashboard_prepare_runtime", args={}, headless_backend=False,
     io=io(**merge(DASH_BASE, discover_plugins=raise_("ValueError", "boom"), print=None)))
case("_dashboard_prepare_runtime", args={}, headless_backend=False,
     io=io(**merge(DASH_BASE, _require_dashboard_web_deps=raise_("SystemExit"))))
case("_dashboard_prepare_runtime", args={}, headless_backend=False)  # fail-closed

# ── _first_positional_argv ──────────────────────────────────────────────────
case("_first_positional_argv", env={"argv": ["chat", "hi"]}, io=io(command_argv=["chat", "hi"]))
case("_first_positional_argv", env={"argv": []}, io=io(command_argv=[]))
case("_first_positional_argv", env={"argv": ["--flag"]}, io=io(command_argv=["--flag"]))
case("_first_positional_argv", env={"argv": ["x"]}, io=io(command_argv=raise_("RuntimeError", "x")))
case("_first_positional_argv", env={"argv": ["x"]})  # fail-closed
case("_first_positional_argv", env={"argv": ["-p", "x", "chat"]}, io=io(command_argv=["chat"]))

# ── _resolve_deferred_platform_cli_command ──────────────────────────────────
case("_resolve_deferred_platform_cli_command", command_name=None)
case("_resolve_deferred_platform_cli_command", command_name="")
case("_resolve_deferred_platform_cli_command", command_name="photon",
     io=io(**{"platform_registry.get": None}))
case("_resolve_deferred_platform_cli_command", command_name="photon",
     io=io(**{"platform_registry.get": raise_("ValueError", "x"), "logger.debug": None}))
case("_resolve_deferred_platform_cli_command", command_name="photon",
     io=io(**{"platform_registry.get": raise_("SystemExit")}))
case("_resolve_deferred_platform_cli_command", command_name="x")  # fail-closed

# ── _agent_subcommand_selected ──────────────────────────────────────────────
case("_agent_subcommand_selected", args={"command": "cron", "cron_command": "run"})
case("_agent_subcommand_selected", args={"command": "cron", "cron_command": "list"})
case("_agent_subcommand_selected", args={"command": "gateway", "gateway_command": "run"})
case("_agent_subcommand_selected", args={"command": "chat"})
case("_agent_subcommand_selected", args={"command": None})
case("_agent_subcommand_selected", args={"command": "curator"})
case("_agent_subcommand_selected", args={})  # missing command -> AttributeError

# ── _chat_runs_at_gateway ───────────────────────────────────────────────────
case("_chat_runs_at_gateway", args={"command": None})
case("_chat_runs_at_gateway", args={"command": "chat"})
case("_chat_runs_at_gateway", args={"command": "gateway"})
case("_chat_runs_at_gateway", args={"command": ""})
case("_chat_runs_at_gateway", args={})  # AttributeError

# ── _apply_user_config_bypass ───────────────────────────────────────────────
case("_apply_user_config_bypass", args={"ignore_user_config": True}, io=io(**{"os.environ.set": None}))
case("_apply_user_config_bypass", args={"ignore_user_config": False})
case("_apply_user_config_bypass", args={})
case("_apply_user_config_bypass", args={"ignore_user_config": 1}, io=io(**{"os.environ.set": None}))
case("_apply_user_config_bypass", args={"ignore_user_config": True},
     io=io(**{"os.environ.set": raise_("SystemExit")}))
case("_apply_user_config_bypass", args={"ignore_user_config": True})  # fail-closed

# ── _run_oneshot_from_args ──────────────────────────────────────────────────
case("_run_oneshot_from_args", args={"z": True}, io=io(launch_gateway_chat=0, **{"sys.exit": None}))
case("_run_oneshot_from_args", args={}, io=io(launch_gateway_chat=None, **{"sys.exit": None}))
case("_run_oneshot_from_args", args={}, io=io(launch_gateway_chat="x", **{"sys.exit": None}))
case("_run_oneshot_from_args", args={}, io=io(launch_gateway_chat=raise_("RuntimeError", "x")))
case("_run_oneshot_from_args", args={},
     io=io(launch_gateway_chat=0, **{"sys.exit": raise_("KeyboardInterrupt")}))
case("_run_oneshot_from_args", args={})  # fail-closed

# ── _enable_cpu_profile ─────────────────────────────────────────────────────
case("_enable_cpu_profile", args={"cpu_profile": True}, io=io(enable_cpu_profile_from_args=None))
case("_enable_cpu_profile", args={}, io=io(enable_cpu_profile_from_args=raise_("ValueError", "x")))
case("_enable_cpu_profile", args={}, io=io(enable_cpu_profile_from_args=raise_("SystemExit")))
case("_enable_cpu_profile", args={})  # fail-closed
case("_enable_cpu_profile", args={"cpu_profile": False}, io=io(enable_cpu_profile_from_args=None))

# ── _light_chat_parser ──────────────────────────────────────────────────────
case("_light_chat_parser", io=io(build_top_level_parser=[{"id": "p"}, {"id": "s"}, {"id": "c"}],
                                 **{"parser.set_defaults": None}))
case("_light_chat_parser", io=io(build_top_level_parser=[{"id": "p"}, {"id": "s"}, None]))
case("_light_chat_parser", io=io(build_top_level_parser=[{"id": "p"}, {"id": "s"}, {"id": "c"}],
                                 **{"parser.set_defaults": raise_("SystemExit")}))
case("_light_chat_parser")  # fail-closed
case("_light_chat_parser", io=io(build_top_level_parser=["PARSER", "SUB", "CHAT"]))

# ── _promote_top_level_resume ───────────────────────────────────────────────
case("_promote_top_level_resume", args={"resume": "sid", "continue_last": False, "command": None})
case("_promote_top_level_resume", args={"resume": None, "continue_last": True, "command": None})
case("_promote_top_level_resume", args={"resume": None, "continue_last": False, "command": None})
case("_promote_top_level_resume", args={"resume": "sid", "continue_last": False, "command": "gateway"})
case("_promote_top_level_resume", args={"resume": "sid", "command": None})
case("_promote_top_level_resume", args={"resume": None, "command": None})  # missing continue_last
case("_promote_top_level_resume", args={"resume": "sid", "continue_last": False})  # missing command

# ── _try_fast_serve_launch ──────────────────────────────────────────────────
SERVE_BASE = {"os.environ.get": None, "get_container_exec_info": None,
              "build_serve_parser": {"id": "p"}, "parser.parse_known_args": [{"x": 1}, []],
              "cmd_dashboard": None}
case("_try_fast_serve_launch", env={"argv": ["serve", "--port", "8080"]}, io=io(**SERVE_BASE))
case("_try_fast_serve_launch", env={"argv": ["chat"]}, io=io(**{"os.environ.get": None}))
case("_try_fast_serve_launch", env={"argv": ["serve", "-h"]}, io=io(**{"os.environ.get": None}))
case("_try_fast_serve_launch", env={"argv": ["serve"]}, io=io(**{"os.environ.get": "1"}))
case("_try_fast_serve_launch", env={"argv": ["serve"]},
     io=io(**merge(SERVE_BASE, get_container_exec_info={"x": 1})))
case("_try_fast_serve_launch", env={"argv": ["serve"]},
     io=io(**merge(SERVE_BASE, **{"parser.parse_known_args": [{"x": 1}, ["--unknown"]]})))
case("_try_fast_serve_launch", env={"argv": ["serve"]},
     io=io(**merge(SERVE_BASE, **{"parser.parse_known_args": raise_("ArgumentError", "bad")})))
case("_try_fast_serve_launch", env={"argv": ["serve"]},
     io=io(**merge(SERVE_BASE, **{"parser.parse_known_args": raise_("ValueError", "bad")})))
case("_try_fast_serve_launch", env={"argv": ["serve"]},
     io=io(**merge(SERVE_BASE, cmd_dashboard=raise_("SystemExit"))))
case("_try_fast_serve_launch", env={"argv": ["serve"]})  # fail-closed

# ── _attach_plugin_cli_command ──────────────────────────────────────────────
case("_attach_plugin_cli_command",
     cmd_info={"name": "plug", "help": "h", "setup_fn": "setup", "handler_fn": "handler"},
     io=io(**{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": None,
              "parser.set_defaults": None}))
case("_attach_plugin_cli_command", cmd_info={"name": "plug", "help": "h", "setup_fn": "setup"},
     io=io(**{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": None}))
case("_attach_plugin_cli_command",
     cmd_info={"name": "plug", "help": "h", "setup_fn": "setup", "handler_fn": None},
     io=io(**{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": None}))
case("_attach_plugin_cli_command", cmd_info={"help": "h", "setup_fn": "s"})  # KeyError name
case("_attach_plugin_cli_command", cmd_info={"name": "plug", "help": "h", "setup_fn": None},
     io=io(**{"subparsers.add_parser": {"id": "pp"}}))  # TypeError not callable
case("_attach_plugin_cli_command",
     cmd_info={"name": "plug", "help": "h", "setup_fn": "setup"},
     io=io(**{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": raise_("ValueError", "x")}))
case("_attach_plugin_cli_command",
     cmd_info={"name": "plug", "help": "h", "setup_fn": "setup"})  # fail-closed
case("_attach_plugin_cli_command",
     cmd_info={"name": "plug", "help": "h", "description": "desc", "setup_fn": "setup"},
     io=io(**{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": None}))

# ── _register_plugin_cli_commands ───────────────────────────────────────────
REG_BASE = {"_plugin_cli_discovery_needed": True, "command_argv": ["plug"],
            "recover_for_command": None, "discover_plugins": None,
            "platform_registry.get": None}
case("_register_plugin_cli_commands", io=io(_plugin_cli_discovery_needed=False))
case("_register_plugin_cli_commands",
     io=io(**merge(REG_BASE, discover_plugin_cli_commands=[{"name": "plug", "help": "h", "setup_fn": "s"}],
                   **{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": None,
                      "get_plugin_manager": {"_cli_commands": {}}})))
case("_register_plugin_cli_commands",
     io=io(**merge(REG_BASE, discover_plugin_cli_commands=[],
                   **{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": None,
                      "get_plugin_manager": {"_cli_commands": {"extra": {"name": "extra", "help": "h", "setup_fn": "s"}}}})))
case("_register_plugin_cli_commands",
     io=io(**merge(REG_BASE, discover_plugin_cli_commands=[{"name": "dup", "help": "h", "setup_fn": "s"}],
                   **{"subparsers.add_parser": {"id": "pp"}, "plugin.setup_fn": None,
                      "get_plugin_manager": {"_cli_commands": {"dup": {"name": "dup", "help": "h", "setup_fn": "s"}}}})))
case("_register_plugin_cli_commands",
     io=io(**merge(REG_BASE, recover_for_command=raise_("ValueError", "x"), **{"logger.debug": None})))
case("_register_plugin_cli_commands", io=io(_plugin_cli_discovery_needed=raise_("SystemExit")))
case("_register_plugin_cli_commands", io=io(_plugin_cli_discovery_needed=True))  # fail-closed


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