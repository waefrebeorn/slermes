#!/usr/bin/env python3
"""Generate tests/fixtures/main_cli_flow.jsonl for the main_cli_flow oracle.

Each case: {"func": <live name>, "in": {...}}. in.io maps seam names to
scripted results (plain value = reused verbatim; list = queue consumed in
call order; {"__script__": true, "ret"/"raise"/"stdout"/"stderr"} = explicit
script entry). in.env is environment STATE (env.get/env.pop are recorded but
state-resolved, not scripted). print/stringio.getvalue need a null marker
entry (echo/effect seams). Run from the repo root:

    python tests/gen_main_cli_flow_cases.py
"""
import json
import os
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "main_cli_flow.jsonl")

CASES = []


def C(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None, stdout=None, stderr=None):
    e = {"__script__": True}
    if ret is not None:
        e["ret"] = ret
    if raise_spec is not None:
        e["raise"] = raise_spec
    if stdout is not None:
        e["stdout"] = stdout
    if stderr is not None:
        e["stderr"] = stderr
    return e


def raise_spec(kind, message="", errno=None, code=None):
    r = {"type": kind}
    if kind == "SystemExit":
        r["code"] = code
    else:
        r["message"] = message
        if errno is not None:
            r["errno"] = errno
    return r


# ── _early_interface_config_path ──────────────────────────────────────────
C("_early_interface_config_path", env={"HERMES_HOME": "/home/u/.hermes"}, io={})
C("_early_interface_config_path", env={"HERMES_HOME": "/x/"}, io={})
C("_early_interface_config_path", env={"HERMES_HOME": ""}, io={"expanduser": "/home/u"})
C("_early_interface_config_path", env={}, io={"expanduser": "/root"})
C("_early_interface_config_path", env={"HERMES_HOME": "rel/dir"}, io={})
C("_early_interface_config_path", env={}, io={"expanduser": "~"})

# ── _resolve_sudo_user_profile_env ────────────────────────────────────────
C("_resolve_sudo_user_profile_env", name="default", io={})
C("_resolve_sudo_user_profile_env", name="work",
  io={"sudo_invoker_default_home": None})
C("_resolve_sudo_user_profile_env", name="work",
  io={"sudo_invoker_default_home": "/home/u", "named_profile_is_live": True})
C("_resolve_sudo_user_profile_env", name="work",
  io={"sudo_invoker_default_home": "/home/u", "named_profile_is_live": False})
C("_resolve_sudo_user_profile_env", name="work",
  io={"sudo_invoker_default_home": "/home/u/", "named_profile_is_live": True})
C("_resolve_sudo_user_profile_env", name="",
  io={"sudo_invoker_default_home": "/home/u", "named_profile_is_live": True})

# ── _under_gateway_supervisor ─────────────────────────────────────────────
C("_under_gateway_supervisor", argv=["gateway", "run"],
  env={"HERMES_SUPERVISED_CHILD": "1"}, io={})
C("_under_gateway_supervisor", argv=["gateway", "run"],
  env={"HERMES_S6_SUPERVISED_CHILD": "1"}, io={})
C("_under_gateway_supervisor", argv=["gateway", "run"],
  env={"INVOCATION_ID": "abc123"}, io={})
C("_under_gateway_supervisor", argv=["chat"],
  env={"INVOCATION_ID": "abc123"}, io={})
C("_under_gateway_supervisor", argv=["-p", "x", "gateway", "run"],
  env={"INVOCATION_ID": "abc123"}, io={})
C("_under_gateway_supervisor", argv=[],
  env={"HERMES_GATEWAY_EXTERNAL_SUPERVISOR": " YES "}, io={})
C("_under_gateway_supervisor", argv=[],
  env={"HERMES_GATEWAY_PROFILE_PIN": "on"}, io={})
C("_under_gateway_supervisor", argv=[],
  env={"HERMES_GATEWAY_EXTERNAL_SUPERVISOR": "0", "HERMES_GATEWAY_PROFILE_PIN": "false"}, io={})
