#!/usr/bin/env python3
"""Oracle for the hermes_cli/main.py FINAL UNCLAIMED-remainder FLOW port
(src/cli/main_cli_flow3.c).

Drives the LIVE main.py defs with SCRIPTED seams: every collaborator (claimed
siblings, foreign-module imports, os/sys reads, print, logger, path methods,
argparse builder registrations) is a recording stub fed the same scripts the
C harness feeds mc3_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events), the canonical return ("ret") and the post-call
out-state ("state" — always {} here; env writes ride seams). main composes
the ported _build_cli_parser REAL (AST-extracted into one namespace), matching
the C port's internal composition; all other claimed siblings are seamed.

Targets: _build_cli_parser, main (top-level) + the nested closures
_skills_dir_is_unseeded / _skills_sync_bg (in _start_chat_background_prefetch),
_cmd (in _forward_command, free vars module/attr/forward_return bound as
per-case globals), _custody_refusal (in cmd_update).

Live defs are AST-extracted from the quarry (annotations kept as strings via
the __future__ flag); nested FunctionDefs are located by walking EVERY
top-level def body (they live in claimed parents). The heavy quarry import
chain is never imported. Every exec namespace is a registered sys.modules
module (py3.14). Fail-closed: 0 cases => exit 1; a missing seam script raises
RuntimeError("no script for X"); any harness/oracle exception per case rides
as {"__raise__": cls} (BaseException catch, so SystemExit/KeyboardInterrupt/
GeneratorExit propagate). Summary: 'ORACLE main_cli_flow3: N cases, M mismatches'.

Representation contract (both sides, see src/cli/main_cli_flow3.c):
- parser/subparsers/chat_parser/args/custody/module doubles are
  types.SimpleNamespace records, ALWAYS truthy when present (even {}); plain
  dict inputs (container_info) use JSON truthiness. tuple -> array; None ->
  null; integral floats collapse to ints; Path -> POSIX str; uncaught
  exception -> {"__raise__": cls}; an exception passed as a logger arg
  projects to {"__raise__": cls, "msg": str(e)}.
- the args double's "func" field is a STRING handler marker materialized to a
  recording callable; norm() drops callables from SimpleNamespace echoes, so
  seam args carrying args omit a STRING func (present-null func survives as
  null; matches C mc3_arg_args).
- scripts (FAIL-CLOSED): absent key -> RuntimeError("no script for <name>");
  present null -> None; {"$seq": [...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg": m}
  raises (trace ret is the WHOLE marker); any other value reused. None-valued
  arg keys dropped on both sides.
- env.import_fails gates in-function imports (sys.modules[full]=None AND
  parent.attr=None -> ModuleNotFoundError "import of <name> halted; None in
  sys.modules" — the py3.14 message). env._stdio_repaired models the
  `from hermes_cli import _stdio_repaired` attribute read. env.argv feeds
  sys.argv[1:].
- fixture domain: ASCII operands; home a normalized absolute POSIX path;
  path.rglob scripts are lists; import_module dicts lack the forwarded attr;
  build_top_level_parser scripts are 3-lists (or null / short / long lists to
  exercise the unpack raises); rc is int/bool/str/null/non-integral float;
  exception markers name builtins constructible with one str message.
"""
import __future__
import ast
import builtins
import json
import logging as _real_logging
import os
import sys
import types
from pathlib import Path, PurePath, PurePosixPath

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "main.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "main.py")
# scrubbed-env safety: the quarry chain can read Path.home(); point it at scratch.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "mc3_home"))

NAME = "main_cli_flow3"

PORTED = [
    "_skills_dir_is_unseeded", "_skills_sync_bg", "_cmd", "_custody_refusal",
    "_build_cli_parser", "main",
]
WANTED_TOP = {"_build_cli_parser", "main"}
WANTED_NESTED = {"_skills_dir_is_unseeded", "_skills_sync_bg", "_cmd", "_custody_refusal"}
WANTED = WANTED_TOP | WANTED_NESTED

