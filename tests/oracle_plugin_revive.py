#!/usr/bin/env python3
"""oracle_plugin_revive.py — differential oracle for the plugins.py revival.

Reads the harness's JSONL stdout (tests/t_port_plugin_revive.c, which
#include's lib/libplugin/plugin.c). Each record echoes the full fixture case;
this oracle replays it against AST-extracted LIVE defs from the quarry
(read-only) with recording seam stubs driven by the SAME io scripts, then
diffs ret / extra / trace / state / err. Fail-closed: any unparseable harness
line, missing case echo, or live-extraction failure is a mismatch/fatal.

Gate:
  bash tests/run_one_oracle.sh plugin_revive oracle_plugin_revive.py "" \
       "lib/libjson/json.o"

Live defs are extracted from hermes_cli/plugins.py and the sibling modules
the facade's claimed names actually live in (plugins_dispatch, plugins_loader,
plugins_context, plugin_capabilities, plugins_discovery, plugins_manifest,
utils/env). Module constants are exec'd (Assign AND AnnAssign, plain-Name
targets only); dataclasses (_PreToolCallDirective, CapabilitySpec) are exec'd
into a registered ModuleType namespace.

Representation projections (mirrored exactly by the C port, documented in
plugin.c's revival section): remove_skill -> key-existed bool; ensure ->
_discovered bool; directive -> action + message out-slot; resolve -> blocked
bool + message out-slot; context engine / command handler / module loads ->
string tokens. Path values normalize via as_posix; Windows backslashes in
path-typed trace fields and loader exception messages normalize to '/'.
"""
import ast
import builtins
import json
import os
import sys
import types
import hashlib
from contextlib import suppress
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, Optional, Set, Tuple, Union

_REPO = Path(__file__).resolve().parent.parent
_QUARRY = _REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home_plugin_revive"))

import __future__ as _future
_ANN_FLAG = _future.annotations.compiler_flag

SRC = {}
for _rel in ("hermes_cli/plugins.py", "hermes_cli/plugins_dispatch.py",
             "hermes_cli/plugins_loader.py", "hermes_cli/plugins_context.py",
             "hermes_cli/plugins_discovery.py", "hermes_cli/plugins_manifest.py",
             "hermes_cli/plugin_capabilities.py", "utils/env.py"):
    SRC[_rel] = (_QUARRY / _rel).read_text(encoding="utf-8")

TREES = {rel: ast.parse(src) for rel, src in SRC.items()}


def _find_def(rel, name):
    for node in TREES[rel].body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            return node
        if isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name == name:
                    return sub
    raise RuntimeError(f"live def not found: {rel}:{name}")


def _find_class(rel, name):
    for node in TREES[rel].body:
        if isinstance(node, ast.ClassDef) and node.name == name:
            return node
    raise RuntimeError(f"live class not found: {rel}:{name}")


