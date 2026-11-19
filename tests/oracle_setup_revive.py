#!/usr/bin/env python3
"""Differential oracle for the setup wizard cluster revival (setup_gaps.c).

Reads the harness's JSONL on stdin ({"func","in","out"}), recomputes the LIVE
hermes_cli/setup*.py reference for each case with identically scripted seams,
and compares the ordered seam trace + return value + final config + final
navigation state. Prints: ORACLE setup_revive: N cases, M mismatches.

Seam scripts (in.io): a single value is reused for every call; an array is
consumed per call (the last element repeats once exhausted). A value carrying
{"__raise__": kind, "msg": ...} scripts an exception. Missing scripts are
fail-closed (the case counts as a mismatch on either side).
"""
import ast
import builtins
import contextlib
import copy
import json
import logging
import os
import re
import sys
import types
import __future__
from contextvars import ContextVar
from dataclasses import dataclass, field
from enum import Enum
from functools import partial
from pathlib import Path
from typing import Any, Callable, Optional

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE setup_revive: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
# The runner scrubs the environment; the quarry import chain needs a home.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "setup_revive_home"))
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

ANN_FLAG = __future__.annotations.compiler_flag


def _extract(rel, modname, seed):
    """AST-extract one quarry module into a registered module namespace."""
    path = QUARRY / rel
    src = path.read_text(encoding="utf-8")
    mod = types.ModuleType(modname)
    mod.__file__ = str(path)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seed)
    ns["__name__"] = modname
    ns["__builtins__"] = builtins.__dict__
    tree = ast.parse(src)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                     flags=ANN_FLAG), ns)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not isinstance(node.target, ast.Name):
                    continue
            else:
                continue
            try:
                exec_node(node)
            except Exception:
                pass

    seed_consts()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            try:
                exec_node(node)
            except Exception:
                pass
    seed_consts()   # constants that reference functions / partial aliases
    return ns


_COMMON = dict(copy=copy, re=re, os=os, sys=sys, logging=logging,
               contextlib=contextlib, Path=Path, Callable=Callable,
               Optional=Optional, Any=Any, Enum=Enum, ContextVar=ContextVar,
               partial=partial, dataclass=dataclass, field=field,
               contextmanager=contextlib.contextmanager)

NS_S = _extract("hermes_cli/setup.py", "hermes_cli.setup", dict(_COMMON))
NS_Q = _extract("hermes_cli/setup_quick.py", "hermes_cli.setup_quick", dict(_COMMON))
NS_T = _extract("hermes_cli/setup_tts.py", "hermes_cli.setup_tts", dict(_COMMON))
NS_P = _extract("hermes_cli/setup_platforms.py", "hermes_cli.setup_platforms", dict(_COMMON))
NS_M = _extract("hermes_cli/setup_migration.py", "hermes_cli.setup_migration", dict(_COMMON))

# MenuNavigationEvent / MenuNavigationStart / _NO_REPLAY live in curses_ui.
_csrc = (QUARRY / "hermes_cli/curses_ui.py").read_text(encoding="utf-8")
for _node in ast.parse(_csrc).body:
    _name = getattr(_node, "name", None)
    if _name is None and isinstance(_node, ast.Assign):
        _name = _node.targets[0].id if isinstance(_node.targets[0], ast.Name) else None
    if _name in ("MenuNavigationEvent", "MenuNavigationStart", "_NO_REPLAY"):
        exec(compile(ast.Module(body=[_node], type_ignores=[]), "curses_ui.py", "exec",
                     flags=ANN_FLAG), NS_S)

# Module-level imported names the extracted defs read (bound per case below).
for _n in ("cfg_get", "DEFAULT_CONFIG", "get_hermes_home", "get_config_path", "get_env_path",
           "load_config", "save_config", "save_env_value", "remove_env_value", "get_env_value",
           "ensure_hermes_home", "Colors", "color", "print_error", "print_info",
           "print_success", "print_warning", "masked_secret_prompt", "project_root_str",
           "setup_tts", "setup_terminal_backend", "setup_gateway", "_print_setup_summary",
           "_skip_configured_section", "offer_agent_import", "_run_portal_one_shot",
           "_run_quick_setup"):
    NS_S.setdefault(_n, None)
NS_S["project_root_str"] = lambda: str(_REPO)
NS_S["color"] = lambda s, *a: s
NS_S["Colors"] = types.SimpleNamespace(CYAN="", BOLD="", YELLOW="", MAGENTA="")
NS_S["DEFAULT_CONFIG"] = {}