_real_logging.disable(_real_logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef):
        if _node.name in WANTED_TOP:
            DEF_CODE[_node.name] = _compile_node(_node, SRC)
        # nested closures live in CLAIMED parents — walk every top-level body
        for _child in ast.walk(_node):
            if isinstance(_child, ast.FunctionDef) and _child.name in WANTED_NESTED:
                DEF_CODE[_child.name] = _compile_node(_child, SRC)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PathDouble):
        return v.s
    if isinstance(v, PurePath):
        return str(v)
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams (FAIL-CLOSED) ────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    if name not in scripts.io:
        marker = {"__raise__": "RuntimeError", "msg": f"no script for {name}"}
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


def _rec(name, argmap):
    def f(*a, **kw):
        return record(name, argmap(a, kw))
    return f


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


# ── object doubles ──────────────────────────────────────────────────────────
class PathDouble:
    """POSIX path double: / is pure join; filesystem methods ride seams."""
    def __init__(self, s):
        if not isinstance(s, (str, PurePath)):
            raise TypeError("argument should be a str or an os.PathLike object "
                            f"where __fspath__ returns a str, not '{type(s).__name__}'")
        self.s = str(s)

    def __truediv__(self, other):
        return PathDouble(str(PurePosixPath(self.s) / str(other)))

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def is_dir(self):
        return record("path.is_dir", {"path": self.s})

    def rglob(self, pattern):
        r = record("path.rglob", {"path": self.s, "pattern": pattern})
        return iter(r) if isinstance(r, list) else r


class ParserDouble(types.SimpleNamespace):
    def set_defaults(self, **kw):
        f = kw.get("func")
        return record("parser.set_defaults",
                      {"func": getattr(f, "__name__", str(f))})

    def print_help(self):
        return record("parser.print_help", {})


def mk_parser(script):
    if isinstance(script, dict):
        return ParserDouble(**script)
    return script


class ArgsDouble(types.SimpleNamespace):
    pass


class FnDouble:
    def __init__(self, name, seam, argmap):
        self.__name__ = name
        self._seam = seam
        self._argmap = argmap

    def __call__(self, *a, **kw):
        return record(self._seam, self._argmap(a, kw))


def mk_args_double(script):
    if not isinstance(script, dict):
        return script
    d = dict(script)
    if isinstance(d.get("func"), str):
        d["func"] = FnDouble(d["func"], "args.func", lambda a, k: {"args": a[0]})
    return ArgsDouble(**d)


def mk_args(d):
    return types.SimpleNamespace(**(d or {}))


class NamedFn:
    def __init__(self, name):
        self.__name__ = name

    def __call__(self, *a, **kw):
        raise AssertionError(f"NamedFn {self.__name__} should not be called")


class ForwardFn:
    def __init__(self, attr):
        self.attr = attr
        self.__name__ = attr

    def __call__(self, args):
        return record("forward.call", {"attr": self.attr, "args": args})


class ModDouble:
    def __init__(self, d):
        self.__dict__.update(d or {})

    def __getattr__(self, attr):
        return ForwardFn(attr)


class CustodyDouble(types.SimpleNamespace):
    def refusal_notice(self):
        return record("custody.refusal_notice", {})


class ModulesDouble:
    def get(self, name, default=None):
        return record("sys.modules.get", {"name": name})


class LoggerDouble:
    def _log(self, level, msg, args, kw):
        d = {"msg": msg, "args": list(args)}
        if kw.get("exc_info"):
            d["exc_info"] = True
        return record("logger." + level, d)

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args, kw)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args, kw)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args, kw)


_LOGGER_SINGLETON = LoggerDouble()


class LoggingModule(types.ModuleType):
    def getLogger(self, name=None):
        return _LOGGER_SINGLETON


_LOGGING_DOUBLE = LoggingModule("logging")


class EnvironDouble:
    def get(self, name, default=None):
        return record("os.environ.get", {"name": name, "default": default})

    def setdefault(self, name, value):
        return record("os.environ.setdefault", {"name": name, "value": value})

    def __setitem__(self, key, value):
        return record("os.environ.set", {"key": key, "value": value})


class OsDouble:
    def __init__(self):
        self.environ = EnvironDouble()


class StderrDouble:
    def write(self, text):
        return record("sys.stderr.write", {"text": text})


class SysDouble:
    def __init__(self):
        self.argv = ["hermes"]
        self.stderr = StderrDouble()
        self.modules = ModulesDouble()

    def exit(self, code=None):
        record("sys.exit", {"code": code})
        raise SystemExit(code)