# ns name -> (source, ast name, rename?)
DEF_PLAN = [
    ("is_truthy_value", "utils/env.py", "is_truthy_value"),
    ("env_var_enabled", "utils/env.py", "env_var_enabled"),
    ("_install_plugin_debug_handler", "hermes_cli/plugins.py", "_install_plugin_debug_handler"),
    ("_join_background_discovery", "hermes_cli/plugins.py", "_join_background_discovery"),
    ("_delivery_manager", "hermes_cli/plugins.py", "_delivery_manager"),
    ("invoke_hook", "hermes_cli/plugins.py", "invoke_hook"),
    ("has_hook", "hermes_cli/plugins.py", "has_hook"),
    ("has_middleware", "hermes_cli/plugins.py", "has_middleware"),
    ("_get_pre_tool_call_directive_details", "hermes_cli/plugins.py", "_get_pre_tool_call_directive_details"),
    ("get_pre_tool_call_directive", "hermes_cli/plugins.py", "get_pre_tool_call_directive"),
    ("get_pre_tool_call_block_message", "hermes_cli/plugins.py", "get_pre_tool_call_block_message"),
    ("_resolve_block_from_details", "hermes_cli/plugins.py", "_resolve_block_from_details"),
    ("_dispatch_pre_tool_call_hooks", "hermes_cli/plugins.py", "_dispatch_pre_tool_call_hooks"),
    ("resolve_pre_tool_block", "hermes_cli/plugins.py", "resolve_pre_tool_block"),
    ("get_pre_verify_continue_message", "hermes_cli/plugins.py", "get_pre_verify_continue_message"),
    ("_ensure_plugins_discovered", "hermes_cli/plugins.py", "_ensure_plugins_discovered"),
    ("get_plugin_context_engine", "hermes_cli/plugins.py", "get_plugin_context_engine"),
    ("get_plugin_command_handler", "hermes_cli/plugins.py", "get_plugin_command_handler"),
    ("find_plugin_skill", "hermes_cli/plugins.py", "find_plugin_skill"),
    ("remove_plugin_skill", "hermes_cli/plugins.py", "remove_plugin_skill"),
    ("mgr_has_hook", "hermes_cli/plugins_dispatch.py", "has_hook"),
    ("mgr_has_middleware", "hermes_cli/plugins_dispatch.py", "has_middleware"),
    ("_tool_override_allowed", "hermes_cli/plugins_context.py", "_tool_override_allowed"),
    ("_evict_modules", "hermes_cli/plugins_loader.py", "_evict_modules"),
    ("_platform_name_from_manifest", "hermes_cli/plugins_loader.py", "_platform_name_from_manifest"),
    ("_load_directory_module", "hermes_cli/plugins_loader.py", "_load_directory_module"),
    ("_load_entrypoint_module", "hermes_cli/plugins_loader.py", "_load_entrypoint_module"),
    ("_directory_module_name", "hermes_cli/plugins_loader.py", "_directory_module_name"),
    ("_select_entry_point_group", "hermes_cli/plugins_discovery.py", "_select_entry_point_group"),
    ("manifest_key", "hermes_cli/plugins_manifest.py", "manifest_key"),
    ("plugin_capability_granted", "hermes_cli/plugin_capabilities.py", "plugin_capability_granted"),
    ("granted_capabilities", "hermes_cli/plugin_capabilities.py", "granted_capabilities"),
    ("_plugin_entry", "hermes_cli/plugin_capabilities.py", "_plugin_entry"),
    ("_legacy_gate_set", "hermes_cli/plugin_capabilities.py", "_legacy_gate_set"),
    ("_known", "hermes_cli/plugin_capabilities.py", "_known"),
]

CONST_PLAN = [
    ("utils/env.py", ("TRUTHY_STRINGS",)),
    ("hermes_cli/plugins_loader.py", ("_NS_PARENT",)),
    ("hermes_cli/plugins_discovery.py", ("ENTRY_POINTS_GROUP",)),
    ("hermes_cli/plugin_capabilities.py",
     ("_CAPABILITY_ROWS", "CAPABILITY_REGISTRY", "VALID_CAPABILITY_IDS",
      "GRANTED_KEY", "CONSENT_KEY")),
]


def _exec_node(node, ns, keep_decorators=False):
    import copy
    n = copy.deepcopy(node)
    if not keep_decorators and isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
        n.decorator_list = []
    mod = ast.Module(body=[n], type_ignores=[])
    ast.fix_missing_locations(mod)
    exec(compile(mod, "<live>", "exec", flags=_ANN_FLAG), ns)


def _seed_constants(ns):
    for rel, names in CONST_PLAN:
        for node in TREES[rel].body:
            targets = []
            if isinstance(node, ast.Assign):
                targets = [t.id for t in node.targets if isinstance(t, ast.Name)]
            elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
                targets = [node.target.id]
            if any(t in names for t in targets):
                seg = ast.get_source_segment(SRC[rel], node)
                exec(compile(seg, f"<const:{rel}>", "exec"), ns)
    for rel, names in CONST_PLAN:
        for nm in names:
            if nm not in ns:
                raise RuntimeError(f"constant seed missing: {rel}:{nm}")


# ── per-case recording machinery ──

class CaseCtx:
    def __init__(self, io):
        self.io = io or {}
        self.trace = []
        self.idx = {}

    def script(self, name):
        s = self.io.get(name)
        if isinstance(s, dict) and "__list__" in s:
            lst = s["__list__"]
            i = self.idx.get(name, 0)
            self.idx[name] = i + 1
            if not lst:
                return None
            return lst[i] if i < len(lst) else lst[-1]
        return s

    def rec(self, name, args, ret=None):
        self.trace.append({"ev": name, "args": args, "ret": ret})


