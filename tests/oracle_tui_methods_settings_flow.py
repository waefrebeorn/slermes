#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_config_set.py FLOW port
(src/gateway/tui_methods_settings_flow.c).

MODULE RESOLUTION: the task named tui_gateway/methods_settings.py, which does
NOT exist; the config.set settings handlers live in methods_config_set.py.

Drives the LIVE methods_config_set bodies with SCRIPTED seams: every server
global (_err / _ok / _emit / _sessions / _load_cfg / _display_cfg /
_write_config_key(s) / _save_cfg / the agent_factory + model_switch +
personality helpers), every in-function import callable, and every callback
arg is a recording stub fed the same scripts the C harness feeds its universal
seam. Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"}
in "trace"), the final return ("ret") and the mutated state mirror ("state":
the session carrier).

Live defs are AST-extracted from methods_config_set.py (module Assign constants
seeded; the @_cfgset_guarded setters exec'd WITH their decorator so the guard
wrapper composes live; the @method/@_profile_scoped `def _` dispatcher extracted
BARE as _h_config_set; the _set_yolo nested `persist` closure re-hosted in a
maker). The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_settings_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds pass through every except-Exception site.
- The session carrier is state.session (raw dict or null); its "agent" field is
  an attribute double (AttrDouble, missing attr raises the SimpleNamespace
  message), "agent_ready" an Event double (wait/is_set seams). Mutations echo.
- parsed model-switch doubles carry explicit_provider/is_global/is_session/
  is_once (required present), is_reset/reasoning_effort (getattr defaults), and
  model_input (a property: model_input_raise {type,message} RAISES on access).
- Tuples project to lists; sets to sorted lists; integral floats to ints;
  Paths to POSIX strings. ASCII corpus. Seam args drop None-valued keys.