C("_under_gateway_supervisor", argv=[], env={}, io={})
C("_under_gateway_supervisor", argv=["gateway"],
  env={"HERMES_GATEWAY_EXTERNAL_SUPERVISOR": "1", "HERMES_GATEWAY_PROFILE_PIN": "0"}, io={})

# ── _s6_supervised_gateway_run ────────────────────────────────────────────
C("_s6_supervised_gateway_run", argv=["chat"], env={}, io={})
C("_s6_supervised_gateway_run", argv=["gateway", "run"], env={},
  io={"_s6_running": False})
C("_s6_supervised_gateway_run", argv=["gateway", "run"],
  env={"HERMES_GATEWAY_NO_SUPERVISE": "TRUE"}, io={})
C("_s6_supervised_gateway_run", argv=["gateway", "run"],
  env={"HERMES_GATEWAY_NO_SUPERVISE": " yes "},
  io={"_s6_running": True, "gateway_run_no_supervise": False})
C("_s6_supervised_gateway_run", argv=["gateway", "run", "--foreground"], env={},
  io={"_s6_running": True, "gateway_run_no_supervise": True})
C("_s6_supervised_gateway_run", argv=["-p", "x", "gateway", "run"], env={},
  io={"_s6_running": True, "gateway_run_no_supervise": False})
C("_s6_supervised_gateway_run", argv=["gateway"], env={}, io={})
C("_s6_supervised_gateway_run", argv=["gateway", "run"], env={},
  io={"_s6_running": script(raise_spec=raise_spec("RuntimeError", "s6 probe failed"))})

# ── explicit_cli_profile ──────────────────────────────────────────────────
C("explicit_cli_profile", explicit_cli_profile=None, io={})
C("explicit_cli_profile", explicit_cli_profile="default", io={})
C("explicit_cli_profile", explicit_cli_profile="work", io={})
C("explicit_cli_profile", explicit_cli_profile="", io={})
C("explicit_cli_profile", explicit_cli_profile="profile with space", io={})

# ── _read_packed_ref ──────────────────────────────────────────────────────
PR = "/repo/.git"
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": script(raise_spec=raise_spec("FileNotFoundError", "packed-refs", errno=2))})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": "# pack-refs with: peeled\nabc123 refs/heads/main\n^def456\n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/other",
  io={"path.read_text": "abc123 refs/heads/main\n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": "abc123  refs/heads/main\n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": "  abc123 refs/heads/main\n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": "\ufeffabc123 refs/heads/main\n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main", io={"path.read_text": ""})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main", io={"path.read_text": "abc123\n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": "abc123 refs/heads/main \n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": "^abc123 refs/heads/main\n"})
C("_read_packed_ref", common_dir=PR, ref="refs/heads/main",
  io={"path.read_text": script(raise_spec=raise_spec("ValueError", "bad decode"))})

# ── _dotenv_has_provider_key ──────────────────────────────────────────────
EF = "/home/u/.hermes/.env"
KEYS = ["OPENAI_API_KEY", "ANTHROPIC_API_KEY"]
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": False})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "# comment\n"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "NOEQUALS\n"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "OPENAI_API_KEY=***"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "export OPENAI_API_KEY=***"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "OPENAI_API_KEY=***"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "OPENAI_API_KEY=***"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "OPENAI_API_KEY=***"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=["OTHER_KEY"],
  io={"path.exists": True, "path.read_text": " OTHER_KEY =v\n"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True,
      "path.read_text": script(raise_spec=raise_spec("OSError", "read failed", errno=5))})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True, "path.read_text": "OPENAI_API_KEY=***"})
C("_dotenv_has_provider_key", env_file=EF, provider_env_vars=KEYS,
  io={"path.exists": True,
      "path.read_text": script(raise_spec=raise_spec("SystemExit", code=9))})