# Fail closed if the extraction missed anything the ports rely on.
_REQUIRED = {
    "NS_S": (NS_S, ["_handle_setup_menu_navigation", "_run_setup_steps",
                    "run_setup_action_with_navigation", "setup_model_provider", "setup_tools",
                    "setup_telemetry", "_sub_dict", "_setup_navigation_scope",
                    "_SetupNavigationState", "_SetupCancelled", "_SetupGoBack",
                    "_SETUP_NAVIGATION", "_SEND_CONSENT_EXPLAINER", "_DOCS_BASE",
                    "MenuNavigationEvent", "MenuNavigationStart", "_apply_default_agent_settings"]),
    "NS_Q": (NS_Q, ["_run_portal_one_shot", "_run_first_time_quick_setup", "_print_macos_fda_tip",
                    "_blank_slate_minimal_toolsets", "_blank_slate_minimize_config",
                    "_run_blank_slate_setup", "_blank_slate_walkthrough", "_run_nous_flow",
                    "_reload_config_into", "_quick_setup_nous_portal", "_set_bundled_skills_opt_out",
                    "_MESSAGING_PLATFORMS"]),
    "NS_T": (NS_T, ["_run_xai_oauth_login_from_setup", "_setup_tts_provider",
                    "_TTS_PROVIDER_CHOICES", "_TTS_PROVIDER_LABELS", "_EDGE_VOICE_CHOICES",
                    "_TTS_LOCAL_PROVIDERS", "_TTS_API_KEY_PROVIDERS"]),
    "NS_P": (NS_P, ["_setup_telegram_auto_result", "_profile_name_from_hermes_home"]),
    "NS_M": (NS_M, ["_skip_configured_section", "_get_section_config_summary"]),
}
for _label, (_ns, _names) in _REQUIRED.items():
    _missing = [n for n in _names if n not in _ns or _ns[n] is None]
    if _missing:
        print("ORACLE setup_revive: extraction failed for %s: %s" % (_label, _missing),
              file=sys.stderr)
        sys.exit(2)

# ── stub modules for in-function imports ──


class _NSProxy(types.ModuleType):
    """Module whose attributes resolve through an extraction namespace."""

    def __init__(self, name, ns):
        super().__init__(name)
        self.__dict__["_ns"] = ns

    def __getattr__(self, k):
        try:
            return self.__dict__["_ns"][k]
        except KeyError:
            raise AttributeError(k)


class _FlagModule(types.ModuleType):
    """Module whose gated attributes raise ImportError when a flag is set."""

    def __init__(self, name, gated):
        super().__init__(name)
        self.__dict__["_gated"] = set(gated)
        self.__dict__["_import_error"] = None

    def __getattr__(self, k):
        err = self.__dict__.get("_import_error")
        if err and k in self.__dict__["_gated"]:
            raise ImportError(err)
        raise AttributeError(k)


_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = []
sys.modules["hermes_cli"] = _pkg
sys.modules["hermes_cli.setup"] = _NSProxy("hermes_cli.setup", NS_S)
sys.modules["hermes_cli.setup_quick"] = _NSProxy("hermes_cli.setup_quick", NS_Q)
_pkg.setup = sys.modules["hermes_cli.setup"]
_pkg.setup_quick = sys.modules["hermes_cli.setup_quick"]

MOD_CONFIG = types.ModuleType("hermes_cli.config")
MOD_MAIN = types.ModuleType("hermes_cli.main")
MOD_OBS = types.ModuleType("hermes_cli.observability")
MOD_OBS.__path__ = []
MOD_SURFACE = types.ModuleType("hermes_cli.observability.shared_metrics_setup")
MOD_CONSENT = types.ModuleType("hermes_cli.observability.shared_metrics_consent")
MOD_ERRCOPY = types.ModuleType("hermes_cli.auth_error_copy")
MOD_AUTH = _FlagModule("hermes_cli.auth",
                       ["_is_remote_session", "_save_xai_oauth_tokens",
                        "_xai_oauth_device_code_login", "unsuppress_credential_source"])