- List-valued seam RESULTS are wrapped one level deeper (bare array == queue).
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
SRC = os.path.join(Q, "tui_gateway", "methods_config_set.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


PORTED = [
    "_word", "_raw_word", "_kv", "_cfgset_model_ok", "_emit_session_info",
    "_emit_all_session_info", "_cfgset_await_agent", "_refuse_managed_keys",
    "_refuse_managed_model_pick", "_write_display_sections",
    "_stash_pending_model_switch", "_cfgset_guarded", "_set_word", "_set_toggle",
    "_set_busy", "_set_approval_mode", "_set_details_section", "_set_verbose",
    "_set_focus", "_set_display_toggle", "_set_daybreak", "_set_skin",
    "_set_prompt", "_set_personality", "_set_cwd", "_set_yolo",
    "_set_yolo_persist", "_set_reasoning", "_set_fast", "_set_model",
    "_h_config_set",
]

# setters driven with the uniform (rid, params, key, value, session) signature
SETTER_SIG = {
    "_set_word", "_set_toggle", "_set_busy", "_set_approval_mode",
    "_set_details_section", "_set_verbose", "_set_focus", "_set_display_toggle",
    "_set_daybreak", "_set_skin", "_set_prompt", "_set_personality", "_set_cwd",
    "_set_yolo", "_set_reasoning", "_set_fast", "_set_model",
}


def stub_pkg(name):
    if name not in sys.modules:
        new_module(name)
    return sys.modules[name]


def stub_mod(full):
    parts = full.split(".")
    for i in range(1, len(parts)):
        stub_pkg(".".join(parts[:i]))
    mod = new_module(full) if full not in sys.modules else sys.modules[full]
    parent = sys.modules.get(".".join(parts[:-1]))
    if parent is not None:
        setattr(parent, parts[-1], mod)
    return mod


MANAGED_MOD = stub_mod("hermes_cli.managed_scope")
CONFIG_MOD = stub_mod("hermes_cli.config")
MSWITCH_MOD = stub_mod("hermes_cli.model_switch")
CFGBACK_MOD = stub_mod("hermes_cli.config_backend")
MODELS_MOD = stub_mod("hermes_cli.models")
PERSONALITY_MOD = stub_mod("hermes_cli.personality")
FOCUS_MOD = stub_mod("hermes_cli.focus_view")
CONST_MOD = stub_mod("hermes_constants")
FAST_MOD = stub_mod("agent.fast_mode")
CODEX_MOD = stub_mod("agent.codex_service_tiers")
APPROVAL_CTX_MOD = stub_mod("tools.approval_context")
APPROVAL_YOLO_MOD = stub_mod("tools.approval_yolo")
UTILS_MOD = stub_mod("utils")
LIFECYCLE_MOD = stub_mod("tui_gateway.session_lifecycle")


class ConfigWriteError(Exception):
    def __init__(self, message="", code=None):
        super().__init__(message)
        self.code = code


class SessionNotOwned(RuntimeError):
    pass


CFGBACK_MOD.ConfigWriteError = ConfigWriteError
LIFECYCLE_MOD.SessionNotOwned = SessionNotOwned


# -- methods_config_set.py namespace (live defs + seeded constants) --
def extract():
    mod = new_module("tset_oracle_methods_config_set")
    ns = mod.__dict__
    ns.update({"os": os, "json": json, "types": types, "copy": copy,
               "Path": Path, "PurePosixPath": PurePosixPath})
    import contextlib
    ns["contextlib"] = contextlib
    ns["ConfigWriteError"] = ConfigWriteError
    ns["SessionNotOwned"] = SessionNotOwned
    ns["HandlerRegistry"] = lambda: types.SimpleNamespace(
        method=lambda name: (lambda fn: fn), profile_scoped=lambda fn: fn)
    ns["bind_module"] = lambda *a, **k: None
    # external constants (server globals / hermes_constants)
    ns["INDICATOR_STYLES"] = ("ascii", "emoji", "kaomoji", "unicode")
    ns["_DETAIL_SECTION_NAMES"] = ("thinking", "tools", "subagents", "activity")
    ns["_DETAIL_MODES"] = frozenset({"hidden", "collapsed", "expanded"})
    ns["_STATUSBAR_MODES"] = frozenset({"off", "top", "bottom"})
    ns["_APPROVAL_MODES"] = frozenset({"manual", "smart", "off"})
    ns["_DISPLAY_TOGGLE_KEYS"] = frozenset(
        {"display.message_reactions", "display.in_app_tips", "display.in_app_tours"})
    ns["_BOOL_WORDS"] = {"1": True, "on": True, "true": True, "yes": True,
                         "0": False, "off": False, "false": False, "no": False}
    ns["_MOUSE_TRACKING_ALIASES"] = {
        "0": "off", "1": "all", "all": "all", "any": "all", "button": "buttons",
        "buttons": "buttons", "click": "buttons", "false": "off", "full": "all",
        "no": "off", "off": "off", "on": "all", "scroll": "wheel", "true": "all",
        "wheel": "wheel", "yes": "all"}
    ns["AGENT_STILL_STARTING"] = (
        "Hermes is still starting this session (loading tools), so this command could not run yet. "
        "Wait for the status bar to show ready and try again.")
    ns["STATIC_TIERS"] = frozenset({"priority", "ultrafast"})
    ns["FOCUS_TOOL_PROGRESS_MODE"] = "off"
    ns["git_probe"] = types.SimpleNamespace(branch=lambda cwd: None)
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # the ManagedKeyError class (subclasses the stub ConfigWriteError)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "ManagedKeyError":
            exec_node(src, node, ns, SRC)
    # module constants (skip the handler table -- exec'd after the defs)
    skip = {"_registry", "method", "_profile_scoped", "_CONFIG_SETTERS"}
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            if any(t.id in skip for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # function defs in source order (decorated setters keep @_cfgset_guarded)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name != "_":
            exec_node(src, node, ns, SRC)
    # the `def _` dispatcher: strip registry decorators, rename
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_" and node.decorator_list:
            text = "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])
            text = text.replace("def _(", "def _h_config_set(", 1)
            exec(compile(text, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
    # the handler table AFTER the defs
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_CONFIG_SETTERS" for t in node.targets):
            exec_node(src, node, ns, SRC)
    # the _set_yolo nested `persist` closure -> a maker binding session/row_id
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_set_yolo":
            for sub in ast.walk(node):
                if isinstance(sub, ast.FunctionDef) and sub.name == "persist":
                    body = "\n".join(src.splitlines()[sub.lineno - 1:sub.end_lineno])
                    maker = ("def __mk_persist__(session, row_id):\n"
                             + textwrap.indent(textwrap.dedent(body), "    ")
                             + "\n    return persist\n")
                    exec(compile(maker, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
    required = [p for p in PORTED if p != "_set_yolo_persist"] + [
        "_CONFIG_SETTERS", "_FAST_WORDS", "_REASONING_DISPLAY_WORDS",
        "_SESSION_SCOPED_KEYS", "ManagedKeyError", "__mk_persist__"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_settings_flow: methods_config_set missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# -- exception materialization / projection --
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "PermissionError": PermissionError, "OSError": OSError, "IndexError": IndexError,
    "TimeoutError": TimeoutError, "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
    "ConfigWriteError": ConfigWriteError, "SessionNotOwned": SessionNotOwned,
    "ManagedKeyError": NS["ManagedKeyError"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        cls = type("KeyError", (KeyError,),
                   {"__str__": lambda self: self.args[0] if self.args else ""})
    if kind == "ManagedKeyError":
        e = cls(spec.get("message", ""))
        return e
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- attribute / event / parsed doubles --
class AttrDouble:
    """Agent attribute double backed by a raw dict (mutations echo to state)."""
    def __init__(self, d):
        object.__setattr__(self, "_d", d)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError(f"'types.SimpleNamespace' object has no attribute '{k}'")

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v


class EventDouble:
    def __init__(self, ctx, marker):
        self.ctx = ctx
        self.id = (marker.get("id") if isinstance(marker, dict) else None) or ""

    def wait(self, timeout=None):
        return self.ctx.call("event_wait", {"id": self.id, "timeout": timeout})

    def is_set(self):
        return self.ctx.call("event_is_set", {"id": self.id})


class ParsedDouble:
    def __init__(self, d):
        object.__setattr__(self, "_src", d)
        for k, v in d.items():
            if k not in ("model_input", "model_input_raise"):
                object.__setattr__(self, k, v)
        object.__setattr__(self, "_mi", d.get("model_input"))
        object.__setattr__(self, "_mi_raise", d.get("model_input_raise"))

    @property
    def model_input(self):
        mr = object.__getattribute__(self, "_mi_raise")
        if isinstance(mr, dict):
            raise make_exc(mr)
        return object.__getattribute__(self, "_mi")

    def __getattr__(self, k):
        raise AttributeError(f"'types.SimpleNamespace' object has no attribute '{k}'")


def parsed_build(raw):
    return ParsedDouble(raw) if isinstance(raw, dict) else raw


class SilentCM:
    def __init__(self, marker):
        self.__cm_marker__ = marker

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class YieldCM:
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self.value

    def __exit__(self, *a):
        return False


def cm_build(raw):
    return SilentCM(raw) if isinstance(raw, dict) else raw


def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return proj_exc(x)
        if isinstance(x, AttrDouble):
            return conv(object.__getattribute__(x, "_d"))
        if isinstance(x, ParsedDouble):
            return conv(object.__getattribute__(x, "_src"))
        if isinstance(x, EventDouble):
            return {"id": x.id}
        if isinstance(x, SilentCM):
            return conv(x.__cm_marker__)
        if isinstance(x, types.SimpleNamespace):
            return conv({k: val for k, val in vars(x).items()})
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


# -- per-case script backend (mirrors the C tset_call exactly) --
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
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def _args(*names):
    """Positional/keyword args projector (avoids the py3.14 lambda-default
    parser quirk on certain parameter sequences). Absent -> None (dropped)."""
    def f(*a, **k):
        out = {}
        for i, nm in enumerate(names):
            if i < len(a):
                out[nm] = a[i]
            elif nm in k:
                out[nm] = k[nm]
            else:
                out[nm] = None
        return out
    return f


CURRENT = {}


def materialize_session(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    if isinstance(raw.get("agent"), dict):
        raw["agent"] = AttrDouble(raw["agent"])
    if isinstance(raw.get("agent_ready"), dict):
        raw["agent_ready"] = EventDouble(ctx, raw["agent_ready"])
    return raw


def session_build(raw):
    if not isinstance(raw, dict):
        return raw
    out = dict(raw)
    if isinstance(out.get("agent"), dict):
        out["agent"] = AttrDouble(out["agent"])
    return out


def sessions_items_build(raw):
    if not isinstance(raw, list):
        return raw
    return [[sid, session_build(sess)] for sid, sess in raw]


class EnvironDouble:
    """os.environ double: get/__setitem__/pop fire seams."""
    def __init__(self, ctx):
        self.ctx = ctx

    def get(self, key, default=None):
        return self.ctx.call("environ_get", {"key": key})

    def __setitem__(self, key, value):
        return self.ctx.call("environ_set", {"key": key, "value": value})

    def pop(self, key, default=None):
        return self.ctx.call("environ_pop", {"key": key})


def persistence_build(raw):
    if raw is None:
        return YieldCM(None)
    if isinstance(raw, dict):
        obj = types.SimpleNamespace(
            sessions=types.SimpleNamespace(
                set_yolo=rec(CURRENT["ctx"], "set_yolo",
                             lambda row_id, on: {"row_id": row_id, "on": on})))
        return YieldCM(obj)
    return YieldCM(raw)


def install_stubs(ctx, args):
    CURRENT["ctx"] = ctx
    NS["_err"] = rec(ctx, "err", _args("rid", "code", "message", "data"))
    NS["_ok"] = rec(ctx, "ok", lambda rid, result: {"rid": rid, "result": result})
    NS["_emit"] = rec(ctx, "emit",
                      lambda event, sid, payload: {"event": event, "sid": sid, "payload": payload})
    NS["_session_info"] = rec(ctx, "session_info",
                              lambda agent, session: {"agent": agent, "session": session})
    NS["_sessions"] = types.SimpleNamespace(
        get=rec(ctx, "sessions_get", lambda session_id: {"session_id": session_id},
                build=lambda raw: CURRENT["session_double"] if raw is not None else None),
        items=rec(ctx, "sessions_items", lambda: {}, build=sessions_items_build))
    NS["_wait_agent"] = rec(ctx, "wait_agent",
                            lambda session, rid: {"session": session, "rid": rid})
    NS["_load_cfg"] = rec(ctx, "load_cfg", lambda: {})
    NS["_load_cfg_raw"] = rec(ctx, "load_cfg_raw", lambda: {})
    NS["_save_cfg"] = rec(ctx, "save_cfg", lambda cfg: {"cfg": cfg})
    NS["_display_cfg"] = rec(ctx, "display_cfg", lambda: {})
    NS["_write_config_key"] = rec(ctx, "write_config_key",
                                  lambda key, value: {"key": key, "value": value})
    NS["_write_config_keys"] = rec(ctx, "write_config_keys",
                                   lambda updates: {"updates": updates})
    NS["_write_config_changes"] = rec(ctx, "write_config_changes",
                                      _args("sets", "drops"))
    NS["_load_service_tier"] = rec(ctx, "load_service_tier", lambda: {})
    NS["_resolve_model"] = rec(ctx, "resolve_model", lambda: {})
    NS["_load_busy_input_mode"] = rec(ctx, "load_busy_input_mode", lambda: {})
    NS["_load_tool_progress_mode"] = rec(ctx, "load_tool_progress_mode", lambda: {})
    NS["_coerce_statusbar"] = rec(ctx, "coerce_statusbar", lambda raw: {"raw": raw})
    NS["_display_mouse_tracking"] = rec(ctx, "display_mouse_tracking",
                                        lambda display: {"display": display})
    NS["_persist_live_session_runtime"] = rec(ctx, "persist_live_session_runtime",
                                              lambda session: {"session": session})
    NS["_start_agent_build"] = rec(ctx, "start_agent_build",
                                   lambda sid, session: {"sid": sid, "session": session})
    NS["_restart_completed_failed_agent_build"] = rec(
        ctx, "restart_failed_agent_build",
        lambda sid, session, ready: {"sid": sid, "session": session, "ready": ready})
    NS["_session_profile_runtime_scope"] = rec(
        ctx, "session_profile_runtime_scope", lambda session: {"session": session},
        build=cm_build)
    NS["_apply_model_switch"] = rec(
        ctx, "apply_model_switch",
        _args("sid", "session", "value", "confirm_expensive_model", "parsed_flags"))
    NS["_config_model_target"] = rec(ctx, "config_model_target", lambda: {})
    NS["_pending_switch_selection_warning"] = rec(
        ctx, "pending_switch_selection_warning", _args("model", "provider", "agent"))
    NS["_validate_personality"] = rec(ctx, "validate_personality", _args("value", "cfg"))
    NS["_apply_personality_to_session"] = rec(
        ctx, "apply_personality_to_session",
        lambda sid, session, prompt, pname:
        {"sid": sid, "session": session, "prompt": prompt, "pname": pname})
    NS["_broadcast_global_event"] = rec(ctx, "broadcast_global_event",
                                        _args("event", "payload"))
    NS["_note_skin_broadcast"] = rec(ctx, "note_skin_broadcast", lambda: {})
    NS["resolve_skin"] = rec(ctx, "resolve_skin", lambda: {})
    NS["git_probe"] = types.SimpleNamespace(branch=rec(ctx, "git_branch", lambda cwd: {"cwd": cwd}))
    NS["_sess_nowait"] = rec(ctx, "sess_nowait",
                             lambda params, rid: {"params": params, "rid": rid})
    NS["_session_db"] = rec(ctx, "session_db", lambda session: {"session": session},
                            build=lambda raw: YieldCM(raw))
    NS["_persistence_for"] = rec(ctx, "persistence_for", lambda db: {"db": db},
                                 build=persistence_build)
    NS["is_truthy_value"] = rec(ctx, "is_truthy_value", _args("value", "default"))
    NS["_hermes_home"] = Path(args.get("hermes_home", "/h/launch"))
    NS["get_hermes_home"] = rec(ctx, "get_hermes_home", lambda: {},
                                build=lambda raw: Path(raw) if isinstance(raw, str) else raw)
    # os double: environ seams + path probes
    NS["os"] = types.SimpleNamespace(
        environ=EnvironDouble(ctx),
        path=types.SimpleNamespace(
            expanduser=rec(ctx, "expanduser", lambda raw: {"raw": raw}),
            abspath=rec(ctx, "abspath", lambda path: {"path": path}),
            isdir=rec(ctx, "isdir", lambda path: {"path": path})))
    # in-function import stubs
    MANAGED_MOD.is_key_managed = rec(ctx, "is_key_managed", lambda key_path: {"key_path": key_path})
    MANAGED_MOD.managed_config_keys = rec(ctx, "managed_config_keys", lambda: {})
    CONFIG_MOD._managed_source = rec(ctx, "managed_source", lambda path: {"path": path})
    CONFIG_MOD.read_user_config_raw = rec(ctx, "read_user_config_raw", lambda: {})
    MSWITCH_MOD.model_selection_config_updates = rec(
        ctx, "model_selection_config_updates",
        lambda result, model: {"result": result, "model": model})
    MSWITCH_MOD.resolve_persist_behavior = rec(
        ctx, "resolve_persist_behavior",
        _args("is_global", "is_session", "is_once", "explicit_provider"))
    MSWITCH_MOD.parse_model_switch_args = rec(ctx, "parse_model_switch_args",
                                              lambda value: {"value": value}, build=parsed_build)
    CONST_MOD.parse_reasoning_effort = rec(ctx, "parse_reasoning_effort", lambda raw: {"raw": raw})
    CFGBACK_MOD.supports_file_tooling = rec(ctx, "supports_file_tooling", lambda: {})
    FAST_MOD.service_tier_word = rec(ctx, "service_tier_word", lambda tier: {"tier": tier})
    FAST_MOD.STATIC_TIERS = frozenset({"priority", "ultrafast"})
    MODELS_MOD.resolve_fast_mode_overrides = rec(
        ctx, "resolve_fast_mode_overrides",
        _args("model", "provider", "base_url", "tier"))
    CODEX_MOD.ultrafast_offered = rec(
        ctx, "ultrafast_offered",
        _args("model", "provider", "base_url", "access_token"))
    APPROVAL_CTX_MOD._normalize_approval_mode = rec(ctx, "normalize_approval_mode",
                                                    lambda mode: {"mode": mode})
    APPROVAL_YOLO_MOD.toggle_session_yolo = rec(
        ctx, "toggle_session_yolo",
        lambda session_key, value=None, persist=None: {"session_key": session_key, "value": value, "persist": "persist"})
    FOCUS_MOD.resolve_focus_arg = rec(ctx, "resolve_focus_arg",
                                      lambda arg, current: {"arg": arg, "current": current})
    FOCUS_MOD.normalize_tool_progress_mode = rec(ctx, "normalize_tool_progress_mode",
                                                 _args("mode", "default"))
    FOCUS_MOD.FOCUS_TOOL_PROGRESS_MODE = "off"
    PERSONALITY_MOD.persist_personality = rec(ctx, "persist_personality",
                                              lambda pname: {"pname": pname})
    UTILS_MOD.is_truthy_value = NS["is_truthy_value"]
    # Path.resolve monkeypatch (never hits the real filesystem)
    Path.resolve = lambda self: Path(ctx.call("path_resolve", {"path": self.as_posix()}))


REAL = {nm: NS[nm] for nm in PORTED if nm in NS and nm != "_set_yolo_persist"}


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    state_in = copy.deepcopy(inp.get("state") or {})
    for nm, real in REAL.items():
        NS[nm] = real
    session_raw = state_in.get("session")
    session_double = materialize_session(session_raw, ctx) if isinstance(session_raw, dict) else None
    CURRENT["session_double"] = session_double
    install_stubs(ctx, args)
    rid = args.get("rid")
    params = args.get("params")
    key = args.get("key")
    value = args.get("value")
    try:
        if func == "_word" or func == "_raw_word":
            ret = NS[func](value)
        elif func == "_kv":
            ret = NS[func](rid, key, value, **(args.get("extra") or {}))
        elif func == "_cfgset_model_ok":
            ret = NS[func](rid, key, value, warning=args.get("warning", ""),
                           confirm_message=args.get("confirm_message", ""),
                           scope=args.get("scope", "session"), **(args.get("extra") or {}))
        elif func == "_emit_session_info":
            ret = NS[func](args.get("sid"), session_double)
        elif func == "_emit_all_session_info":
            ret = NS[func]()
        elif func == "_cfgset_await_agent":
            ret = NS[func](session_double, rid)
        elif func == "_refuse_managed_keys":
            ret = NS[func](*(args.get("key_paths") or []))
        elif func == "_refuse_managed_model_pick":
            ret = NS[func](args.get("reasoning_effort", ""), args.get("result"))
        elif func == "_write_display_sections":
            ret = NS[func](sections=args.get("sections"),
                           drop_sections=tuple(args.get("drop_sections") or ()),
                           **(args.get("display_fields") or {}))
        elif func == "_stash_pending_model_switch":
            ret = NS[func](rid, key, value, session_double, args.get("confirmed", False),
                           parsed_build(args.get("parsed")))
        elif func == "_cfgset_guarded":
            fn = rec(ctx, "guarded_fn",
                     lambda r, p, k, v, s: {"rid": r, "params": p, "key": k, "value": v, "session": s})
            handler = NS[func](fn)
            ret = handler(rid, params, key, value, session_double)
        elif func == "_set_yolo_persist":
            persist = NS["__mk_persist__"](session_double, args.get("row_id"))
            ret = persist(args.get("on"))
        elif func in SETTER_SIG:
            ret = NS[func](rid, params, key, value, session_double)
        elif func == "_h_config_set":
            ret = NS[func](rid, params)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_in)}


# -- three-way seam-name cross-check --
SEAM_NAMES = [
    "err", "ok", "emit", "session_info", "sessions_get", "sessions_items",
    "wait_agent", "load_cfg", "load_cfg_raw", "save_cfg", "display_cfg",
    "write_config_key", "write_config_keys", "write_config_changes",
    "load_service_tier", "resolve_model", "load_busy_input_mode",
    "load_tool_progress_mode", "coerce_statusbar", "display_mouse_tracking",
    "persist_live_session_runtime", "start_agent_build", "restart_failed_agent_build",
    "session_profile_runtime_scope", "apply_model_switch", "config_model_target",
    "pending_switch_selection_warning", "validate_personality",
    "apply_personality_to_session", "broadcast_global_event", "note_skin_broadcast",
    "resolve_skin", "git_branch", "sess_nowait", "session_db", "persistence_for",
    "set_yolo", "toggle_session_yolo", "is_truthy_value", "environ_get",
    "environ_set", "environ_pop", "expanduser", "abspath", "isdir",
    "get_hermes_home", "path_resolve", "event_wait", "event_is_set", "guarded_fn",
    "is_key_managed", "managed_config_keys", "managed_source", "read_user_config_raw",
    "model_selection_config_updates", "resolve_persist_behavior",
    "parse_model_switch_args", "parse_reasoning_effort", "supports_file_tooling",
    "service_tier_word", "resolve_fast_mode_overrides", "ultrafast_offered",
    "normalize_approval_mode", "resolve_focus_arg", "normalize_tool_progress_mode",
    "persist_personality",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_settings_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_settings_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tset_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_settings_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_settings_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_settings_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_funcs.add(case.get("func"))
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_settings_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_tui_methods_settings_flow: fixture funcs outside PORTED: {sorted(badf)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown, case_no=0):
    if shown >= 100:
        return
    print(f"MISMATCH {func} (case {case_no}):")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:2000]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:2000]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:2000]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:2000]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:1000]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:1000]}")


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
            report_mismatch(func, want, g, shown, n)
            shown += 1
    print(f"ORACLE tui_methods_settings_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_settings_flow.jsonl"
    n = bad = 0
    counts = {}
    ported_set = set(PORTED) | set(SEAM_NAMES)
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            builtin_types = ("'NoneType'", "'str'", "'int'", "'float'", "'bool'",
                             "'list'", "'dict'")
            if not msg.startswith(builtin_types) and \
                    any(f"'{nm}'" in msg for nm in ported_set):
                bad += 1
                print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
    thin = [f for f in PORTED if counts.get(f, 0) < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_settings_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()