# ── _latest_session_id ────────────────────────────────────────────────────
C("_latest_session_id", use_tui=False, io={"_resolve_last_session": "s1"})
C("_latest_session_id", use_tui=False, io={"_resolve_last_session": None})
C("_latest_session_id", use_tui=True, io={"_resolve_last_session": "t1"})
C("_latest_session_id", use_tui=True, io={"_resolve_last_session": [None, "c1"]})
C("_latest_session_id", use_tui=True, io={"_resolve_last_session": [None, None]})
C("_latest_session_id", use_tui=True, io={"_resolve_last_session": ["", "c1"]})

# ── _first_run_setup_guard ────────────────────────────────────────────────
FR_IO = {"print": None, "cmd_setup": None,
         "print_noninteractive_setup_guidance": None}
C("_first_run_setup_guard", args={},
  io=dict(FR_IO, is_interactive_stdin=False))
C("_first_run_setup_guard", args={},
  io=dict(FR_IO, is_interactive_stdin=True, input=""))
C("_first_run_setup_guard", args={},
  io=dict(FR_IO, is_interactive_stdin=True, input="Y"))
C("_first_run_setup_guard", args={},
  io=dict(FR_IO, is_interactive_stdin=True, input=" YES "))
C("_first_run_setup_guard", args={},
  io=dict(FR_IO, is_interactive_stdin=True, input="n"))
C("_first_run_setup_guard", args={},
  io=dict(FR_IO, is_interactive_stdin=True,
          input=script(raise_spec=raise_spec("EOFError", "EOF when reading a line"))))
C("_first_run_setup_guard", args={},
  io=dict(FR_IO, is_interactive_stdin=True,
          input=script(raise_spec=raise_spec("KeyboardInterrupt"))))
C("_first_run_setup_guard", args={"query": "hi"},
  io=dict(FR_IO, is_interactive_stdin=True, input="no"))

# ── _finalize_update_receipt ──────────────────────────────────────────────
C("_finalize_update_receipt", code=0, reason="ok",
  io={"finalize_pending_update_receipt": None})
C("_finalize_update_receipt", code=1, reason="fail",
  io={"finalize_pending_update_receipt": None})
C("_finalize_update_receipt", code=2, reason="",
  io={"finalize_pending_update_receipt": script(raise_spec=raise_spec("Exception", "disk full"))})
C("_finalize_update_receipt", code=3, reason="x",
  io={"finalize_pending_update_receipt": script(raise_spec=raise_spec("SystemExit", code=3))})
C("_finalize_update_receipt", code=1, reason="y",
  io={"finalize_pending_update_receipt": script(raise_spec=raise_spec("KeyboardInterrupt"))})
C("_finalize_update_receipt", code=0, reason="done",
  io={"finalize_pending_update_receipt": script(raise_spec=raise_spec("OSError", "io", errno=5))})

# ── _update_preflight_handled ─────────────────────────────────────────────
UP_BASE = {"handle_metadata_args": False, "is_managed": False,
           "evaluate_update_admission": None}
C("_update_preflight_handled", args={}, project_root="/repo",
  io=dict(UP_BASE, handle_metadata_args=True))
C("_update_preflight_handled", args={}, project_root="/repo",
  io=dict(UP_BASE, is_managed=True, managed_error=None, _record_stop=None))
C("_update_preflight_handled", args={"plan": True}, project_root="/repo",
  io=dict(UP_BASE, is_managed=True, managed_error=None))
C("_update_preflight_handled", args={"plan": True}, project_root="/repo",
  io=dict(UP_BASE, collect_runtime_inventory={"profiles": []}, print_update_plan=None))
HOLDERS_EMPTY = []
C("_update_preflight_handled", args={"list_venv_holders": True}, project_root="/repo",
  io=dict(UP_BASE, list_venv_holders=HOLDERS_EMPTY,
          **{"json.dumps": json.dumps(HOLDERS_EMPTY, indent=2)}, print=None))
HOLDERS = [{"pid": 123, "exe": "C:/x/python.exe", "user": "eman"}]
C("_update_preflight_handled", args={"list_venv_holders": True}, project_root="/repo",
  io=dict(UP_BASE, list_venv_holders=HOLDERS,
          **{"json.dumps": json.dumps(HOLDERS, indent=2)}, print=None))