def materialize(name, script):
    if script is None:
        return None
    if name == "build_top_level_parser":
        if isinstance(script, (list, tuple)):
            return tuple(mk_parser(x) for x in script)
        return script
    if name in ("kanban.build_parser", "projects.build_parser"):
        return mk_parser(script)
    if name == "_parse_cli_args":
        return mk_args_double(script)
    if name == "import_module":
        return ModDouble(script if isinstance(script, dict) else {})
    if name == "sys.modules.get":
        return CustodyDouble(**script) if isinstance(script, dict) else script
    if name == "path.rglob":
        return iter(script) if isinstance(script, list) else script
    return script


# ── seam arg projections (SeamFn-installed collaborators) ───────────────────
SEAM_ARGS = {
    "get_hermes_home": lambda a, k: {},
    "_sync_bundled_skills_for_startup": lambda a, k: {},
    "import_module": lambda a, k: {"module": a[0]},
    "sys.modules.get": lambda a, k: {"name": a[0]},
    "custody.refusal_notice": lambda a, k: {},
    "build_top_level_parser": lambda a, k: {},
    "lsp.register_subparser": lambda a, k: {"subparsers": a[0]},
    "register_send_subparser": lambda a, k: {"subparsers": a[0]},
    "build_peer_parser": lambda a, k: {"subparsers": a[0]},
    "portal.add_parser": lambda a, k: {"subparsers": a[0]},
    "kanban.build_parser": lambda a, k: {"subparsers": a[0]},
    "projects.build_parser": lambda a, k: {"subparsers": a[0]},
    "_register_plugin_cli_commands": lambda a, k: {"subparsers": a[0]},
    "_set_process_title": lambda a, k: {},
    "_warn_if_unsupervised_pid1": lambda a, k: {},
    "_advertise_agent_env": lambda a, k: {},
    "configure_windows_stdio": lambda a, k: {},
    "install_truststore": lambda a, k: {},
    "_cleanup_quarantined_exes": lambda a, k: {},
    "_sweep_stale_bytecode_if_checkout_changed": lambda a, k: {},
    "_recover_update_debts_on_startup": lambda a, k: {},
    "_first_positional_argv": lambda a, k: {},
    "install_root": lambda a, k: {},
    "maybe_run_boot_bootstrap": lambda a, k: {"root": a[0]},
    "check_runtime": lambda a, k: {"root": a[0]},
    "print": lambda a, k: {"text": a[0], "file": "stderr" if k.get("file") is not None else None},
    "_try_termux_fast_tui_launch": lambda a, k: {},
    "_try_termux_fast_cli_launch": lambda a, k: {},
    "_try_fast_serve_launch": lambda a, k: {},
    "_try_fast_chat_launch": lambda a, k: {},
    "get_container_exec_info": lambda a, k: {},
    "exec_in_container": lambda a, k: {"info": a[0], "argv": a[1]},
    "_parse_cli_args": lambda a, k: {"parser": a[0], "subparsers": a[1], "argv": a[2]},
    "_enable_cpu_profile": lambda a, k: {"args": a[0]},
    "cmd_version": lambda a, k: {"args": a[0]},
    "_prepare_agent_startup": lambda a, k: {"args": a[0]},
    "_run_oneshot_from_args": lambda a, k: {"args": a[0]},
    "set_process_surface": lambda a, k: {"surface": a[0]},
    "_default_to_chat": lambda a, k: {"args": a[0]},
}