def _is_raise(s):
    return isinstance(s, dict) and isinstance(s.get("__raise__"), str)


def _materialize_raise(s):
    cls = s["__raise__"]
    msg = s.get("msg", "")
    base = getattr(builtins, cls, None)
    if isinstance(base, type) and issubclass(base, BaseException):
        return base(msg)
    return type(cls, (Exception,), {})(msg)


def _norm_path_str(s):
    return s.replace("\\", "/") if isinstance(s, str) else s


def _norm_trace(trace):
    out = []
    for ev in trace:
        e = {"ev": ev["ev"], "args": json.loads(json.dumps(ev["args"])), "ret": ev["ret"]}
        a = e["args"]
        if isinstance(a, dict):
            for k in ("path", "location"):
                if k in a:
                    a[k] = _norm_path_str(a[k])
            if isinstance(a.get("submodule_search_locations"), list):
                a["submodule_search_locations"] = [_norm_path_str(x) for x in a["submodule_search_locations"]]
        out.append(e)
    return out


def _norm_mod(v):
    if isinstance(v, dict):
        return v
    if isinstance(v, types.ModuleType):
        return {"kind": "ns", "name": v.__name__,
                "package": getattr(v, "__package__", ""),
                "path": list(getattr(v, "__path__", []))}
    tok = getattr(v, "__token__", None)
    if tok is not None:
        return {"kind": "mod", "token": tok,
                "package": getattr(v, "__package__", ""),
                "path": [_norm_path_str(p) for p in getattr(v, "__path__", [])]}
    return str(v)


def _norm_entry(e):
    if not isinstance(e, dict):
        return e
    out = {}
    for k, v in e.items():
        out[k] = v.as_posix() if isinstance(v, Path) else v
    return out


def _pairs(d):
    return [[k, v] for k, v in d.items()]


class _NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class _ThreadDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def is_alive(self):
        s = self.ctx.script("bg.is_alive")
        self.ctx.rec("bg.is_alive", {}, s)
        return bool(s)

    def join(self, timeout=None):
        s = self.ctx.script("bg.join")
        self.ctx.rec("bg.join", {"timeout": timeout}, s)
        if _is_raise(s):
            raise _materialize_raise(s)


class _HandlerDouble:
    def __init__(self, ctx, stream):
        self.ctx = ctx
        ctx.rec("stream_handler_new", {"stream": stream}, ctx.script("stream_handler_new"))

    def setLevel(self, level):
        name = {10: "DEBUG", 20: "INFO", 30: "WARNING"}.get(level, str(level))
        self.ctx.rec("handler_set_level", {"level": name}, self.ctx.script("handler_set_level"))

    def setFormatter(self, fmt):
        self.ctx.rec("handler_set_formatter", {"fmt": fmt._fmt}, self.ctx.script("handler_set_formatter"))


class _FormatterDouble:
    def __init__(self, fmt):
        self._fmt = fmt


class _LoggerDouble:
    def __init__(self, ctx):
        self.ctx = ctx
        self._propagate = True

    @property
    def propagate(self):
        return self._propagate

    @propagate.setter
    def propagate(self, v):
        self.ctx.rec("logger_set_propagate", {"propagate": bool(v)}, self.ctx.script("logger_set_propagate"))
        self._propagate = v

    def addHandler(self, h):
        self.ctx.rec("logger_add_handler", {}, self.ctx.script("logger_add_handler"))

    def setLevel(self, level):
        name = {10: "DEBUG", 20: "INFO", 30: "WARNING"}.get(level, str(level))
        self.ctx.rec("logger_set_level", {"level": name}, self.ctx.script("logger_set_level"))

    def _log(self, level, msg, *args):
        text = msg % args if args else msg
        self.ctx.rec("log", {"level": level, "msg": text}, self.ctx.script("log"))

    def debug(self, msg, *args, **kw):
        self._log("DEBUG", msg, *args)

    def info(self, msg, *args, **kw):
        self._log("INFO", msg, *args)

    def warning(self, msg, *args, **kw):
        self._log("WARNING", msg, *args)


class _ManagerDouble:
    pass


class _LoaderDouble:
    def __init__(self, ctx, token):
        self.ctx = ctx
        self.__token__ = token

    def exec_module(self, module):
        s = self.ctx.script("exec_module")
        self.ctx.rec("exec_module", {"spec": self.__token__, "module": module.__token__}, s)
        if _is_raise(s):
            raise _materialize_raise(s)