C("_update_preflight_handled", args={}, project_root="/repo",
  io=dict(UP_BASE, evaluate_update_admission={"message": "image-managed install"},
          record_refusal_receipt=None, print=None))
C("_update_preflight_handled", args={"check": True, "branch": "main", "channel": "stable"},
  project_root="/repo",
  io=dict(UP_BASE, _resolve_update_branch="main", _cmd_update_check=None))
C("_update_preflight_handled", args={"check": True}, project_root="/repo",
  io=dict(UP_BASE, _resolve_update_branch="main", _cmd_update_check=None))
C("_update_preflight_handled", args={"check": True, "branch": ""}, project_root="/repo",
  io=dict(UP_BASE, _resolve_update_branch="dev", _cmd_update_check=None))
C("_update_preflight_handled", args={}, project_root="/repo", io=dict(UP_BASE))
C("_update_preflight_handled", args={}, project_root="/repo",
  io=dict(UP_BASE, is_managed=True,
          managed_error=script(raise_spec=raise_spec("SystemExit", code=1))))

# ── _dashboard_lifecycle_flags ────────────────────────────────────────────
DL_IO = {"get_hermes_home": "/home/u/.hermes", "_report_dashboard_status": None,
         "print": None}
C("_dashboard_lifecycle_flags", args={"status": True}, token_file="/t", io=dict(DL_IO))
C("_dashboard_lifecycle_flags", args={"stop": True}, token_file="/t", io=dict(DL_IO))
C("_dashboard_lifecycle_flags", args={}, token_file="/t", io=dict(DL_IO))
C("_dashboard_lifecycle_flags", args={"status": True}, token_file=None, io=dict(DL_IO))
C("_dashboard_lifecycle_flags", args={"stop": True}, token_file=None,
  io=dict(DL_IO, _find_stale_dashboard_pids=[]))
C("_dashboard_lifecycle_flags", args={"stop": True}, token_file=None,
  io=dict(DL_IO, _find_stale_dashboard_pids=[123],
          _kill_stale_dashboard_processes={"failed": []}))
C("_dashboard_lifecycle_flags", args={"stop": True}, token_file=None,
  io=dict(DL_IO, _find_stale_dashboard_pids=[123],
          _kill_stale_dashboard_processes={"failed": [123]}))
C("_dashboard_lifecycle_flags", args={}, token_file=None, io=dict(DL_IO))

# ── _dashboard_sanitize_desktop_env ───────────────────────────────────────
DS_IO = {"_is_desktop_owned_backend": False, "_is_electron_packaged_web_dist": True}
C("_dashboard_sanitize_desktop_env", headless_backend=True,
  env={"HERMES_WEB_DIST": "/app/dist", "HERMES_SERVE_HEADLESS": "1"}, io=dict(DS_IO))
C("_dashboard_sanitize_desktop_env", headless_backend=False,
  env={"HERMES_WEB_DIST": "/app/dist", "HERMES_SERVE_HEADLESS": "1"},
  io=dict(DS_IO, _is_desktop_owned_backend=True))
C("_dashboard_sanitize_desktop_env", headless_backend=False,
  env={"HERMES_WEB_DIST": "/app.asar/unpacked/dist", "HERMES_SERVE_HEADLESS": "1"},
  io=dict(DS_IO))
C("_dashboard_sanitize_desktop_env", headless_backend=False,
  env={"HERMES_WEB_DIST": "/dev/dist", "HERMES_SERVE_HEADLESS": "1"},
  io=dict(DS_IO, _is_electron_packaged_web_dist=False))
C("_dashboard_sanitize_desktop_env", headless_backend=False, env={}, io=dict(DS_IO))
C("_dashboard_sanitize_desktop_env", headless_backend=False,
  env={"HERMES_SERVE_HEADLESS": "1"}, io=dict(DS_IO))