MOD_TELEGRAM = _FlagModule("hermes_cli.telegram_managed_bot", ["auto_setup_telegram_bot_result"])
MOD_TOOLSCFG = types.ModuleType("hermes_cli.tools_config")
MOD_TOOLSETS = _FlagModule("toolsets", ["TOOLSETS", "resolve_toolset"])
MOD_FLOWS = types.ModuleType("hermes_cli.model_setup_flows")
MOD_GWSVC = types.ModuleType("hermes_cli.gateway_setup_service")
MOD_NOUSSUB = types.ModuleType("hermes_cli.nous_subscription")
MOD_CURSES = types.ModuleType("hermes_cli.curses_ui")
MOD_TOOLS = types.ModuleType("tools")
MOD_TOOLS.__path__ = []
MOD_SYNC = types.ModuleType("tools.skills_sync")
MOD_BUNDLED = types.ModuleType("tools.skills_sync_bundled_ops")
for _m in (MOD_CONFIG, MOD_MAIN, MOD_OBS, MOD_SURFACE, MOD_CONSENT, MOD_ERRCOPY, MOD_AUTH,
           MOD_TELEGRAM, MOD_TOOLSCFG, MOD_TOOLSETS, MOD_FLOWS, MOD_GWSVC, MOD_NOUSSUB,
           MOD_CURSES, MOD_TOOLS, MOD_SYNC, MOD_BUNDLED):
    sys.modules[_m.__name__] = _m
_pkg.config = MOD_CONFIG
_pkg.main = MOD_MAIN
_pkg.observability = MOD_OBS
_pkg.auth = MOD_AUTH
_pkg.telegram_managed_bot = MOD_TELEGRAM
_pkg.tools_config = MOD_TOOLSCFG
_pkg.model_setup_flows = MOD_FLOWS
_pkg.gateway_setup_service = MOD_GWSVC
_pkg.nous_subscription = MOD_NOUSSUB
_pkg.curses_ui = MOD_CURSES
MOD_OBS.shared_metrics_setup = MOD_SURFACE
MOD_OBS.shared_metrics_consent = MOD_CONSENT
MOD_TOOLS.skills_sync = MOD_SYNC
MOD_TOOLS.skills_sync_bundled_ops = MOD_BUNDLED
sys.modules["tools"] = MOD_TOOLS

# ── scripted seam recorders ──

TRACE = []
NOSCRIPT = []
SCRIPTS = {}
CURSORS = {}


class _NoScript(Exception):
    pass


def _next(name):
    if name not in SCRIPTS:
        NOSCRIPT.append(name)
        raise _NoScript(name)
    v = SCRIPTS[name]
    if isinstance(v, list):
        if not v:
            return None
        i = CURSORS.get(name, 0)
        CURSORS[name] = i + 1
        return v[min(i, len(v) - 1)]
    return v


def _exc(marker):
    kind = marker["__raise__"]
    msg = marker.get("msg", "")
    if kind == "_SetupGoBack":
        return NS_S["_SetupGoBack"](marker.get("prompt_index", 0))
    if kind == "_SetupCancelled":
        return NS_S["_SetupCancelled"]()
    if kind == "_SetupControlFlow":
        return NS_S["_SetupControlFlow"](msg)
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


def call(name, args, transform=None):
    v = _next(name)
    TRACE.append({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(v)})
    if isinstance(v, dict) and "__raise__" in v:
        raise _exc(v)
    if transform is not None:
        return transform(v)
    return v


def exc_json(exc):
    return {"kind": type(exc).__name__, "msg": str(exc)}


def _choice_args(question, choices, default, description):
    a = {"question": question, "choices": list(choices), "default": default}
    if description is not None:
        a["description"] = description
    return a


def _prompt_args(question, default, password):
    a = {"question": question}
    if default is not None:
        a["default"] = default
    if password:
        a["password"] = True
    return a


def s_print(*a, **k):
    TRACE.append({"ev": "print", "args": {}, "ret": None})
    return None


def s_print_header(title, *, gap=False):
    return call("print_header", {"title": title, "gap": gap})


def s_print_banner(*lines):
    return call("_print_banner", {"lines": list(lines)})


def s_info(*lines):
    return call("_info", {"lines": list(lines)})


def s_print_info(msg):
    return call("print_info", {"msg": msg})


def s_print_success(msg):
    return call("print_success", {"msg": msg})


def s_print_warning(msg):
    return call("print_warning", {"msg": msg})


def s_print_error(msg):
    return call("print_error", {"msg": msg})


def s_prompt_yes_no(question, default=True, description=None):
    a = {"question": question, "default": default}
    if description is not None:
        a["description"] = description
    return call("prompt_yes_no", a)