class _EpDouble:
    def __init__(self, ctx, name, group):
        self.ctx = ctx
        self.name = name
        self.group = group

    def load(self):
        s = self.ctx.script("ep.load")
        self.ctx.rec("ep.load", {"name": self.name}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        return s


class _EpsSelectDouble:
    def __init__(self, ctx, eps):
        self.ctx = ctx
        self.eps = eps

    def select(self, group=None):
        s = self.ctx.script("ep.select")
        self.ctx.rec("ep.select", {"group": group}, s)
        return [_EpDouble(self.ctx, e.get("name", ""), e.get("group", "")) for e in (self.eps or [])]


class _ScopeCtx:
    def __init__(self, ctx, home):
        self.ctx = ctx
        self.home = home

    def __enter__(self):
        self.ctx.rec("home_scope.enter", {"home": self.home}, self.ctx.script("home_scope.enter"))
        return self

    def __exit__(self, *a):
        self.ctx.rec("home_scope.exit", {}, self.ctx.script("home_scope.exit"))
        return False


class _ConfigStub(types.ModuleType):
    """sys.modules['hermes_cli.config'] double: `from ... import load_config`
    fires the import_config seam (a scripted raise models an import failure);
    the returned load_config fires the load_config seam."""

    def __init__(self, ctx):
        super().__init__("hermes_cli.config")
        self.ctx = ctx

    def __getattr__(self, name):
        if name == "load_config":
            s = self.ctx.script("import_config")
            self.ctx.rec("import_config", {}, s)
            if _is_raise(s):
                raise _materialize_raise(s)
            ctx = self.ctx

            def load_config():
                sc = ctx.script("load_config")
                ctx.rec("load_config", {}, sc)
                if _is_raise(sc):
                    raise _materialize_raise(sc)
                return json.loads(json.dumps(sc)) if isinstance(sc, (dict, list)) else sc
            return load_config
        raise AttributeError(name)

# ── per-case live namespace ──

def build_ns(case, ctx, m):
    mod = types.ModuleType("plugin_revive_live")
    sys.modules["plugin_revive_live"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "plugin_revive_live", "__builtins__": builtins})
    for nm, obj in (("Any", Any), ("Optional", Optional), ("Callable", Callable),
                    ("Dict", Dict), ("List", List), ("Mapping", Mapping),
                    ("Set", Set), ("Tuple", Tuple), ("Union", Union),
                    ("dataclass", dataclass), ("field", field), ("suppress", suppress),
                    ("Path", Path), ("hashlib", hashlib), ("types", types),
                    ("json", json)):
        ns[nm] = obj
    # classes first: CAPABILITY_REGISTRY's dict comprehension constructs
    # CapabilitySpec, so the class must exist before constant seeding.
    _exec_node(_find_class("hermes_cli/plugin_capabilities.py", "CapabilitySpec"), ns,
               keep_decorators=True)
    _exec_node(_find_class("hermes_cli/plugins.py", "_PreToolCallDirective"), ns,
               keep_decorators=True)
    _seed_constants(ns)
    for nsname, rel, astname in DEF_PLAN:
        node = _find_def(rel, astname)
        if nsname != astname:
            import copy
            node = copy.deepcopy(node)
            node.name = nsname
        _exec_node(node, ns)

    state = case.get("state") or {}
    inp = case.get("in") or {}

    sys_double = types.SimpleNamespace(modules=dict(state.get("sys_modules") or {}),
                                       stderr="stderr")
    ns["sys"] = sys_double

    def getenv(name, default=None):
        s = ctx.script("getenv")
        ctx.rec("getenv", {"name": name}, s)
        return default if s is None else s
    ns["os"] = types.SimpleNamespace(getenv=getenv)

    ns["logging"] = types.SimpleNamespace(
        StreamHandler=lambda stream: _HandlerDouble(ctx, stream),
        Formatter=_FormatterDouble, DEBUG=10, INFO=20, WARNING=30)
    ns["logger"] = _LoggerDouble(ctx)

    current = object()
    thread_double = _ThreadDouble(ctx) if state.get("bg_present") else None
    ns["threading"] = types.SimpleNamespace(
        current_thread=lambda: thread_double if state.get("bg_is_current") else current,
        RLock=_NoopLock, Lock=_NoopLock)
    ns["_background_discovery_thread"] = thread_double
    ns["_delivery_join_abandoned"] = bool(state.get("delivery_join_abandoned"))
    ns["_PLUGINS_DEBUG"] = bool(state.get("plugins_debug"))
    ns["_DEBUG_HANDLER_INSTALLED"] = bool(state.get("debug_handler_installed"))

    wl = types.SimpleNamespace()
    if state.get("whitelist_allowed") is not None:
        wl.allowed = set(state["whitelist_allowed"])
    if state.get("whitelist_fmt") is not None:
        wl.fmt = state["whitelist_fmt"]
    ns["_thread_tool_whitelist"] = wl

    def get_plugin_manager():
        s = ctx.script("get_plugin_manager")
        ctx.rec("get_plugin_manager", {}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        return m
    ns["get_plugin_manager"] = get_plugin_manager

    def in_plugin_load_worker():
        s = ctx.script("in_plugin_load_worker")
        ctx.rec("in_plugin_load_worker", {}, s)
        return bool(s)
    ns["in_plugin_load_worker"] = in_plugin_load_worker

    ns["_MODULE_NAMESPACE_LOCK"] = _NoopLock()
    ns["_BARE_MODULE_SCOPE"] = dict(state.get("bare_module_scope") or {})

    def lifecycle_invoke_hook(hook_name, **kwargs):
        s = ctx.script("lifecycle.invoke_hook")
        ctx.rec("lifecycle.invoke_hook", {"hook": hook_name, "kwargs": kwargs}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        return json.loads(json.dumps(s)) if isinstance(s, list) else s
    lifecycle_mod = types.ModuleType("hermes_cli.lifecycle")
    lifecycle_mod.invoke_hook = lifecycle_invoke_hook

    def request_tool_approval(tool_name, message, rule_key=None):
        s = ctx.script("approval.request")
        ctx.rec("approval.request",
                {"tool_name": tool_name, "message": message, "rule_key": rule_key}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        return json.loads(json.dumps(s)) if isinstance(s, dict) else s

    def set_obs_ctx(turn_id="", tool_call_id="", session_id=""):
        s = ctx.script("approval.set_ctx")
        ctx.rec("approval.set_ctx",
                {"turn_id": turn_id, "tool_call_id": tool_call_id, "session_id": session_id}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        return s

    def reset_obs_ctx(tokens):
        s = ctx.script("approval.reset_ctx")
        ctx.rec("approval.reset_ctx", {"tokens": tokens}, s)
        if _is_raise(s):
            raise _materialize_raise(s)

    def safe_worker_enabled():
        s = ctx.script("safe_worker_enabled")
        ctx.rec("safe_worker_enabled", {}, s)
        return bool(s)

    approval_mod = types.ModuleType("tools.approval")
    approval_mod.request_tool_approval = request_tool_approval
    approval_ctx_mod = types.ModuleType("tools.approval_context")
    approval_ctx_mod.set_current_observability_context = set_obs_ctx
    approval_ctx_mod.reset_current_observability_context = reset_obs_ctx
    swp_mod = types.ModuleType("agent.safe_worker_policy")
    swp_mod.safe_worker_enabled = safe_worker_enabled

    for parent in ("hermes_cli", "tools", "agent"):
        pm = types.ModuleType(parent)
        pm.__path__ = []
        sys.modules[parent] = pm
    sys.modules["hermes_cli.config"] = _ConfigStub(ctx)
    sys.modules["hermes_cli.lifecycle"] = lifecycle_mod
    sys.modules["tools.approval"] = approval_mod
    sys.modules["tools.approval_context"] = approval_ctx_mod
    sys.modules["agent.safe_worker_policy"] = swp_mod

    ns["_facade"] = lambda: types.SimpleNamespace(
        _plugin_home_scope=lambda home: _ScopeCtx(ctx, home),
        plugin_capability_granted=ns["plugin_capability_granted"])

    def spec_from_file_location(name, location, submodule_search_locations=None):
        s = ctx.script("spec_from_file_location")
        ctx.rec("spec_from_file_location",
                {"module_name": name, "location": str(location),
                 "submodule_search_locations": list(submodule_search_locations or [])}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        if s is None:
            return None
        if isinstance(s, str):
            return types.SimpleNamespace(__token__=s, loader=_LoaderDouble(ctx, s))
        tok = s.get("token")
        loader = None if s.get("loader") is None else _LoaderDouble(ctx, tok)
        return types.SimpleNamespace(__token__=tok, loader=loader)

    def module_from_spec(spec):
        s = ctx.script("module_from_spec")
        ctx.rec("module_from_spec", {"spec": spec.__token__}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        return types.SimpleNamespace(__token__=s)

    def entry_points():
        s = ctx.script("entry_points")
        ctx.rec("entry_points", {}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        if not isinstance(s, dict):
            return s
        kind = s.get("kind", "list")
        if kind == "select":
            return _EpsSelectDouble(ctx, s.get("eps") or [])
        if kind == "dict":
            return {g: [_EpDouble(ctx, e.get("name", ""), e.get("group", ""))
                        for e in eps]
                    for g, eps in (s.get("groups") or {}).items()}
        return [_EpDouble(ctx, e.get("name", ""), e.get("group", ""))
                for e in (s.get("eps") or [])]

    ns["importlib"] = types.SimpleNamespace(
        metadata=types.SimpleNamespace(entry_points=entry_points),
        util=types.SimpleNamespace(spec_from_file_location=spec_from_file_location,
                                   module_from_spec=module_from_spec))
    return ns


def build_manager(case, ns, ctx):
    state = case.get("state") or {}
    m = _ManagerDouble()
    m._hooks = json.loads(json.dumps(state.get("hooks") or {}))
    m._middleware = json.loads(json.dumps(state.get("middleware") or {}))
    skills = {}
    for qn, entry in (state.get("plugin_skills") or {}).items():
        e = json.loads(json.dumps(entry))
        if isinstance(e, dict) and isinstance(e.get("path"), dict) and "__path__" in e["path"]:
            e["path"] = Path(e["path"]["__path__"])
        skills[qn] = e
    m._plugin_skills = skills
    m._plugin_commands = json.loads(json.dumps(state.get("plugin_commands") or {}))
    m._context_engine = state.get("context_engine")
    m.scope_key = state.get("scope_key", "/h/s1")
    m.home_path = state.get("home_path", "/h/s1")
    if state.get("has_discovered_attr", True):
        m._discovered = bool(state.get("discovered"))
    if not state.get("has_middleware_attr", True):
        del m._middleware

    def discover_and_load(force=False):
        s = ctx.script("discover_and_load")
        ctx.rec("discover_and_load", {"force": force}, s)
        if _is_raise(s):
            m._discovered = False
            raise _materialize_raise(s)
        if isinstance(s, dict) and "set_discovered" in s:
            m._discovered = bool(s["set_discovered"])

    def invoke_hook(hook_name, **kwargs):
        s = ctx.script("manager.invoke_hook")
        ctx.rec("manager.invoke_hook", {"hook": hook_name, "kwargs": kwargs}, s)
        if _is_raise(s):
            raise _materialize_raise(s)
        return json.loads(json.dumps(s)) if isinstance(s, list) else s

    m.discover_and_load = discover_and_load
    m.invoke_hook = invoke_hook
    if ns is not None:
        bind_manager_methods(m, ns, state)
    return m


def bind_manager_methods(m, ns, state):
    m.has_hook = types.MethodType(ns["mgr_has_hook"], m)
    if state.get("manager_has_middleware_method", True):
        m.has_middleware = types.MethodType(ns["mgr_has_middleware"], m)
    # _load_directory_module calls self._directory_module_name(manifest).
    m._directory_module_name = types.MethodType(ns["_directory_module_name"], m)


MANIFEST_FNS = {"_load_directory_module", "_load_entrypoint_module",
                "_platform_name_from_manifest"}


def snapshot(case, m, ns):
    state = case.get("state") or {}
    inp = case.get("in") or {}
    fn = case["fn"]
    return {
        "hooks": _pairs(m._hooks),
        "middleware": _pairs(m._middleware) if hasattr(m, "_middleware") else [],
        "plugin_skills": [[k, _norm_entry(v)] for k, v in m._plugin_skills.items()],
        "plugin_commands": _pairs(m._plugin_commands),
        "sys_modules": [[k, _norm_mod(v)] for k, v in ns["sys"].modules.items()],
        "bare_module_scope": _pairs(ns["_BARE_MODULE_SCOPE"]),
        "context_engine": m._context_engine,
        "discovered": bool(getattr(m, "_discovered", False)),
        "delivery_join_abandoned": bool(ns["_delivery_join_abandoned"]),
        "plugins_debug": bool(ns["_PLUGINS_DEBUG"]),
        "debug_handler_installed": bool(ns["_DEBUG_HANDLER_INSTALLED"]),
        "whitelist_allowed": state.get("whitelist_allowed"),
        "whitelist_fmt": state.get("whitelist_fmt"),
        "bg_present": bool(state.get("bg_present")),
        "bg_is_current": bool(state.get("bg_is_current")),
        "has_discovered_attr": hasattr(m, "_discovered"),
        "has_middleware_attr": hasattr(m, "_middleware"),
        "manager_has_middleware_method": bool(state.get("manager_has_middleware_method", True)),
        "scope_key": m.scope_key,
        "home_path": m.home_path,
        "manifest_source": state.get("manifest_source", "user"),
        "plugin_id": state.get("plugin_id", "p1"),
        "load_module_name": (inp.get("module_name") if fn == "_load_directory_module"
                             else state.get("load_module_name")),
        "manifest": (inp.get("manifest") if fn in MANIFEST_FNS
                     else state.get("manifest")),
    }


def call_live(case, ns, m, ctx):
    """Return (ret, extra) in the C projection; exceptions propagate."""
    fn = case["fn"]
    inp = case.get("in") or {}
    state = case.get("state") or {}
    if fn == "_env_enabled":
        return bool(ns["env_var_enabled"](inp.get("name", ""))), {}
    if fn == "_install_plugin_debug_handler":
        ns["_install_plugin_debug_handler"](force=bool(inp.get("force")))
        return None, {}
    if fn == "_tool_override_allowed":
        ctx_obj = types.SimpleNamespace(
            manifest=types.SimpleNamespace(source=state.get("manifest_source", "user")),
            plugin_id=state.get("plugin_id", "p1"), _manager=m)
        return bool(ns["_tool_override_allowed"](ctx_obj, inp.get("name", ""))), {}
    if fn == "_platform_name_from_manifest":
        mf = inp.get("manifest")
        if mf is None:
            raise AttributeError("'NoneType' object has no attribute 'name'")
        return ns["_platform_name_from_manifest"](types.SimpleNamespace(**mf)), {}
    if fn == "_load_directory_module":
        mf = types.SimpleNamespace(**(inp.get("manifest") or {}))
        mod = ns["_load_directory_module"](m, mf, module_name=inp.get("module_name"))
        return mod.__token__, {}
    if fn == "_load_entrypoint_module":
        mf = types.SimpleNamespace(**(inp.get("manifest") or {}))
        return ns["_load_entrypoint_module"](m, mf), {}
    if fn == "has_hook":
        return bool(ns["has_hook"](inp.get("name", ""))), {}
    if fn == "has_middleware":
        return bool(ns["has_middleware"](inp.get("name", ""))), {}
    if fn == "find_plugin_skill":
        r = ns["find_plugin_skill"](m, inp.get("name", ""))
        return (r.as_posix() if isinstance(r, Path) else r), {}
    if fn == "remove_plugin_skill":
        before = inp.get("name", "") in m._plugin_skills
        ns["remove_plugin_skill"](m, inp.get("name", ""))
        return before, {}
    if fn == "get_pre_tool_call_directive":
        action, message = ns["get_pre_tool_call_directive"](
            inp.get("tool_name", ""), inp.get("args"), **(inp.get("hook_kwargs") or {}))
        return action, {"message": message}
    if fn == "get_pre_tool_call_block_message":
        msg = ns["get_pre_tool_call_block_message"](
            inp.get("tool_name", ""), inp.get("args"), **(inp.get("hook_kwargs") or {}))
        return msg, {"message": msg}
    if fn == "resolve_pre_tool_block":
        msg = ns["resolve_pre_tool_block"](
            inp.get("tool_name", ""), inp.get("args"), **(inp.get("hook_kwargs") or {}))
        return msg is not None, {"message": msg}
    if fn == "get_pre_verify_continue_message":
        return ns["get_pre_verify_continue_message"](**(inp.get("kwargs") or {})), {}
    if fn == "_ensure_plugins_discovered":
        ns["_ensure_plugins_discovered"]()
        return bool(getattr(m, "_discovered", False)), {}
    if fn == "get_plugin_context_engine":
        return ns["get_plugin_context_engine"](), {}
    if fn == "get_plugin_command_handler":
        return ns["get_plugin_command_handler"](inp.get("name", "")), {}
    raise RuntimeError(f"oracle: no handler for fn {fn}")


IMPLEMENTED = ["_install_plugin_debug_handler", "_env_enabled", "_tool_override_allowed",
               "_platform_name_from_manifest", "_load_directory_module",
               "_load_entrypoint_module", "has_hook", "has_middleware",
               "find_plugin_skill", "remove_plugin_skill", "get_pre_tool_call_directive",
               "get_pre_tool_call_block_message", "resolve_pre_tool_block",
               "get_pre_verify_continue_message", "_ensure_plugins_discovered",
               "get_plugin_context_engine", "get_plugin_command_handler"]


def run_case(case):
    ctx = CaseCtx(case.get("io"))
    m = build_manager(case, None, ctx)
    ns = build_ns(case, ctx, m)
    bind_manager_methods(m, ns, case.get("state") or {})
    expected = {"ret": None, "extra": {}, "trace": [], "state": {}, "err": None}
    orig_exists = Path.exists

    def fake_exists(self):
        s = ctx.script("fs.exists")
        ctx.rec("fs.exists", {"path": str(self)}, s)
        return bool(s)

    try:
        Path.exists = fake_exists
        try:
            ret, extra = call_live(case, ns, m, ctx)
            expected["ret"] = ret
            expected["extra"] = extra
        except BaseException as exc:  # SystemExit included — fail-closed
            expected["err"] = {"cls": type(exc).__name__, "msg": str(exc)}
        expected["trace"] = _norm_trace(ctx.trace)
        expected["state"] = snapshot(case, m, ns)
        if expected["err"] is not None:
            expected["ret"] = None
            expected["extra"] = {}
            expected["err"]["msg"] = _norm_path_str(expected["err"]["msg"])
    finally:
        Path.exists = orig_exists
    return json.loads(json.dumps(expected))


def main():
    cases = 0
    mismatches = 0
    per_fn = {}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as exc:
            print(f"MISMATCH bad harness line: {exc}", file=sys.stderr)
            mismatches += 1
            cases += 1
            continue
        case = rec.get("case")
        if not isinstance(case, dict) or "fn" not in case:
            print("MISMATCH harness record missing case echo", file=sys.stderr)
            mismatches += 1
            cases += 1
            continue
        cases += 1
        fn = case["fn"]
        per_fn[fn] = per_fn.get(fn, 0) + 1
        try:
            expected = run_case(case)
        except BaseException as exc:
            print(f"FATAL oracle replay failed for {fn}: {type(exc).__name__}: {exc}",
                  file=sys.stderr)
            mismatches += 1
            continue
        got = {"ret": rec.get("ret"), "extra": rec.get("extra") or {},
               "trace": rec.get("trace") or [], "state": rec.get("state") or {},
               "err": rec.get("err")}
        bad = [k for k in ("ret", "extra", "trace", "state", "err") if got[k] != expected[k]]
        if bad:
            mismatches += 1
            print(f"MISMATCH {fn} case#{cases} fields={bad}", file=sys.stderr)
            for k in bad[:2]:
                g = json.dumps(got[k], ensure_ascii=False)
                e = json.dumps(expected[k], ensure_ascii=False)
                print(f"  got {k}: {g[:1200]}", file=sys.stderr)
                print(f"  want {k}: {e[:1200]}", file=sys.stderr)
    thin = [fn for fn in IMPLEMENTED if per_fn.get(fn, 0) < 5]
    if thin:
        print(f"FATAL fixture coverage <5 cases for: {thin}", file=sys.stderr)
        mismatches += 1
    if cases == 0:
        print("FATAL no cases received (harness/link failure in disguise)", file=sys.stderr)
    print(f"ORACLE plugin_revive: {cases} cases, {mismatches} mismatches")
    sys.exit(1 if (mismatches or cases == 0) else 0)


if __name__ == "__main__":
    main()