C("_dashboard_sanitize_desktop_env", headless_backend=False,
  env={"HERMES_WEB_DIST": "/app/dist"}, io=dict(DS_IO))

# ── _require_dashboard_web_deps ───────────────────────────────────────────
RD_IO = {"import.fastapi": None, "import.uvicorn": None, "print": None,
         "smart_app_control_block_message": None,
         "missing_optional_deps_message": "The dashboard can't start: repair guidance."}
C("_require_dashboard_web_deps", io=dict(RD_IO))
C("_require_dashboard_web_deps",
  io=dict(RD_IO,
          **{"import.fastapi": script(raise_spec=raise_spec(
              "ImportError", "DLL load failed while importing _ssl: policy block"))},
          smart_app_control_block_message="Smart App Control blocked the embedded runtime."))
C("_require_dashboard_web_deps",
  io=dict(RD_IO,
          **{"import.fastapi": script(raise_spec=raise_spec("ImportError", "No module named 'fastapi'"))},
          smart_app_control_block_message=""))
C("_require_dashboard_web_deps",
  io=dict(RD_IO,
          **{"import.fastapi": script(raise_spec=raise_spec("ImportError", "broken fastapi"))},
          smart_app_control_block_message=None))
C("_require_dashboard_web_deps",
  io=dict(RD_IO,
          **{"import.uvicorn": script(raise_spec=raise_spec("ImportError", "No module named 'uvicorn'"))}))
C("_require_dashboard_web_deps",
  io=dict(RD_IO,
          **{"import.fastapi": script(raise_spec=raise_spec("RuntimeError", "native init crashed"))}))
C("_require_dashboard_web_deps",
  io=dict(RD_IO,
          **{"import.fastapi": script(raise_spec=raise_spec(
              "ModuleNotFoundError", "No module named 'fastapi'"))}))

# ── _guard_noninteractive_user_config ─────────────────────────────────────
C("_guard_noninteractive_user_config",
  args={"_noninteractive_config_validated": True, "oneshot": "hi"}, io={})
C("_guard_noninteractive_user_config", args={}, io={})
C("_guard_noninteractive_user_config", args={"oneshot": "hi"},
  io={"require_parseable_user_config": None})
C("_guard_noninteractive_user_config", args={"query": "q"},
  io={"require_parseable_user_config": None})
C("_guard_noninteractive_user_config", args={"oneshot": "hi", "ignore_user_config": True},
  io={"require_parseable_user_config": None})
C("_guard_noninteractive_user_config", args={"oneshot": "hi", "safe_mode": True},
  io={"require_parseable_user_config": None})
C("_guard_noninteractive_user_config", args={"oneshot": "hi"},
  io={"require_parseable_user_config": script(raise_spec=raise_spec(
      "InvalidUserConfigError", "bad yaml in config")), "print": None})
C("_guard_noninteractive_user_config", args={"oneshot": "", "query": None}, io={})
C("_guard_noninteractive_user_config",
  args={"query": "q", "ignore_user_config": False, "safe_mode": False},
  io={"require_parseable_user_config": None})
C("_guard_noninteractive_user_config", args={"query": "q"},
  io={"require_parseable_user_config": script(raise_spec=raise_spec(
      "RuntimeError", "unexpected"))})

# ── _is_tui_chat_launch ───────────────────────────────────────────────────
C("_is_tui_chat_launch", args={}, env={}, io={"_bypass_chat_launch": True})
C("_is_tui_chat_launch", args={"tui": True}, env={}, io={"_bypass_chat_launch": False})
C("_is_tui_chat_launch", args={}, env={"HERMES_TUI": "1"}, io={"_bypass_chat_launch": False})
C("_is_tui_chat_launch", args={"command": "gateway"}, env={}, io={"_bypass_chat_launch": False})
C("_is_tui_chat_launch", args={"command": None}, env={},
  io={"_bypass_chat_launch": False, "_resolve_use_tui": True})
C("_is_tui_chat_launch", args={"command": "chat"}, env={},
  io={"_bypass_chat_launch": False, "_resolve_use_tui": False})