def s_prompt_choice(question, choices, default=0, description=None):
    return call("prompt_choice", _choice_args(question, choices, default, description))


def s_load_config():
    return call("load_config", {})


def s_save_config(config):
    return call("save_config", {"config": config})


def s_get_env_value(name):
    return call("get_env_value", {"name": name})


def s_remove_env_value(name):
    return call("remove_env_value", {"name": name})


def s_get_hermes_home():
    return call("get_hermes_home", {})


def s_setup_terminal_backend(config):
    return call("setup_terminal_backend", {"config": config})


def s_setup_gateway(config):
    return call("setup_gateway", {"config": config})


def s_print_setup_summary(config, hermes_home):
    return call("_print_setup_summary", {"config": config, "hermes_home": hermes_home})


def s_record_setup_completed(config):
    return call("_record_setup_completed", {"config": config})


def s_tools_command(first_install=False, config=None):
    return call("tools_command", {"first_install": first_install, "config": config})


def s_save_consent(enabled, send, config):
    return call("save_consent", {"enabled": enabled, "send": send, "config": config})


def s_get_section_summary(config, section_key):
    return call("_get_section_config_summary", {"config": config, "section_key": section_key})


def s_select_provider_and_model():
    return call("select_provider_and_model", {})


def s_failure_lines(exc, retry_command=None):
    return call("provider_setup_failure_lines",
                {"exc": exc_json(exc), "retry_command": retry_command})


def s_logger_debug(fmt, *a):
    return call("logger.debug", {"msg": fmt % a if a else fmt})


def s_is_remote_session():
    return call("_is_remote_session", {})


def s_device_login(open_browser=False):
    return call("_xai_oauth_device_code_login", {"open_browser": open_browser})


def s_save_tokens(tokens, **kw):
    a = {"tokens": tokens}
    a.update({k: v for k, v in kw.items() if v is not None})
    return call("_save_xai_oauth_tokens", a)


def s_unsuppress(source, kind):
    return call("unsuppress_credential_source", {"source": source, "kind": kind})


def s_auto_telegram(profile_name=None):
    a = {}
    if profile_name is not None:
        a["profile_name"] = profile_name
    return call("auto_setup_telegram_bot_result", a)


def s_listdir(path):
    return call("os.listdir", {"path": Path(path).as_posix()})


def s_plugin_keys():
    return call("_get_plugin_toolset_keys", {})


def s_resolve_toolset(name):
    return call("resolve_toolset", {"name": name})


def s_model_flow_nous(config):
    return call("_model_flow_nous", {"config": config})


def s_ensure_gateway_service(context=None, interactive=None, config=None):
    return call("ensure_gateway_service",
                {"context": context, "interactive": interactive, "config": config})


def s_keep_imported(config, imported_from):
    return call("_keep_imported_model", {"config": config, "imported_from": imported_from})


def s_set_bundled_opt_out(opt_out):
    return call("set_bundled_skills_opt_out", {"opt_out": opt_out})


def s_sync_skills(quiet=False):
    return call("sync_skills", {"quiet": quiet})


def s_blank_done(config, hermes_home, tools_line, *extra, intro=None):
    a = {"config": config, "hermes_home": hermes_home, "tools_line": tools_line,
         "extra": list(extra)}
    if intro is not None:
        a["intro"] = intro
    return call("_blank_slate_done", a)


def s_managed_nous():
    return call("managed_nous_tools_enabled", {})


def s_nous_features(config):
    return call("get_nous_subscription_features", {"config": config},
                transform=lambda v: types.SimpleNamespace(**(v or {})))


def s_tts_local(selected):
    return call("_tts_local_install_step", {"selected": selected})


def s_tts_api(selected):
    return call("_tts_api_key_step", {"selected": selected})


def s_tts_xai(config):
    return call("_tts_xai_step", {"config": config})


def s_set_handler(handler):
    return call("set_menu_navigation_handler",
                {"handler": getattr(handler, "__name__", str(handler))})


def s_reset_handler(token):
    return call("reset_menu_navigation_handler", {"token": token})