# builder registrations: (seam name, [(kwarg, handler-name-marker), ...])
BUILDERS = [
    ("build_model_parser", [("cmd_model", "cmd_model")]),
    ("build_moa_parser", []),
    ("build_fallback_parser", []),
    ("build_worktree_parser", []),
    ("build_browser_parser", []),
    ("build_secrets_parser", []),
    ("build_egress_parser", []),
    ("build_migrate_parser", []),
    ("build_codex_runtime_parser", []),
    ("build_gateway_parser", [("cmd_gateway", "cmd_gateway"), ("cmd_proxy", "cmd_proxy"),
                              ("cmd_gateway_enroll", "cmd_gateway_enroll")]),
    ("build_setup_parser", [("cmd_setup", "cmd_setup")]),
    ("build_whatsapp_parser", [("cmd_whatsapp", "cmd_whatsapp")]),
    ("build_whatsapp_cloud_parser", [("cmd_whatsapp_cloud", "cmd_whatsapp_cloud")]),
    ("build_slack_parser", [("cmd_slack", "cmd_slack")]),
    ("build_login_parser", [("cmd_login", "cmd_login")]),
    ("build_logout_parser", [("cmd_logout", "cmd_logout")]),
    ("build_auth_parser", [("cmd_auth", "cmd_auth")]),
    ("build_status_parser", [("cmd_status", "cmd_status")]),
    ("build_pause_parser", []),
    ("build_cron_parser", [("cmd_cron", "cmd_cron")]),
    ("build_webhook_parser", [("cmd_webhook", "cmd_webhook")]),
    ("build_hooks_parser", [("cmd_hooks", "cmd_hooks")]),
    ("build_doctor_parser", [("cmd_doctor", "cmd_doctor")]),
    ("build_verify_parser", [("cmd_verify", "cmd_verify")]),
    ("build_security_parser", [("cmd_security", "cmd_security")]),
    ("build_approvals_parser", [("cmd_approvals", "cmd_approvals")]),
    ("build_dump_parser", [("cmd_dump", "cmd_dump")]),
    ("build_debug_parser", [("cmd_debug", "cmd_debug")]),
    ("build_backup_parser", [("cmd_backup", "cmd_backup")]),
    ("build_checkpoints_parser", []),
    ("build_import_cmd_parser", [("cmd_import", "cmd_import")]),
    ("build_config_parser", [("cmd_config", "cmd_config")]),
    ("build_skin_parser", [("cmd_skin", "cmd_skin")]),
    ("build_console_parser", [("cmd_console", "cmd_console")]),
    ("build_pairing_parser", [("cmd_pairing", "cmd_pairing")]),
    ("build_skills_parser", [("cmd_skills", "cmd_skills")]),
    ("build_bundles_parser", []),
    ("build_plugins_parser", [("cmd_plugins", "cmd_plugins")]),
    ("build_curator_parser", []),
    ("build_pets_parser", []),
    ("build_clipboard_parser", []),
    ("build_journey_parser", []),
    ("build_memory_parser", [("cmd_memory", "cmd_memory")]),
    ("build_tools_parser", [("cmd_tools", "cmd_tools")]),
    ("build_computer_use_parser", []),
    ("build_mcp_parser", [("cmd_mcp", "cmd_mcp")]),
    ("build_connectors_parser", []),
    ("build_sessions_parser", [("cmd_sessions", "cmd_sessions_lazy")]),
    ("build_insights_parser", [("cmd_insights", "cmd_insights")]),
    ("build_usage_parser", []),
    ("build_monitoring_parser", [("cmd_monitoring", "cmd_monitoring")]),
    ("build_vault_parser", []),
    ("build_update_parser", [("cmd_update", "cmd_update")]),
    ("build_uninstall_parser", [("cmd_uninstall", "cmd_uninstall")]),
    ("build_acp_parser", [("cmd_acp", "cmd_acp")]),
    ("build_profile_parser", [("cmd_profile", "cmd_profile")]),
    ("build_completion_parser", [("cmd_completion", "cmd_completion")]),  # + parser kwarg
    ("build_dashboard_parser", [("cmd_dashboard", "cmd_dashboard"),
                                ("cmd_dashboard_register", "cmd_dashboard_register")]),
    ("build_webapp_parser", [("cmd_webapp", "cmd_webapp")]),
    ("build_gui_parser", [("cmd_gui", "cmd_gui")]),
    ("build_logs_parser", [("cmd_logs", "cmd_logs")]),
    ("build_prompt_size_parser", [("cmd_prompt_size", "cmd_prompt_size")]),
]


def _builder_argmap(kws, with_parser=False):
    def am(a, k):
        d = {"subparsers": a[0]}
        for kw, _ in kws:
            f = k[kw]
            d[kw] = getattr(f, "__name__", str(f))
        if with_parser:
            d["parser"] = k["parser"]
        return d
    return am


for _nm, _kws in BUILDERS:
    SEAM_ARGS[_nm] = _builder_argmap(_kws, with_parser=(_nm == "build_completion_parser"))


# ── stub modules for lazy imports ───────────────────────────────────────────
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# gate registry: full module name -> (module, parent, attr-on-parent)
GATE_MODULES = {}