C("_is_tui_chat_launch", args={"command": "chat"}, env={"HERMES_TUI": "0"},
  io={"_bypass_chat_launch": False, "_resolve_use_tui": True})
C("_is_tui_chat_launch", args={"command": "acp"}, env={}, io={"_bypass_chat_launch": False})

# ── _default_to_chat ──────────────────────────────────────────────────────
DC_IO = {"_promote_top_level_resume": None, "_set_chat_arg_defaults": None,
         "cmd_chat": None}
C("_default_to_chat", args={}, io=dict(DC_IO))
C("_default_to_chat", args={"command": None, "resume": "s1"}, io=dict(DC_IO))
C("_default_to_chat", args={"command": "chat", "query": "hi"}, io=dict(DC_IO))
C("_default_to_chat", args={"continue_last": True}, io=dict(DC_IO))
C("_default_to_chat", args={},
  io=dict(DC_IO, cmd_chat=script(raise_spec=raise_spec("SystemExit", code=0))))

# ── _parse_cli_args ───────────────────────────────────────────────────────
SP = {"has_choices": True, "choices": ["chat", "gateway", "model"], "required": False}
PC_IO = {"stringio.getvalue": None, "print": None}


def pc(io_extra, **kw):
    C("_parse_cli_args", io=dict(PC_IO, **io_extra), **kw)


pc({"_coalesce_session_name_args": ["chat"],
    "parser.parse_args": [script(ret={"command": "chat"})]},
   argv=["chat"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": [],
    "parser.parse_args": [script(ret={"command": None})]},
   argv=[], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["-c", "model"],
    "parser.parse_args": [
        script(raise_spec=raise_spec("SystemExit", code=2),
               stderr="usage: hermes [-h]\nerror: unrecognized arguments: model\n"),
        script(ret={"command": None, "resume": "model"})]},
   argv=["-c", "model"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["--help"],
    "parser.parse_args": [script(raise_spec=raise_spec("SystemExit", code=0),
                                 stdout="usage: hermes [-h] ...\n")]},
   argv=["--help"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["gateway", "run"],
    "parser.parse_args": [script(raise_spec=raise_spec("SystemExit", code=0),
                                 stdout="gateway help\n")]},
   argv=["gateway", "run"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["model"],
    "parser.parse_args": [script(ret={"command": "model"}, stdout="spec text\n")]},
   argv=["model"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["chat"],
    "parser.parse_args": [script(ret={"command": "chat"})]},
   argv=["chat"], subparsers={"has_choices": False, "required": False})
pc({"_coalesce_session_name_args": ["-p", "x", "chat"],
    "parser.parse_args": [script(ret={"command": "chat"})]},
   argv=["-p", "x", "chat"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["-c", "foo bar"],
    "parser.parse_args": [script(ret={"command": None, "resume": "foo bar"})]},
   argv=["-c", "foo", "bar"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["model"],
    "parser.parse_args": [
        script(raise_spec=raise_spec("SystemExit", code=2), stderr="err\n"),
        script(raise_spec=raise_spec("SystemExit", code=2), stderr="err2\n")]},
   argv=["model"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["model"],
    "parser.parse_args": [script(raise_spec=raise_spec("RuntimeError", "parser exploded"))]},
   argv=["model"], subparsers=dict(SP))
pc({"_coalesce_session_name_args": ["chat", "--resume", "x"],
    "parser.parse_args": [script(ret={"command": "chat", "resume": "x"})]},
   argv=["chat", "--resume", "x"], subparsers=dict(SP))


def main():
    counts = Counter(c["func"] for c in CASES)
    short = [f for f, n in counts.items() if n < 5]
    if short:
        raise SystemExit(f"FATAL: funcs with <5 cases: {short}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {OUT}: {len(CASES)} cases across {len(counts)} funcs")
    for fn, n in sorted(counts.items()):
        print(f"  {fn}: {n}")


if __name__ == "__main__":
    main()