class _Surface:
    """provider_setup_surface context-manager double (enter/exit are ambient)."""

    def __init__(self, label):
        self.label = label

    def __enter__(self):
        TRACE.append({"ev": "provider_setup_surface.enter",
                      "args": {"label": self.label}, "ret": None})
        return self

    def __exit__(self, et, ev, tb):
        TRACE.append({"ev": "provider_setup_surface.exit",
                      "args": {"exc": {"kind": et.__name__, "msg": str(ev)} if et else None},
                      "ret": False})
        return False


def s_surface(label):
    return _Surface(label)


def _setup_double():
    return types.SimpleNamespace(
        print_header=lambda title, *, gap=False: call("_setup.print_header",
                                                      {"title": title, "gap": gap}),
        _info=lambda *lines: call("_setup._info", {"lines": list(lines)}),
        prompt_choice=lambda question, choices, default=0, description=None: call(
            "_setup.prompt_choice", _choice_args(question, choices, default, description)),
        print_info=lambda msg: call("_setup.print_info", {"msg": msg}),
        print_warning=lambda msg: call("_setup.print_warning", {"msg": msg}),
        print_success=lambda msg: call("_setup.print_success", {"msg": msg}),
        print_error=lambda msg: call("_setup.print_error", {"msg": msg}),
        get_env_value=lambda name: call("_setup.get_env_value", {"name": name}),
        save_config=lambda config: call("_setup.save_config", {"config": config}),
        prompt=lambda question, default=None, password=False: call(
            "_setup.prompt", _prompt_args(question, default, password)),
    )


_LOGGER = types.SimpleNamespace(debug=s_logger_debug)

# ── per-case installation ──


def install_case(case):
    global TRACE, NOSCRIPT, SCRIPTS, CURSORS
    TRACE = []
    NOSCRIPT = []
    SCRIPTS = case.get("io") or {}
    CURSORS = {}
    env = case.get("env") or {}

    # hermes_cli.setup namespace: seam the I/O leaves, keep the ported flows real.
    NS_S.update(
        print=s_print, logger=_LOGGER, _info=s_info, print_header=s_print_header,
        print_info=s_print_info, print_success=s_print_success,
        print_warning=s_print_warning, print_error=s_print_error,
        _print_banner=s_print_banner,
        prompt_choice=s_prompt_choice, prompt_yes_no=s_prompt_yes_no,
        load_config=s_load_config, save_config=s_save_config,
        get_env_value=s_get_env_value, remove_env_value=s_remove_env_value,
        get_hermes_home=s_get_hermes_home,
        setup_terminal_backend=s_setup_terminal_backend, setup_gateway=s_setup_gateway,
        _print_setup_summary=s_print_setup_summary,
        _record_setup_completed=s_record_setup_completed,
    )
    MOD_CONFIG.load_config = s_load_config
    MOD_CONFIG.save_config = s_save_config
    MOD_MAIN.select_provider_and_model = s_select_provider_and_model
    MOD_SURFACE.provider_setup_surface = s_surface
    MOD_CONSENT.save_consent = s_save_consent
    MOD_ERRCOPY.provider_setup_failure_lines = s_failure_lines
    MOD_CURSES.set_menu_navigation_handler = s_set_handler
    MOD_CURSES.reset_menu_navigation_handler = s_reset_handler
    MOD_FLOWS._model_flow_nous = s_model_flow_nous
    MOD_GWSVC.ensure_gateway_service = s_ensure_gateway_service
    MOD_NOUSSUB.get_nous_subscription_features = s_nous_features
    MOD_TOOLSCFG.tools_command = s_tools_command
    MOD_TOOLSCFG.CONFIGURABLE_TOOLSETS = env.get("configurable_toolsets") or []
    MOD_TOOLSCFG._get_plugin_toolset_keys = s_plugin_keys
    if env.get("toolsets_import_error"):
        MOD_TOOLSETS.__dict__.pop("TOOLSETS", None)
        MOD_TOOLSETS.__dict__.pop("resolve_toolset", None)
    else:
        MOD_TOOLSETS.TOOLSETS = env.get("toolsets") or {}
        MOD_TOOLSETS.resolve_toolset = s_resolve_toolset
    MOD_TOOLSETS.__dict__["_import_error"] = env.get("toolsets_import_error")
    MOD_SYNC.sync_skills = s_sync_skills
    MOD_BUNDLED.set_bundled_skills_opt_out = s_set_bundled_opt_out
    if env.get("auth_import_error"):
        for _k in ("_is_remote_session", "_xai_oauth_device_code_login",
                   "_save_xai_oauth_tokens", "unsuppress_credential_source"):
            MOD_AUTH.__dict__.pop(_k, None)
    else:
        MOD_AUTH._is_remote_session = s_is_remote_session
        MOD_AUTH._xai_oauth_device_code_login = s_device_login
        MOD_AUTH._save_xai_oauth_tokens = s_save_tokens
        MOD_AUTH.unsuppress_credential_source = s_unsuppress
    MOD_AUTH.__dict__["_import_error"] = env.get("auth_import_error")
    if env.get("telegram_import_error"):
        MOD_TELEGRAM.__dict__.pop("auto_setup_telegram_bot_result", None)
    else:
        MOD_TELEGRAM.auto_setup_telegram_bot_result = s_auto_telegram
    MOD_TELEGRAM.__dict__["_import_error"] = (
        "telegram_managed_bot unavailable" if env.get("telegram_import_error") else None)

    # setup_quick: ambient sys/os/Path doubles + seamed helpers.
    NS_Q.update(print=s_print, logger=_LOGGER,
                _keep_imported_model=s_keep_imported, _blank_slate_done=s_blank_done)
    NS_Q["sys"] = types.SimpleNamespace(platform=env.get("platform", "win32"))
    NS_Q["os"] = types.SimpleNamespace(listdir=s_listdir)
    _home = env.get("home", "/h")

    def _path_double(p):
        return Path(p)

    _path_double.home = lambda: Path(_home)
    NS_Q["Path"] = _path_double

    # setup_tts: the _setup module double + seamed step helpers.
    NS_T.update(print=s_print, logger=_LOGGER, _setup=_setup_double(),
                tool_backend_helpers=types.SimpleNamespace(
                    managed_nous_tools_enabled=s_managed_nous),
                _tts_local_install_step=s_tts_local,
                _tts_api_key_step=s_tts_api, _tts_xai_step=s_tts_xai)

    # setup_migration: summary lookup is seamed; print is recorded.
    NS_M.update(print=s_print, logger=_LOGGER,
                _get_section_config_summary=s_get_section_summary)
    NS_P.update(print=s_print, logger=_LOGGER)