def _link(name, mod, parent, attr):
    GATE_MODULES[name] = (mod, parent, attr)
    if parent is not None:
        setattr(parent, attr, mod)


def install_stubs():
    hc = _mod("hermes_cli")
    hc.__path__ = []
    hc._stdio_repaired = False
    cfg = _mod("hermes_cli.config", get_hermes_home=SeamFn("get_hermes_home"),
               get_container_exec_info=SeamFn("get_container_exec_info"))
    _link("hermes_cli.config", cfg, hc, "config")
    prs = _mod("hermes_cli._parser", build_top_level_parser=SeamFn("build_top_level_parser"))
    _link("hermes_cli._parser", prs, hc, "_parser")
    stdio = _mod("hermes_cli.stdio", configure_windows_stdio=SeamFn("configure_windows_stdio"))
    _link("hermes_cli.stdio", stdio, hc, "stdio")
    boot = _mod("hermes_cli.boot_bootstrap", maybe_run_boot_bootstrap=SeamFn("maybe_run_boot_bootstrap"))
    _link("hermes_cli.boot_bootstrap", boot, hc, "boot_bootstrap")
    venv = _mod("hermes_cli.venv_sync", check_runtime=SeamFn("check_runtime"))
    _link("hermes_cli.venv_sync", venv, hc, "venv_sync")
    cont = _mod("hermes_cli.main_container", exec_in_container=SeamFn("exec_in_container"))
    _link("hermes_cli.main_container", cont, hc, "main_container")
    obs = _mod("hermes_cli.observability")
    obs.__path__ = []
    _link("hermes_cli.observability", obs, hc, "observability")
    smd = _mod("hermes_cli.observability.shared_metrics_disabled",
               set_process_surface=SeamFn("set_process_surface"))
    _link("hermes_cli.observability.shared_metrics_disabled", smd, obs, "shared_metrics_disabled")
    send = _mod("hermes_cli.send_cmd", register_send_subparser=SeamFn("register_send_subparser"))
    _link("hermes_cli.send_cmd", send, hc, "send_cmd")
    subs = _mod("hermes_cli.subcommands")
    subs.__path__ = []
    _link("hermes_cli.subcommands", subs, hc, "subcommands")
    peer = _mod("hermes_cli.subcommands.peer", build_peer_parser=SeamFn("build_peer_parser"))
    _link("hermes_cli.subcommands.peer", peer, subs, "peer")
    portal = _mod("hermes_cli.portal_cli", add_parser=SeamFn("portal.add_parser"))
    _link("hermes_cli.portal_cli", portal, hc, "portal_cli")
    kanban = _mod("hermes_cli.kanban", build_parser=SeamFn("kanban.build_parser"))
    _link("hermes_cli.kanban", kanban, hc, "kanban")
    projects = _mod("hermes_cli.projects_cmd", build_parser=SeamFn("projects.build_parser"))
    _link("hermes_cli.projects_cmd", projects, hc, "projects_cmd")
    agent = _mod("agent")
    agent.__path__ = []
    lsp = _mod("agent.lsp")
    lsp.__path__ = []
    _link("agent.lsp", lsp, agent, "lsp")
    lspcli = _mod("agent.lsp.cli", register_subparser=SeamFn("lsp.register_subparser"))
    _link("agent.lsp.cli", lspcli, lsp, "cli")
    sslv = _mod("agent.ssl_verify", install_truststore=SeamFn("install_truststore"))
    _link("agent.ssl_verify", sslv, agent, "ssl_verify")
    pm = _mod("pm")
    pm.__path__ = []
    pmpaths = _mod("pm.paths", install_root=SeamFn("install_root"))
    _link("pm.paths", pmpaths, pm, "paths")
    _mod("importlib", import_module=SeamFn("import_module"))
    _link("importlib", sys.modules["importlib"], None, None)
    sys.modules["logging"] = _LOGGING_DOUBLE


# ── base namespace ──────────────────────────────────────────────────────────
_OS_DOUBLE = OsDouble()
_SYS_DOUBLE = SysDouble()
_BASE_NS = {
    "__name__": "mc3_live_main_cli",
    "__builtins__": builtins,
    "logging": _LOGGING_DOUBLE,
    "logger": _LOGGER_SINGLETON,
    "os": _OS_DOUBLE,
    "sys": _SYS_DOUBLE,
    "Path": PathDouble,
    "print": SeamFn("print"),
}
_MOD = types.ModuleType("mc3_live_main_cli")
_MOD.__dict__.update(_BASE_NS)
sys.modules["mc3_live_main_cli"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)

