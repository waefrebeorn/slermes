#!/usr/bin/env python3
"""Oracle for the TUI plugin-method FLOW port
(src/gateway/tui_methods_plugins_flow.c).

The quarry has NO tui_gateway/methods_plugins.py; the live defs come from the
real defining modules (methods_*.py plugin family):
  methods_plugin_catalog.py        _ (plugins.catalog.lookup), register
  methods_tools_plugin_commands.py _plugin_command_handler, _run_plugin_command,
                                   _dispatch_plugin_in_profile_scope, register
  methods_tools_plugin_servers.py  _plugin_server_rows, _server_identity,
                                   _portable_server_tools, _plugin_tool_ownership,
                                   register
  methods_tools.py                 _tools_mod, _is_registry_command,
                                   _dispatch_plugin, refresh_plugin_sessions,
                                   _plugin_rows, _session_home_scope,
                                   _refresh_live_sessions

Drives the LIVE defs with SCRIPTED seams: every server global (_ok/_str_arg/
_sessions/_sessions_lock/...), deferred module import (_tools_mod), module
method, in-function import callable and ported sibling flow is a recording
stub fed the same scripts the C harness feeds its universal seam. Compares
the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace"),
the final return ("ret") and the module-global mirror ("state" — echoed).

Live defs are AST-extracted (module Assign/AnnAssign constants seeded,
plain-Name targets only; UNDECORATED FunctionDefs exec'd with the
future-annotations flag into registered sys.modules modules — py3.14). The
decorated defs (_ catalog handler, _session_home_scope) are exec'd WITHOUT
their decorators: the handler body runs directly; the home-scope generator
is driven next()+close() (enter phase + finally resets). The heavy quarry
import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam
script is a recorded RuntimeError raise on BOTH sides. Per-case wrapper
catches BaseException (SystemExit included). Summary:
'ORACLE tui_methods_plugins_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception / suppress site.
- Scripted raises of plain kinds carry str(exc) == message verbatim; real
  builtin exception instances are materialized.
- tools_mod scripts return the module NAME string; it resolves to a
  SimpleNamespace of recording stubs. A wrong marker raises AttributeError
  on the first attribute access. Module DATA attributes (toolsets) come
  from io["toolsets.data"] WITHOUT a trace event.
- CM doubles are SILENT (construction seam only; enter/exit untraced);
  _sessions_lock is silent; _sessions.items() is the sessions_items seam.
- Agent doubles: session["agent"] dicts materialize to SimpleNamespaces;
  seam args project them back to the raw dict. Server doubles carry
  id/session; status doubles state/availability; package doubles manifest.
- Tuples project to lists; sets to sorted unique lists; integral floats to
  ints; Paths to POSIX strings. PurePosixPath is seeded as Path so str(Path)
  keeps forward slashes on Windows; fixture paths are POSIX, normalized.
- str()/strip() fold the ASCII fixture corpus only; str()-projected values
  are strings/ints/bools/null (no floats).
- Seam args drop None-valued TOP-LEVEL keys on both sides.
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path, PurePosixPath

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC_TOOLS = os.path.join(Q, "tui_gateway", "methods_tools.py")
SRC_CMDS = os.path.join(Q, "tui_gateway", "methods_tools_plugin_commands.py")
SRC_SERVERS = os.path.join(Q, "tui_gateway", "methods_tools_plugin_servers.py")
SRC_CATALOG = os.path.join(Q, "tui_gateway", "methods_plugin_catalog.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node, skip_decorators=False):
    start = node.lineno
    if not skip_decorators:
        for d in getattr(node, "decorator_list", []):
            start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path, skip_decorators=False):
    text = textwrap.dedent(seg(src, node, skip_decorators))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


# ── namespaces (live defs + seeded constants) ──
def extract():
    import contextlib
    import threading
    nss = {}
    # methods_tools.py — undecorated defs + the stripped home-scope generator
    mod = new_module("tui_gateway.methods_tools")
    ns = mod.__dict__
    ns.update({"contextlib": contextlib, "sys": sys, "os": os, "re": re,
               "json": json, "threading": threading, "Path": PurePosixPath})
    src = open(SRC_TOOLS, encoding="utf-8").read()
    seed_constants(src, ns, SRC_TOOLS)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef):
            if not node.decorator_list:
                exec_node(src, node, ns, SRC_TOOLS)
            elif node.name == "_session_home_scope":
                exec_node(src, node, ns, SRC_TOOLS, skip_decorators=True)
    nss["tools"] = ns
    # methods_tools_plugin_commands.py
    mod = new_module("tui_gateway.methods_tools_plugin_commands")
    ns = mod.__dict__
    ns.update({"contextlib": contextlib, "Path": PurePosixPath})
    src = open(SRC_CMDS, encoding="utf-8").read()
    seed_constants(src, ns, SRC_CMDS)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            exec_node(src, node, ns, SRC_CMDS)
    nss["cmds"] = ns
    # methods_tools_plugin_servers.py
    mod = new_module("tui_gateway.methods_tools_plugin_servers")
    ns = mod.__dict__
    ns.update({"Path": PurePosixPath})
    src = open(SRC_SERVERS, encoding="utf-8").read()
    seed_constants(src, ns, SRC_SERVERS)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            exec_node(src, node, ns, SRC_SERVERS)
    nss["servers"] = ns
    # methods_plugin_catalog.py — the decorated anonymous handler, stripped
    mod = new_module("tui_gateway.methods_plugin_catalog")
    ns = mod.__dict__
    src = open(SRC_CATALOG, encoding="utf-8").read()
    seed_constants(src, ns, SRC_CATALOG)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef):
            if node.name == "_":
                exec_node(src, node, ns, SRC_CATALOG, skip_decorators=True)
            elif not node.decorator_list:
                exec_node(src, node, ns, SRC_CATALOG)
    nss["catalog"] = ns
    required = {
        "tools": ["_tools_mod", "_is_registry_command", "_dispatch_plugin",
                  "refresh_plugin_sessions", "_plugin_rows",
                  "_session_home_scope", "_refresh_live_sessions"],
        "cmds": ["_plugin_command_handler", "_run_plugin_command",
                 "_dispatch_plugin_in_profile_scope", "register"],
        "servers": ["_plugin_server_rows", "_server_identity",
                    "_portable_server_tools", "_plugin_tool_ownership",
                    "register"],
        "catalog": ["_", "register"],
    }
    for area, names in required.items():
        missing = [k for k in names if k not in nss[area]]
        if missing:
            print(f"FATAL oracle_tui_methods_plugins_flow: {area} missing: {missing}")
            sys.exit(2)
    return nss


NSS = extract()

# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "PermissionError": PermissionError, "OSError": OSError, "IndexError": IndexError,
    "StopIteration": StopIteration, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── doubles ──
class SilentCM:
    """Silent context-manager double carrying its script marker."""
    def __init__(self, marker):
        self.__cm_marker__ = marker

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class AgentDouble(types.SimpleNamespace):
    """session['agent'] materialization; projects back to the raw dict."""
    pass


def agent_proj(a):
    if isinstance(a, AgentDouble):
        return a.__agent_raw__
    return a


def session_proj(s):
    if isinstance(s, dict):
        out = dict(s)
        if "agent" in out:
            out["agent"] = agent_proj(out["agent"])
        return out
    return s


def manager_proj(m):
    return getattr(m, "__mgr_raw__", m)


def mat_agent(agent):
    if isinstance(agent, dict):
        d = AgentDouble(**agent)
        d.__agent_raw__ = agent
        return d
    return agent


def mat_session(sess):
    if isinstance(sess, dict) and isinstance(sess.get("agent"), dict):
        out = dict(sess)
        out["agent"] = mat_agent(out["agent"])
        return out
    return sess


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, AgentDouble):
            return conv(x.__agent_raw__)
        if isinstance(x, types.SimpleNamespace):
            if hasattr(x, "__tmp_module__"):
                return x.__tmp_module__
            if hasattr(x, "__mgr_raw__"):
                return conv(x.__mgr_raw__)
            return conv(vars(x))
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── per-case script backend (mirrors the C tmp_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        # C returns a fresh copy per call; keep the script entry pristine.
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def ns_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def set_build(raw):
    return set(raw) if isinstance(raw, list) else raw


def cm_build(raw):
    return SilentCM(raw)


class DictGetDouble:
    """core._servers / core._lazy_server_tool_names: .get fires a seam."""
    def __init__(self, ctx, seam, build=None):
        self._ctx = ctx
        self._seam = seam
        self._build = build

    def get(self, key):
        raw = self._ctx.call(self._seam, {"key": key})
        return self._build(raw) if self._build else raw


def server_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    attrs = {k: v for k, v in raw.items()}
    attrs["_is_recycled_stdio"] = rec(
        ctx, "server._is_recycled_stdio", lambda: {"server": raw.get("id")})
    return types.SimpleNamespace(**attrs)


def core_double(ctx):
    return types.SimpleNamespace(
        _servers=DictGetDouble(ctx, "core._servers.get",
                               lambda raw: server_build(raw, ctx)),
        _lazy_server_tool_names=DictGetDouble(ctx, "core._lazy_server_tool_names.get"))


def manager_double_plugins(raw, ctx):
    """get_plugin_manager result for _plugin_tool_ownership."""
    if not isinstance(raw, dict):
        return raw
    attrs = {}
    if "_plugins" in raw:
        attrs["_plugins"] = {k: ns_build(v) for k, v in raw["_plugins"].items()}
    d = types.SimpleNamespace(**attrs)
    d.__mgr_raw__ = raw
    return d


def manager_double_tools(raw, ctx):
    """manager argument of _portable_server_tools."""
    d = types.SimpleNamespace(
        get_portable_mcp_servers=rec(ctx, "manager.get_portable_mcp_servers", lambda: {}),
        get_portable_mcp_server_plugins=rec(
            ctx, "manager.get_portable_mcp_server_plugins", lambda: {}))
    d.__mgr_raw__ = raw
    return d


def sessions_items_build(raw):
    if not isinstance(raw, list):
        return raw
    return [[sid, mat_session(sess)] for sid, sess in raw]
# ── module doubles (tools_mod resolves the scripted module name) ──
MODULE_ATTRS = {
    "hermes_cli.plugins": {
        "get_plugin_command_handler": "plugins.get_plugin_command_handler",
        "plugin_command_absent_nowait": "plugins.plugin_command_absent_nowait",
        "resolve_plugin_command_result": "plugins.resolve_plugin_command_result",
        "discover_plugins": "plugins.discover_plugins",
        "get_plugin_manager": "get_plugin_manager",
    },
    "hermes_cli.plugin_command_dispatch": {
        "invoke_plugin_command": "plugin_command_dispatch.invoke_plugin_command"},
    "hermes_cli.plugin_choices": {
        "clarify_asker": "plugin_choices.clarify_asker",
        "bind_asker": "plugin_choices.bind_asker",
    },
    "hermes_cli.agent_plugins": {"load_agent_plugin": "agent_plugins.load_agent_plugin"},
    "hermes_cli.plugins_manifest": {
        "portable_mcp_server_name": "plugins_manifest.portable_mcp_server_name"},
    "tools.mcp_liveness": {
        "status": "mcp_liveness.status", "describe": "mcp_liveness.describe"},
    "tools.mcp_tool_scope": {"_resolve_server_key": "mcp_tool_scope._resolve_server_key"},
    "hermes_platform.declaration": {"lookup": "declaration.lookup"},
    "hermes_cli.plugins_cmd_catalog": {
        "catalog_install_record": "plugins_cmd_catalog.catalog_install_record",
        "catalog_rows_maps": "plugins_cmd_catalog.catalog_rows_maps",
        "catalog_row_fields": "plugins_cmd_catalog.catalog_row_fields",
        "looks_like_catalog_name": "plugins_cmd_catalog.looks_like_catalog_name",
    },
    "tools.mcp_tool_config": {
        "_interpolate_env_vars": "mcp_tool_config._interpolate_env_vars"},
    "hermes_cli.config": {"load_config": "config.load_config"},
    "hermes_cli.plugins_cmd": {
        "_get_enabled_set": "plugins_cmd._get_enabled_set",
        "_get_disabled_set": "plugins_cmd._get_disabled_set",
        "_read_install_metadata": "plugins_cmd._read_install_metadata",
        "_category_active_names": "plugins_cmd._category_active_names",
        "_discover_all_plugins": "plugins_cmd._discover_all_plugins",
        "_plugin_status": "plugins_cmd._plugin_status",
        "_is_portable_plugin_dir": "plugins_cmd._is_portable_plugin_dir",
        "pinned_revision": "plugins_cmd.pinned_revision",
    },
    "hermes_cli.plugins_cadence": {
        "cached_git_updates": "plugins_cadence.cached_git_updates"},
    "hermes_cli.desktop_plugin_halves": {
        "desktop_source": "desktop_plugin_halves.desktop_source"},
    "hermes_cli.plugins_settings": {
        "plugin_settings_fields": "plugins_settings.plugin_settings_fields"},
    "tools.mcp_tool_agent": {
        "refresh_agent_mcp_tools": "mcp_tool_agent.refresh_agent_mcp_tools"},
    "agent.plugin_live_notes": {
        "queue_plugin_notice": "plugin_live_notes.queue_plugin_notice"},
    "hermes_constants": {
        "hermes_home_key": "hermes_constants.hermes_home_key",
        "set_hermes_home_override": "hermes_constants.set_hermes_home_override",
        "reset_hermes_home_override": "hermes_constants.reset_hermes_home_override",
    },
    "agent.runtime_cwd": {
        "set_session_cwd": "agent.runtime_cwd.set_session_cwd",
        "reset_session_cwd": "agent.runtime_cwd.reset_session_cwd",
    },
    "hermes_cli.commands": {"resolve_command": "commands.resolve_command"},
}

ARGS_FN = {
    "importlib.import_module": lambda module: {"module": module},
    "ok": lambda rid, result: {"rid": rid, "result": result},
    "str_arg": lambda params, key: {"params": params, "key": key},
    "bind_module": lambda g, server, skip=(): {
        "module": g.get("__name__"), "server": server, "skip": list(skip)},
    "plugin_catalog.load_catalog_live": lambda: {},
    "plugin_catalog.resolved_removed_entries": lambda: {},
    "plugin_catalog.match_removed": lambda name, removed: {"name": name, "removed": removed},
    "plugins_cmd_catalog.looks_like_catalog_name": lambda name: {"name": name},
    "plugins.get_plugin_command_handler": lambda name: {"name": name},
    "plugins.plugin_command_absent_nowait": lambda name: {"name": name},
    "plugins.resolve_plugin_command_result": lambda result: {"result": result},
    "plugins.discover_plugins": lambda: {},
    "get_plugin_manager": lambda: {},
    "plugin_command_dispatch.invoke_plugin_command":
        lambda handler, arg, **kw: {"handler": handler, "arg": arg, **kw},
    "plugin_choices.clarify_asker": lambda cb: {"callback": "_clarify_block"},
    "plugin_choices.bind_asker": lambda asker: {"asker": asker},
    "set_session_context": lambda key, cwd=None: {"session_key": key, "cwd": cwd},
    "clear_session_context": lambda tokens: {"tokens": tokens},
    "session_source": lambda session: {"session": session_proj(session)},
    "session_home_scope": lambda session, profile=None: {"session": session, "profile": profile},
    "session_profile_runtime_scope": lambda d: {"session": session_proj(d)},
    "profile_home": lambda profile: {"profile": profile},
    "is_registry_command": lambda name: {"name": name},
    "plugin_command_handler": lambda name: {"name": name},
    "run_plugin_command": lambda handler, arg, session=None, sid="": {
        "handler": handler, "arg": arg, "session": session_proj(session), "sid": sid},
    "dispatch_plugin_in_profile_scope":
        lambda name, arg, session, sid="", profile=None: {
            "name": name, "arg": arg, "session": session_proj(session),
            "sid": sid, "profile": profile},
    "commands.resolve_command": lambda base: {"base": base},
    "agent_plugins.load_agent_plugin":
        lambda plugin_dir, root: {"plugin_dir": plugin_dir.as_posix(),
                                  "root": root.as_posix()},
    "plugins_manifest.portable_mcp_server_name": lambda key, server: {"key": key, "server": server},
    "mcp_liveness.status": lambda internal_name: {"internal_name": internal_name},
    "mcp_liveness.describe": lambda decl, availability, state, display_name: {
        "decl": decl, "availability": availability, "state": state,
        "display_name": display_name},
    "declaration.lookup": lambda internal_name: {"internal_name": internal_name},
    "core._servers.get": lambda key: {"key": key},
    "core._lazy_server_tool_names.get": lambda key: {"key": key},
    "mcp_tool_scope._resolve_server_key": lambda server: {"server": server},
    "mcp_tool_config._interpolate_env_vars": lambda cfg: {"cfg": cfg},
    "server_identity": lambda cfg: {"cfg": cfg},
    "manager.get_portable_mcp_servers": lambda: {},
    "manager.get_portable_mcp_server_plugins": lambda: {},
    "portable_server_tools": lambda manager, native: {
        "manager": manager_proj(manager), "native": native},
    "config.load_config": lambda: {},
    "plugin_tool_ownership": lambda: {},
    "plugins_cmd._get_enabled_set": lambda: {},
    "plugins_cmd._get_disabled_set": lambda: {},
    "plugins_cmd._read_install_metadata": lambda: {},
    "plugins_cmd._category_active_names": lambda: {},
    "plugins_cmd._discover_all_plugins": lambda: {},
    "plugins_cmd._plugin_status":
        lambda name, enabled, disabled, key=None, source=None, dir_path=None, active=None: {
            "name": name, "enabled": sorted(enabled), "disabled": sorted(disabled),
            "key": key, "source": source, "dir_path": dir_path,
            "active": sorted(active) if active is not None else None},
    "plugins_cmd._is_portable_plugin_dir": lambda plugin_dir: {"plugin_dir": plugin_dir},
    "plugins_cmd.pinned_revision": lambda name, ref_pins: {"name": name, "ref_pins": ref_pins},
    "plugins_cmd_catalog.catalog_rows_maps": lambda: {},
    "plugins_cmd_catalog.catalog_install_record":
        lambda plugin_dir: {"plugin_dir": plugin_dir.as_posix()},
    "plugins_cmd_catalog.catalog_row_fields":
        lambda dir, pins, versions: {"dir": dir, "pins": pins, "versions": versions},
    "plugins_cadence.cached_git_updates": lambda ref_pins: {"ref_pins": ref_pins},
    "desktop_plugin_halves.desktop_source":
        lambda plugin_dir: {"plugin_dir": plugin_dir.as_posix()},
    "plugins_settings.plugin_settings_fields":
        lambda key, plugin_dir: {"key": key,
                                 "plugin_dir": plugin_dir.as_posix() if plugin_dir is not None else None},
    "plugin_server_rows":
        lambda plugin_dir, key, portable, catalog_titles=None: {
            "plugin_dir": plugin_dir.as_posix() if plugin_dir is not None else None,
            "key": key, "portable": portable, "catalog_titles": catalog_titles},
    "refresh_live_sessions":
        lambda home=None, preserve_prefix=False, plugin_note=None: {
            "home": home, "preserve_prefix": preserve_prefix,
            "plugin_note": list(plugin_note) if plugin_note is not None else None},
    "sessions_items": lambda: {},
    "hermes_constants.hermes_home_key": lambda home: {"home": home},
    "get_process_hermes_home": lambda: {},
    "mcp_tool_agent.refresh_agent_mcp_tools":
        lambda agent, enabled_override=None, disabled_override=None,
        quiet_mode=False, preserve_prefix=False: {
            "agent": agent_proj(agent), "enabled_override": enabled_override,
            "disabled_override": disabled_override, "quiet_mode": quiet_mode,
            "preserve_prefix": preserve_prefix},
    "carry_onboarding_toolset": lambda enabled, agent: {"enabled": enabled, "agent": agent_proj(agent)},
    "load_enabled_toolsets": lambda platform: {"platform": platform},
    "load_disabled_toolsets": lambda: {},
    "logger.warning": lambda fmt, *a: {"message": fmt % a if a else fmt},
    "plugin_live_notes.queue_plugin_notice":
        lambda agent, key, text: {"agent": agent_proj(agent), "plugin_key": key, "note": text},
    "session_info": lambda agent, session: {"agent": agent_proj(agent), "session": session_proj(session)},
    "emit": lambda event, sid, info: {"event": event, "sid": sid, "info": info},
    "hermes_constants.set_hermes_home_override": lambda profile_home: {"profile_home": profile_home},
    "hermes_constants.reset_hermes_home_override": lambda token: {"token": token},
    "agent.runtime_cwd.set_session_cwd": lambda cwd: {"cwd": cwd},
    "agent.runtime_cwd.reset_session_cwd": lambda token: {"token": token},
}

# builds applied to specific seam results (materialization for attribute reads)
SEAM_BUILD = {
    "agent_plugins.load_agent_plugin": ns_build,
    "mcp_liveness.status": ns_build,
    "plugins_cmd._get_enabled_set": set_build,
    "plugins_cmd._get_disabled_set": set_build,
    "plugins_cmd._category_active_names": set_build,
    "plugin_choices.bind_asker": cm_build,
    "session_home_scope": cm_build,
    "session_profile_runtime_scope": cm_build,
    "sessions_items": sessions_items_build,
}


def module_double(name, ctx):
    if name == "tools.mcp_tool_common":
        return types.SimpleNamespace(__tmp_module__=name, _core=core_double(ctx))
    if name == "toolsets":
        data = ctx.io.get("toolsets.data")
        attrs = dict(data) if isinstance(data, dict) else {}
        return types.SimpleNamespace(__tmp_module__=name, **attrs)
    spec = MODULE_ATTRS.get(name)
    if spec is None:
        return types.SimpleNamespace(__tmp_module__=name)
    attrs = {}
    for attr, seam in spec.items():
        build = SEAM_BUILD.get(seam)
        if seam == "get_plugin_manager":
            build = lambda raw: manager_double_plugins(raw, ctx)
        attrs[attr] = rec(ctx, seam, ARGS_FN[seam], build)
    return types.SimpleNamespace(__tmp_module__=name, **attrs)


def tools_mod_build(raw, ctx):
    return module_double(raw, ctx) if isinstance(raw, str) else raw


# ── in-function import stub modules (registered once, rebound per case) ──
for _pkg in ("hermes_cli", "agent", "tools", "hermes_platform"):
    if _pkg not in sys.modules:
        new_module(_pkg)
CATALOG_MOD = new_module("hermes_cli.plugin_catalog")
CMD_CAT_MOD = new_module("hermes_cli.plugins_cmd_catalog")
ONBOARD_MOD = new_module("agent.onboarding_session")
CONST_MOD = new_module("hermes_constants")
sys.modules["hermes_cli"].plugin_catalog = CATALOG_MOD
sys.modules["hermes_cli"].plugins_cmd_catalog = CMD_CAT_MOD
sys.modules["agent"].onboarding_session = ONBOARD_MOD
IMPORTLIB_REAL = sys.modules.get("importlib")

# ── real defs restored before every case (seam slots are per-case) ──
SEAMED = {
    "tools": ("_tools_mod", "_is_registry_command", "_refresh_live_sessions",
              "_session_home_scope"),
    "cmds": ("_tools_mod", "_plugin_command_handler", "_run_plugin_command",
             "_is_registry_command", "_session_home_scope"),
    "servers": ("_tools_mod", "_server_identity", "_portable_server_tools"),
    "catalog": (),
}
REAL = {area: {nm: NSS[area][nm] for nm in names if nm in NSS[area]}
        for area, names in SEAMED.items()}


def install(area, ctx):
    ns = NSS[area]
    for nm, real in REAL[area].items():
        ns[nm] = real
    ns["_tools_mod"] = rec(ctx, "tools_mod", lambda module: {"module": module},
                           build=lambda raw: tools_mod_build(raw, ctx))
    if area == "tools":
        ns["_ok"] = rec(ctx, "ok", ARGS_FN["ok"])
        ns["_str_arg"] = rec(ctx, "str_arg", ARGS_FN["str_arg"])
        ns["_dispatch_plugin_in_profile_scope"] = rec(
            ctx, "dispatch_plugin_in_profile_scope",
            ARGS_FN["dispatch_plugin_in_profile_scope"])
        ns["_refresh_live_sessions"] = rec(
            ctx, "refresh_live_sessions", ARGS_FN["refresh_live_sessions"])
        ns["_plugin_tool_ownership"] = rec(ctx, "plugin_tool_ownership", lambda: {})
        ns["_plugin_server_rows"] = rec(ctx, "plugin_server_rows", ARGS_FN["plugin_server_rows"])
        ns["_session_profile_runtime_scope"] = rec(
            ctx, "session_profile_runtime_scope", ARGS_FN["session_profile_runtime_scope"],
            build=cm_build)
        ns["_sessions_lock"] = SilentCM(None)
        ns["_sessions"] = types.SimpleNamespace(
            items=rec(ctx, "sessions_items", lambda: {}, build=sessions_items_build))
        ns["get_process_hermes_home"] = rec(ctx, "get_process_hermes_home", lambda: {})
        ns["_load_enabled_toolsets"] = rec(ctx, "load_enabled_toolsets", ARGS_FN["load_enabled_toolsets"])
        ns["_load_disabled_toolsets"] = rec(ctx, "load_disabled_toolsets", lambda: {})
        ns["logger"] = types.SimpleNamespace(warning=rec(ctx, "logger.warning", ARGS_FN["logger.warning"]))
        ns["_emit"] = rec(ctx, "emit", ARGS_FN["emit"])
        ns["_session_info"] = rec(ctx, "session_info", ARGS_FN["session_info"])
        ns["_profile_home"] = rec(ctx, "profile_home", ARGS_FN["profile_home"])
    elif area == "cmds":
        ns["_set_session_context"] = rec(ctx, "set_session_context", ARGS_FN["set_session_context"])
        ns["_clear_session_context"] = rec(ctx, "clear_session_context", ARGS_FN["clear_session_context"])
        ns["_session_source"] = rec(ctx, "session_source", ARGS_FN["session_source"])
        ns["_session_home_scope"] = rec(ctx, "session_home_scope", ARGS_FN["session_home_scope"], build=cm_build)
        ns["_session_profile_runtime_scope"] = rec(
            ctx, "session_profile_runtime_scope", ARGS_FN["session_profile_runtime_scope"],
            build=cm_build)
        ns["_profile_home"] = rec(ctx, "profile_home", ARGS_FN["profile_home"])
        ns["_is_registry_command"] = rec(ctx, "is_registry_command", ARGS_FN["is_registry_command"])
        ns["_plugin_command_handler"] = rec(ctx, "plugin_command_handler", ARGS_FN["plugin_command_handler"])
        ns["_run_plugin_command"] = rec(ctx, "run_plugin_command", ARGS_FN["run_plugin_command"])
        ns["bind_module"] = rec(ctx, "bind_module", ARGS_FN["bind_module"])
    elif area == "servers":
        ns["_server_identity"] = rec(ctx, "server_identity", ARGS_FN["server_identity"])
        ns["_portable_server_tools"] = rec(ctx, "portable_server_tools", ARGS_FN["portable_server_tools"])
        ns["bind_module"] = rec(ctx, "bind_module", ARGS_FN["bind_module"])
    elif area == "catalog":
        ns["_ok"] = rec(ctx, "ok", ARGS_FN["ok"])
        ns["bind_module"] = rec(ctx, "bind_module", ARGS_FN["bind_module"])
    # in-function import stubs (silent imports; the calls are seams)
    CATALOG_MOD.load_catalog_live = rec(
        ctx, "plugin_catalog.load_catalog_live", lambda: {},
        build=lambda raw: [ns_build(e) for e in raw] if isinstance(raw, list) else raw)
    CATALOG_MOD.match_removed = rec(ctx, "plugin_catalog.match_removed", ARGS_FN["plugin_catalog.match_removed"])
    CATALOG_MOD.resolved_removed_entries = rec(
        ctx, "plugin_catalog.resolved_removed_entries", lambda: {})
    CMD_CAT_MOD.looks_like_catalog_name = rec(
        ctx, "plugins_cmd_catalog.looks_like_catalog_name",
        ARGS_FN["plugins_cmd_catalog.looks_like_catalog_name"])
    ONBOARD_MOD.carry_onboarding_toolset = rec(
        ctx, "carry_onboarding_toolset", ARGS_FN["carry_onboarding_toolset"])
    CONST_MOD.hermes_home_key = rec(
        ctx, "hermes_constants.hermes_home_key", ARGS_FN["hermes_constants.hermes_home_key"])


REG_MOD = {"methods_plugin_catalog": "catalog",
           "methods_tools_plugin_commands": "cmds",
           "methods_tools_plugin_servers": "servers"}

FUNC_AREA = {
    "_": "catalog",
    "_tools_mod": "tools", "_is_registry_command": "tools",
    "_dispatch_plugin": "tools", "refresh_plugin_sessions": "tools",
    "_plugin_rows": "tools", "_session_home_scope": "tools",
    "_refresh_live_sessions": "tools",
    "_plugin_command_handler": "cmds", "_run_plugin_command": "cmds",
    "_dispatch_plugin_in_profile_scope": "cmds",
    "_plugin_server_rows": "servers", "_server_identity": "servers",
    "_portable_server_tools": "servers", "_plugin_tool_ownership": "servers",
}


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    for area in NSS:
        install(area, ctx)
    # The case TARGET must run REAL even when it is also a seamed sibling of
    # other flows (the recorder installed above would otherwise call itself).
    area = FUNC_AREA.get(func)
    if area is None and func.endswith(".register"):
        area = REG_MOD[func.split(".")[0]]
    if area and func in REAL.get(area, {}):
        NSS[area][func] = REAL[area][func]
    state_in = copy.deepcopy(inp.get("state") or {})
    try:
        if func == "_":
            ret = NSS["catalog"]["_"](args.get("rid"), args.get("params"))
        elif func.endswith(".register"):
            area = REG_MOD[func.split(".")[0]]
            ret = NSS[area]["register"](args.get("server"))
        elif func == "_tools_mod":
            import importlib as _real_importlib
            stub = types.SimpleNamespace(
                import_module=rec(ctx, "importlib.import_module", ARGS_FN["importlib.import_module"]))
            sys.modules["importlib"] = stub
            try:
                ret = NSS["tools"]["_tools_mod"](args.get("module"))
            finally:
                if IMPORTLIB_REAL is not None:
                    sys.modules["importlib"] = IMPORTLIB_REAL
                else:
                    del sys.modules["importlib"]
        elif func == "_is_registry_command":
            ret = NSS["tools"]["_is_registry_command"](args.get("base"))
        elif func == "_dispatch_plugin":
            ret = NSS["tools"]["_dispatch_plugin"](
                args.get("rid"), args.get("params"), args.get("session"),
                args.get("name"), args.get("arg"))
        elif func == "refresh_plugin_sessions":
            ret = NSS["tools"]["refresh_plugin_sessions"](
                args.get("home"), args.get("plugin_key", ""), args.get("note", ""))
        elif func == "_plugin_rows":
            ret = NSS["tools"]["_plugin_rows"]()
        elif func == "_session_home_scope":
            gen = NSS["tools"]["_session_home_scope"](
                args.get("session"), cwd=args.get("cwd"), profile=args.get("profile"))
            next(gen)          # enter phase up to the yield
            gen.close()        # GeneratorExit at the yield -> the finally resets
            ret = None
        elif func == "_refresh_live_sessions":
            pn = args.get("plugin_note")
            ret = NSS["tools"]["_refresh_live_sessions"](
                args.get("home"), preserve_prefix=args.get("preserve_prefix", False),
                plugin_note=tuple(pn) if isinstance(pn, list) else pn)
        elif func == "_plugin_command_handler":
            ret = NSS["cmds"]["_plugin_command_handler"](args.get("name"))
        elif func == "_run_plugin_command":
            session = args.get("session")
            if isinstance(session, dict):
                session = mat_session(session)
            ret = NSS["cmds"]["_run_plugin_command"](
                args.get("handler"), args.get("arg"), session, args.get("sid", ""))
        elif func == "_dispatch_plugin_in_profile_scope":
            ret = NSS["cmds"]["_dispatch_plugin_in_profile_scope"](
                args.get("name"), args.get("arg"), args.get("session"),
                args.get("sid", ""), args.get("profile"))
        elif func == "_plugin_server_rows":
            pd = args.get("plugin_dir")
            ret = NSS["servers"]["_plugin_server_rows"](
                PurePosixPath(pd) if pd is not None else None, args.get("key"),
                portable=args.get("portable"), catalog_titles=args.get("catalog_titles"))
        elif func == "_server_identity":
            ret = NSS["servers"]["_server_identity"](args.get("cfg"))
        elif func == "_portable_server_tools":
            mgr = manager_double_tools(args.get("manager"), ctx)
            ret = NSS["servers"]["_portable_server_tools"](mgr, args.get("native") or {})
        elif func == "_plugin_tool_ownership":
            ret = NSS["servers"]["_plugin_tool_ownership"]()
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_in)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = sorted(set(ARGS_FN) | {
    "tools_mod", "server._is_recycled_stdio", "toolsets.data",
})


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_plugins_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES
                 if f'"{n}"' not in csrc and n not in ("tools_mod", "toolsets.data")]
    if missing_c:
        print(f"FATAL oracle_tui_methods_plugins_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    if 'tmp_call(ctx, "tools_mod"' not in csrc or 'tmp_module_data(ctx, "toolsets")' not in csrc:
        print("FATAL oracle_tui_methods_plugins_flow: tools_mod/toolsets.data plumbing absent")
        sys.exit(2)
    c_seams = set(re.findall(r'tmp_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_plugins_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n?\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(ARGS_FN)
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_plugins_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_plugins_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_plugins_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE tui_methods_plugins_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_plugins_flow.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_server_identity", {
        "args": {"cfg": {"command": "run", "args": ["a", 1], "url": "u"}},
        "io": {"tools_mod": "tools.mcp_tool_config",
               "mcp_tool_config._interpolate_env_vars":
                   {"__script__": True, "ret": {"command": "run", "args": ["a", 1], "url": "u"}}}})
    assert spot["ret"] == ["run", ["a", "1"], "u"], spot
    spot = drive("_is_registry_command", {
        "args": {"base": "help"},
        "io": {"tools_mod": "hermes_cli.commands",
               "commands.resolve_command": {"cmd": "help"}}})
    assert spot["ret"] is True, spot
    spot = drive("_is_registry_command", {
        "args": {"base": "help"},
        "io": {"tools_mod": "hermes_cli.commands",
               "commands.resolve_command": None}})
    assert spot["ret"] is False, spot
    spot = drive("_", {
        "args": {"rid": "r1", "params": {"name": "  "}},
        "io": {"plugins_cmd_catalog.looks_like_catalog_name": False,
               "ok": {"jsonrpc": "2.0", "id": "r1", "result": {"status": "invalid_name"}}}})
    assert spot["ret"] == {"jsonrpc": "2.0", "id": "r1", "result": {"status": "invalid_name"}}, spot
    assert spot["trace"][0]["name"] == "plugins_cmd_catalog.looks_like_catalog_name", spot
    spot = drive("refresh_plugin_sessions", {
        "args": {"home": "/h", "plugin_key": "pk", "note": ""},
        "io": {"refresh_live_sessions": None}})
    assert spot["ret"] is None, spot
    assert spot["trace"][0]["args"] == {"home": "/h", "preserve_prefix": True}, spot
    spot = drive("_dispatch_plugin", {
        "args": {"rid": "r1", "params": {}, "session": None, "name": "x", "arg": ""},
        "io": {"str_arg": "", "dispatch_plugin_in_profile_scope": None}})
    assert spot["ret"] is None, spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_plugins_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