def _action_stub(aname):
    def action():
        return call(aname, {})
    return action


def nav_from(spec):
    if not isinstance(spec, dict):
        return None
    st = NS_S["_SetupNavigationState"]()
    st.section_index = spec.get("section_index", -1)
    st.prompt_index = spec.get("prompt_index", 0)
    st.active_prompt_index = spec.get("active_prompt_index", -1)
    st.resolved_choices = copy.deepcopy(spec.get("resolved_choices") or [])
    st.replay_choices = copy.deepcopy(spec.get("replay_choices") or [])
    return st


def nav_norm(st):
    if st is None:
        return None
    return {"section_index": st.section_index, "prompt_index": st.prompt_index,
            "active_prompt_index": st.active_prompt_index,
            "resolved_choices": st.resolved_choices, "replay_choices": st.replay_choices}


def run_live(func, case, config):
    args = case.get("args") or {}
    hh = args.get("hermes_home", "/h")
    if func == "handle_setup_menu_navigation":
        ev = NS_S["MenuNavigationEvent"](args.get("event", "begin"))
        return NS_S["_handle_setup_menu_navigation"](ev, args.get("value"))
    if func == "run_setup_steps":
        steps = [(lbl, _action_stub(an)) for lbl, an in args.get("steps") or []]
        return NS_S["_run_setup_steps"](steps)
    if func == "run_setup_action_with_navigation":
        return NS_S["run_setup_action_with_navigation"](
            args.get("label", "Section"), _action_stub(args.get("action", "act")),
            cancelled_message=args.get("cancelled_message", "Setup cancelled."))
    if func == "setup_model_provider":
        return NS_S["setup_model_provider"](config, quick=args.get("quick", False))
    if func == "setup_tools":
        return NS_S["setup_tools"](config, args.get("first_install", False))
    if func == "setup_telemetry":
        return NS_S["setup_telemetry"](config)
    if func == "skip_configured_section":
        return NS_M["_skip_configured_section"](config, args.get("section_key", "model"),
                                                args.get("label", "Model"))
    if func == "run_xai_oauth_login_from_setup":
        return NS_T["_run_xai_oauth_login_from_setup"]()
    if func == "setup_telegram_auto_result":
        return NS_P["_setup_telegram_auto_result"]()
    if func == "print_macos_fda_tip":
        return NS_Q["_print_macos_fda_tip"]()
    if func == "blank_slate_minimal_toolsets":
        return NS_Q["_blank_slate_minimal_toolsets"](config)
    if func == "blank_slate_minimize_config":
        return NS_Q["_blank_slate_minimize_config"](config)
    if func == "run_portal_one_shot":
        return NS_Q["_run_portal_one_shot"](config)
    if func == "run_first_time_quick_setup":
        return NS_Q["_run_first_time_quick_setup"](config, hh, args.get("is_existing", False),
                                                   args.get("imported_from", ""))
    if func == "run_blank_slate_setup":
        return NS_Q["_run_blank_slate_setup"](config, hh, args.get("is_existing", False))
    if func == "blank_slate_walkthrough":
        return NS_Q["_blank_slate_walkthrough"](config, hh)
    if func == "setup_tts_provider":
        return NS_T["_setup_tts_provider"](config)
    raise KeyError("unknown func: %s" % func)