# claimed siblings called by the ported flows -> SeamFns
_COLLABS = [
    "_register_plugin_cli_commands", "_set_process_title", "_warn_if_unsupervised_pid1",
    "_advertise_agent_env", "_cleanup_quarantined_exes",
    "_sweep_stale_bytecode_if_checkout_changed", "_recover_update_debts_on_startup",
    "_first_positional_argv", "_try_termux_fast_tui_launch", "_try_termux_fast_cli_launch",
    "_try_fast_serve_launch", "_try_fast_chat_launch", "_parse_cli_args",
    "_enable_cpu_profile", "cmd_version", "_prepare_agent_startup",
    "_run_oneshot_from_args", "_default_to_chat", "_sync_bundled_skills_for_startup",
]
for _nm in _COLLABS:
    _BASE_NS[_nm] = SeamFn(_nm)
# module-top-imported builders -> SeamFns (bare globals in the exec namespace)
for _nm, _kws in BUILDERS:
    _BASE_NS[_nm] = SeamFn(_nm)
# claimed handlers passed as VALUES to the builders (projected by __name__)
_HANDLERS = [
    "cmd_chat", "cmd_model", "cmd_gateway", "cmd_proxy", "cmd_gateway_enroll",
    "cmd_setup", "cmd_whatsapp", "cmd_whatsapp_cloud", "cmd_slack", "cmd_login",
    "cmd_logout", "cmd_auth", "cmd_status", "cmd_cron", "cmd_webhook", "cmd_kanban",
    "cmd_project", "cmd_hooks", "cmd_doctor", "cmd_verify", "cmd_security",
    "cmd_approvals", "cmd_dump", "cmd_debug", "cmd_backup", "cmd_import",
    "cmd_config", "cmd_skin", "cmd_console", "cmd_pairing", "cmd_skills",
    "cmd_plugins", "cmd_memory", "cmd_tools", "cmd_mcp", "cmd_sessions_lazy",
    "cmd_insights", "cmd_monitoring", "cmd_update", "cmd_uninstall", "cmd_acp",
    "cmd_profile", "cmd_completion", "cmd_dashboard", "cmd_dashboard_register",
    "cmd_webapp", "cmd_gui", "cmd_logs", "cmd_prompt_size",
]
for _nm in _HANDLERS:
    _BASE_NS[_nm] = NamedFn(_nm)
install_stubs()


def _set_import_gates(env):
    fails = set(env.get("import_fails") or [])
    for name, (mod, parent, attr) in GATE_MODULES.items():
        if name in fails:
            sys.modules[name] = None
            if parent is not None:
                setattr(parent, attr, None)
        else:
            sys.modules[name] = mod
            if parent is not None:
                setattr(parent, attr, mod)


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    _SYS_DOUBLE.argv = ["hermes"] + list(env.get("argv") or [])
    sys.modules["hermes_cli"]._stdio_repaired = bool(env.get("_stdio_repaired", False))
    _set_import_gates(env)
    return _BASE_NS, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────
def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_skills_dir_is_unseeded":
        return ns[func]()
    if func == "_skills_sync_bg":
        ns[func]()
        return None
    if func == "_cmd":
        # the closure's free vars (module/attr/forward_return) bind as globals
        ns["module"] = inp.get("module", "")
        ns["attr"] = inp.get("attr", "")
        ns["forward_return"] = inp.get("forward_return", False)
        return ns[func](mk_args(inp.get("args")))
    if func == "_custody_refusal":
        return ns[func]()
    if func == "_build_cli_parser":
        return ns[func]()
    if func == "main":
        ns[func]()
        return None
    raise KeyError(f"unknown func {func}")


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace, "state": holder.get("state", {})}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace,
                       "state": holder.get("state", {})}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag drive failures / suspicious raises ──
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    names = set(PORTED)
    fx_funcs = {r["func"] for r in fixtures}
    if fx_funcs != names:
        print(f"SELFTEST {NAME}: func set drift: missing={sorted(names - fx_funcs)} extra={sorted(fx_funcs - names)}")
        bad += 1
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported fn: {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "main_cli_flow3.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()