def norm_ret(r):
    if isinstance(r, NS_S["MenuNavigationStart"]):
        d = {"allow_back": r.allow_back}
        if r.should_replay:
            d["replay_value"] = r.replay_value
        return d
    return r


def compute_case(rec):
    func = rec.get("func", "")
    case = rec.get("in") or {}
    install_case(case)
    cfg = case.get("config")
    config = copy.deepcopy(cfg) if isinstance(cfg, dict) else None
    cv = NS_S["_SETUP_NAVIGATION"]
    token = cv.set(nav_from(case.get("nav")))
    try:
        try:
            ret = norm_ret(run_live(func, case, config))
        except _NoScript:
            ret = {"__noscript__": list(NOSCRIPT)}
        except BaseException as e:   # SystemExit included — fail-closed
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(e, NS_S["_SetupGoBack"]):
                ret["prompt_index"] = e.prompt_index
    finally:
        final_nav = nav_norm(cv.get())
        cv.reset(token)
    return {"ret": ret, "trace": TRACE, "config": config, "nav": final_nav,
            "noscript": list(NOSCRIPT)}


def _norm_floats(x):
    """Integral floats compare as ints (libjson serializes them int-like)."""
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: _norm_floats(v) for k, v in x.items()}
    if isinstance(x, list):
        return [_norm_floats(v) for v in x]
    return x


def _canon(x):
    return _norm_floats(json.loads(json.dumps(x)))


def _first_diff(a, b, path="out"):
    if type(a) is not type(b) and not (isinstance(a, (int, float)) and isinstance(b, (int, float))):
        return "%s: type %s != %s" % (path, type(a).__name__, type(b).__name__)
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a:
                return "%s.%s: missing on C side" % (path, k)
            if k not in b:
                return "%s.%s: missing on PY side" % (path, k)
            d = _first_diff(a[k], b[k], "%s.%s" % (path, k))
            if d:
                return d
        return None
    if isinstance(a, list):
        if len(a) != len(b):
            return "%s: len %d != %d" % (path, len(a), len(b))
        for i, (x, y) in enumerate(zip(a, b)):
            d = _first_diff(x, y, "%s[%d]" % (path, i))
            if d:
                return d
        return None
    if a != b:
        return "%s: %r != %r" % (path, a, b)
    return None


def main():
    n = 0
    mismatches = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        n += 1
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print("MISMATCH case %d: harness line not JSON: %s" % (n, e))
            continue
        func = rec.get("func", "?")
        got = rec.get("out")
        if not isinstance(got, dict):
            mismatches += 1
            print("MISMATCH %s case %d: harness emitted no out object" % (func, n))
            continue
        try:
            want = compute_case(rec)
        except Exception as e:
            mismatches += 1
            print("MISMATCH %s case %d: oracle internal error: %r" % (func, n, e))
            continue
        g = _canon(got)
        w = _canon(want)
        if g.get("noscript") or w.get("noscript"):
            mismatches += 1
            print("MISMATCH %s case %d: unscripted seam C=%s PY=%s" %
                  (func, n, g.get("noscript"), w.get("noscript")))
            continue
        if g != w:
            mismatches += 1
            d = _first_diff(g, w) or "values differ"
            print("MISMATCH %s case %d: %s" % (func, n, d))
    print("ORACLE setup_revive: %d cases, %d mismatches" % (n, mismatches))
    if n == 0:
        sys.exit(2)   # fail-closed: an empty run is not a pass
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
