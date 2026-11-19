#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_profiles.py FLOW port
(src/gateway/tui_methods_flow.c).

NOTE: the quarry has no tui_gateway/methods.py — the TUI method surface is the
methods_*.py family; this oracle drives the fully UNCLAIMED methods_profiles.py
helper surface (the PoP annotations name the defining module).

Drives the LIVE methods_profiles bodies with SCRIPTED seams: every server
global (_err / _launch_home / _persistence_for / _profile_ui_meta_lock),
claimed sibling (_denied_source), unported sibling
(_profile_group_selection_context), ported sibling seamed in composers
(_resurrect_recoverable_canonical), in-function import callable and callback
arg is a recording stub fed the same scripts the C harness feeds its universal
seam. Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"}
in "trace"), the final return ("ret") and the mutated caller-dict mirror
("state": cfg / applied).

Live defs are AST-extracted from methods_profiles.py (module Assign/AnnAssign
constants seeded, plain-Name targets only; UNDECORATED FunctionDefs exec'd with
the future-annotations flag into a registered sys.modules module — py3.14).
The heavy quarry import chain is never imported. is_truthy_value /
TRUTHY_STRINGS are AST-extracted from utils/env.py and run REAL (the C port
transcribes them). Ported leaf-flows (_try, _best_effort, _lazy,
_pin_profile_model, _hermes_home_scope, _read_profile_yaml, _clean_revisions,
_bots_title, _clean_names, _model_provider_params, _latest_message_preview,
_display_count_field, _mcp_toggle_state) compose LIVE so nested decision logic
is exercised, matching the C port's inline transcriptions.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site.
- STORE_FAILURES is the fixed kind set {TransientStoreError,
  PermanentStoreError, StoreUnavailable, StoreReplaced} (synthetic Exception
  subclasses shared between make_exc and the persistence.core.errors stub).
- RECOVERABLE_END_REASONS = {agent_close, ws_orphan_reap,
  superseded_by_resume, startup_orphan_reap}.
- Persistence/row/writer doubles: scripted dicts become SimpleNamespaces (a
  double is ALWAYS truthy, even {}); null handles raise the live NoneType
  attribute/CM messages at the access site. Row scripts are dicts or null.
- Context-manager markers (profile_config_scope / persistence_for scripts) are
  dicts wrapped in silent enter/exit doubles; a null marker raises TypeError
  at the live `with` site. The marker projects back to its dict for comparison.
- Tuples project to lists; sets to sorted lists; integral floats to ints;
  Paths to POSIX strings (fixture paths are absolute POSIX, no trailing slash,
  no "." components). Fixture numbers are ints; type-gated leaves never see
  float literals; ui_meta values are str/int/bool/null/list/dict (no floats).
- str()/strip()/lower()/split() fold the ASCII fixture corpus only (the
  strip/split whitespace set includes \\x1c-\\x1f); str() sites see
  str/int/bool/null only.
- Seam args drop None-valued keys on both sides (absent-vs-null parity).
- _save_mcp_toggles' `for srv in wanted` set loop: fixtures only copy from
  launch_mcp when wanted has ONE member (set iteration order is otherwise
  unobservable — no seam fires for already-present servers).
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
from pathlib import Path

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
SRC = os.path.join(Q, "tui_gateway", "methods_profiles.py")
ENV_SRC = os.path.join(Q, "utils", "env.py")

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


PORTED = [
    "_model_provider_params", "_clean_revisions", "_clean_names", "_bots_title",
    "_mcp_toggle_state", "_try", "_best_effort", "_lazy", "_pin_profile_model",
    "_hermes_home_scope", "_resolve_profile", "_read_profile_yaml",
    "_latest_message_preview", "_display_count_field", "_live_count_field",
    "_mcp_entry_enabled", "_save_toolset_pin", "_save_mcp_toggles",
    "_unlink_asset_files", "_landed", "_configure_model", "_configure_ui_meta",
    "_resurrect_recoverable_canonical", "_canonical_session_row",
]


# ── methods_profiles.py namespace (live defs + seeded constants) ──
def extract():
    mod = new_module("tm_oracle_methods_profiles")
    ns = mod.__dict__
    import contextlib
    import threading
    ns.update({"contextlib": contextlib, "sys": sys, "os": os, "re": re,
               "json": json, "threading": threading, "Path": Path,
               "types": types, "copy": copy})
    # method_ctx stubs (the registry decorators are never driven)
    ns["HandlerRegistry"] = lambda: types.SimpleNamespace(
        method=lambda name: (lambda fn: fn), profile_scoped=lambda fn: fn)
    ns["bind_module"] = lambda *a, **k: None
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            exec_node(src, node, ns, SRC)
    required = PORTED + ["_profile_group_selection_context", "_ASSET_EXTS",
                         "_ASSET_MAGIC", "_ABSENT"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_flow: methods_profiles missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── is_truthy_value / TRUTHY_STRINGS (real, from utils/env.py) ──
def extract_truthy():
    src = open(ENV_SRC, encoding="utf-8").read()
    ns = {"Any": object}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "TRUTHY_STRINGS" for t in node.targets):
            exec_node(src, node, ns, ENV_SRC)
        if isinstance(node, ast.FunctionDef) and node.name == "is_truthy_value":
            exec_node(src, node, ns, ENV_SRC)
    if "is_truthy_value" not in ns or "TRUTHY_STRINGS" not in ns:
        print("FATAL oracle_tui_methods_flow: is_truthy_value not found")
        sys.exit(2)
    NS["is_truthy_value"] = ns["is_truthy_value"]
    NS["TRUTHY_STRINGS"] = ns["TRUTHY_STRINGS"]


extract_truthy()


# ── exception materialization / projection ──
TransientStoreError = type("TransientStoreError", (Exception,), {})
PermanentStoreError = type("PermanentStoreError", (Exception,), {})
StoreUnavailable = type("StoreUnavailable", (Exception,), {})
StoreReplaced = type("StoreReplaced", (Exception,), {})
STORE_FAILURES = (TransientStoreError, PermanentStoreError, StoreUnavailable, StoreReplaced)
RECOVERABLE_END_REASONS = frozenset(
    {"agent_close", "ws_orphan_reap", "superseded_by_resume", "startup_orphan_reap"})

EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "PermissionError": PermissionError, "OSError": OSError, "IndexError": IndexError,
    "Exception": Exception, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": asyncio.CancelledError,
    "TransientStoreError": TransientStoreError,
    "PermanentStoreError": PermanentStoreError,
    "StoreUnavailable": StoreUnavailable, "StoreReplaced": StoreReplaced,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── canonical projection (compare parsed JSON, never raw strings) ──
class SilentCM:
    """Silent context-manager double carrying its script marker."""
    def __init__(self, marker):
        self.__cm_marker__ = marker

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class PersistenceDouble:
    def __init__(self, sessions, messages, lineage):
        self.sessions = sessions
        self.messages = messages
        self.lineage = lineage

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def canon(v):
    def conv(x):
        if isinstance(x, SilentCM):
            return conv(x.__cm_marker__)
        if isinstance(x, LazyCallable):
            return conv(x.__lazy_marker__)
        if isinstance(x, types.SimpleNamespace):
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


# ── Path filesystem methods are seams (is_dir / is_file / read_text / unlink) ──
# The live bodies call real Path methods; route them through the current case's
# script backend so the C port's "is_dir"/"is_file"/"read_text"/"unlink" seams
# fire identically. Path construction / as_posix stay real.
_CUR_CTX = None


def _path_is_dir(self):
    return _CUR_CTX.call("is_dir", {"path": self.as_posix()})


def _path_is_file(self):
    return _CUR_CTX.call("is_file", {"path": self.as_posix()})


def _path_read_text(self, encoding=None, errors=None):
    return _CUR_CTX.call("read_text", {"path": self.as_posix(), "encoding": encoding})


def _path_unlink(self, missing_ok=False):
    return _CUR_CTX.call("unlink", {"path": self.as_posix()})


Path.is_dir = _path_is_dir
Path.is_file = _path_is_file
Path.read_text = _path_read_text
Path.unlink = _path_unlink


# ── per-case script backend (mirrors the C tm_call exactly) ──
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
        # C returns a fresh copy per call; live flows mutate seam results in
        # place, so the script entry must stay pristine.
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── attribute doubles ──
def row_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def persistence_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    return PersistenceDouble(
        sessions=types.SimpleNamespace(
            get=rec(ctx, "sessions_get", lambda session_id: {"session_id": session_id},
                    build=row_build),
            get_by_title=rec(ctx, "get_by_title", lambda title: {"title": title},
                             build=row_build)),
        messages=types.SimpleNamespace(
            latest_dialogue_message=rec(
                ctx, "latest_dialogue_message",
                lambda session_id: {"session_id": session_id}),
            display_count=rec(ctx, "display_count",
                              lambda session_id: {"session_id": session_id})),
        lineage=types.SimpleNamespace(
            tip=rec(ctx, "lineage_tip", lambda session_id: {"session_id": session_id})))


def writer_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    return types.SimpleNamespace(
        sessions=types.SimpleNamespace(
            unarchive_recoverable=rec(
                ctx, "unarchive_recoverable",
                lambda session_id: {"session_id": session_id})),
        close=rec(ctx, "writer_close", lambda: {}))


def cm_build(raw):
    return SilentCM(raw) if isinstance(raw, dict) else raw


class LazyCallable:
    """_lazy's late-bound attribute: a callable double carrying its script
    marker. Direct _lazy cases project to the marker; composed callers
    (_pin_profile_model / _configure_model) CALL it, firing the target seam."""
    def __init__(self, marker, fn):
        self.__lazy_marker__ = marker
        self._fn = fn

    def __call__(self, *a, **k):
        if self._fn is None:
            raise TypeError("'%s' object is not callable" % type(self.__lazy_marker__).__name__)
        return self._fn(*a, **k)


def lazy_targets(ctx):
    return {
        ("hermes_cli.profile_provision", "write_profile_model"): rec(
            ctx, "write_profile_model",
            lambda profile_dir, provider, model, base_url="":
            {"profile_dir": profile_dir.as_posix() if isinstance(profile_dir, Path) else profile_dir,
             "provider": provider, "model": model, "base_url": base_url}),
        ("hermes_cli.model_selection_guards", "combined_selection_warning"): rec(
            ctx, "combined_selection_warning",
            lambda model, provider=None, selection_context=None:
            {"model": model, "provider": provider, "selection_context": selection_context},
            build=lambda raw: types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw),
    }


def lazy_rec(ctx):
    targets = lazy_targets(ctx)

    def f(module, name):
        raw = ctx.call("import_attr", {"module": module, "name": name})
        return LazyCallable(raw, targets.get((module, name)))
    return f


class RecLogger:
    def __init__(self, ctx):
        self.ctx = ctx

    def info(self, fmt, *args):
        return self.ctx.call("logger_info", {"msg": fmt % args})


class LoggingDouble:
    def __init__(self, ctx):
        self.ctx = ctx

    def getLogger(self, name):
        return RecLogger(self.ctx)


# ── in-function import stub modules (registered once, rebound per case) ──
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


PROFILES_MOD = stub_mod("hermes_cli.profiles")
TOOLSCFG_MOD = stub_mod("hermes_cli.tools_config")
CONFIG_MOD = stub_mod("hermes_cli.config")
YAML_MOD = stub_mod("hermes_yaml")
MCPCOMMON_MOD = stub_mod("tools.mcp_tool_common")
ERRORS_MOD = stub_mod("persistence.core.errors")
SESSIONS_MOD = stub_mod("persistence.conversation.sessions")
ASSEMBLY_MOD = stub_mod("persistence_assembly")
UTILS_MOD = stub_mod("utils")
POLICY_MOD = stub_mod("tui_gateway.launch_profile_policy")
ERRORS_MOD.STORE_FAILURES = STORE_FAILURES
SESSIONS_MOD.RECOVERABLE_END_REASONS = RECOVERABLE_END_REASONS

# ── real defs restored before every case (seam slots are per-case) ──
REAL = {nm: NS[nm] for nm in PORTED}
SEAMED_SIBLINGS = ("_resurrect_recoverable_canonical",)
CFG_FUNCS = {"_save_toolset_pin", "_save_mcp_toggles"}
APPLIED_FUNCS = {"_configure_model", "_configure_ui_meta"}


def install_stubs(ctx):
    NS["_err"] = rec(ctx, "err",
                     lambda rid, code, msg, data=None:
                     {"rid": rid, "code": code, "message": msg, "data": data})
    NS["_launch_home"] = rec(ctx, "launch_home", lambda: {})
    NS["_persistence_for"] = rec(ctx, "persistence_for", lambda db: {"db": db},
                                 build=lambda raw: persistence_build(raw, ctx))
    NS["_denied_source"] = rec(ctx, "denied_source", lambda row: {"row": row})
    NS["_profile_group_selection_context"] = rec(
        ctx, "profile_group_selection_context",
        lambda profile_dir, target_model:
        {"profile_dir": profile_dir.as_posix(), "target_model": target_model})
    NS["_resurrect_recoverable_canonical"] = rec(
        ctx, "resurrect_recoverable_canonical",
        lambda persistence, profile_path, session_id:
        {"profile_path": profile_path, "session_id": session_id})
    # _lazy is a SEAMED helper: the C port models the late-bound lookup as the
    # import_attr seam; composed callers call the returned callable double.
    NS["_lazy"] = lazy_rec(ctx)
    NS["logging"] = LoggingDouble(ctx)
    NS["_profile_ui_meta_lock"] = SilentCM({})
    PROFILES_MOD.get_profile_dir = rec(ctx, "get_profile_dir", lambda name: {"name": name})
    TOOLSCFG_MOD._save_platform_tools = rec(
        ctx, "save_platform_tools",
        lambda cfg, platform, wanted:
        {"cfg": cfg, "platform": platform, "wanted": sorted(wanted)})
    CONFIG_MOD.read_user_config_raw = rec(ctx, "read_user_config_raw", lambda: {})
    YAML_MOD.safe_load = rec(ctx, "yaml_safe_load", lambda text: {"text": text})
    MCPCOMMON_MOD.mcp_server_enabled = rec(ctx, "mcp_server_enabled",
                                           lambda entry: {"entry": entry})
    MCPCOMMON_MOD.write_mcp_server_enabled = rec(
        ctx, "write_mcp_server_enabled",
        lambda entry, enabled: {"entry": entry, "enabled": enabled})
    ASSEMBLY_MOD.profile_scope = rec(ctx, "profile_scope",
                                     lambda home: {"home": home.as_posix()})
    ASSEMBLY_MOD.open_persistence = rec(ctx, "open_persistence",
                                        lambda scope: {"scope": scope},
                                        build=lambda raw: writer_build(raw, ctx))
    UTILS_MOD.atomic_yaml_write = rec(
        ctx, "atomic_yaml_write",
        lambda path, doc, sort_keys:
        {"path": path.as_posix(), "doc": doc, "sort_keys": sort_keys})
    POLICY_MOD.profile_config_scope = rec(
        ctx, "profile_config_scope",
        lambda path, launch_home: {"path": path.as_posix(), "launch_home": launch_home},
        build=cm_build)


def drive(func, inp):
    global _CUR_CTX
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CUR_CTX = ctx
    state_in = copy.deepcopy(inp.get("state") or {})
    for nm, real in REAL.items():
        NS[nm] = real
    install_stubs(ctx)
    # The case TARGET must run REAL even when it is also a seamed sibling.
    if func in SEAMED_SIBLINGS:
        NS[func] = REAL[func]
    save_config = rec(ctx, "save_config", lambda cfg: {"cfg": cfg})
    try:
        if func == "_model_provider_params":
            ret = NS[func](args.get("params"))
        elif func == "_clean_revisions":
            ret = NS[func](args.get("raw"))
        elif func == "_clean_names":
            ret = NS[func](args.get("values"))
        elif func == "_bots_title":
            ret = NS[func](args.get("ui_meta"))
        elif func == "_mcp_toggle_state":
            ret = NS[func](args.get("servers"))
        elif func == "_try":
            ret = NS[func](rec(ctx, "try_fn", lambda: {}), args.get("default"))
        elif func == "_best_effort":
            ret = NS[func](rec(ctx, "effort_fn", lambda: {}))
        elif func == "_lazy":
            ret = NS[func](args.get("module"), args.get("name"))
        elif func == "_pin_profile_model":
            ret = NS[func](Path(args["profile_dir"]), args.get("provider"),
                           args.get("model"), base_url=args.get("base_url", ""))
        elif func == "_hermes_home_scope":
            ret = NS[func](Path(args["path"]))
        elif func == "_resolve_profile":
            ret = NS[func](args.get("rid"), args.get("params"))
        elif func == "_read_profile_yaml":
            ret = NS[func](Path(args["profile_dir"]))
        elif func == "_latest_message_preview":
            ret = NS[func](persistence_build(args.get("persistence"), ctx),
                           args.get("session_id"))
        elif func == "_display_count_field":
            ret = NS[func](persistence_build(args.get("persistence"), ctx),
                           args.get("session_id"))
        elif func == "_live_count_field":
            ret = NS[func](args.get("db"), args.get("session_id"))
        elif func == "_mcp_entry_enabled":
            ret = NS[func](args.get("entry"))
        elif func == "_save_toolset_pin":
            cfg = state_in.setdefault("cfg", {})
            ret = NS[func](cfg, args.get("enabled"), save_config)
        elif func == "_save_mcp_toggles":
            cfg = state_in.setdefault("cfg", {})
            ret = NS[func](cfg, args.get("enabled"), args.get("launch_mcp"), save_config)
        elif func == "_unlink_asset_files":
            ret = NS[func](Path(args["assets_dir"]), args.get("asset"))
        elif func == "_landed":
            key = args.get("key", "identity")
            keyfn = NS["_mcp_toggle_state"] if key == "mcp_toggle_state" else (lambda v: v)
            ret = NS[func](args.get("cfg"), tuple(args.get("path") or []), keyfn)
        elif func == "_configure_model":
            applied = state_in.setdefault("applied", {})
            ret = NS[func](Path(args["profile_dir"]), args.get("params"), applied)
        elif func == "_configure_ui_meta":
            applied = state_in.setdefault("applied", {})
            ret = NS[func](Path(args["profile_dir"]), args.get("params"), applied)
        elif func == "_resurrect_recoverable_canonical":
            ret = NS[func](persistence_build(args.get("persistence"), ctx),
                           args.get("profile_path"), args.get("session_id"))
        elif func == "_canonical_session_row":
            ret = NS[func](persistence_build(args.get("persistence"), ctx),
                           args.get("profile_path"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state_out = dict(state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "err", "import_attr", "write_profile_model", "launch_home",
    "profile_config_scope", "get_profile_dir", "is_dir", "is_file", "read_text",
    "yaml_safe_load", "latest_dialogue_message", "display_count",
    "persistence_for", "mcp_server_enabled", "save_platform_tools",
    "save_config", "write_mcp_server_enabled", "unlink", "read_user_config_raw",
    "denied_source", "resurrect_recoverable_canonical", "get_by_title",
    "sessions_get", "lineage_tip", "profile_scope", "open_persistence",
    "unarchive_recoverable", "writer_close", "profile_group_selection_context",
    "combined_selection_warning", "atomic_yaml_write", "logger_info",
    "try_fn", "effort_fn",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tm_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n\s*ctx,\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_flow.jsonl"
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
            print(f"FATAL oracle_tui_methods_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_tui_methods_flow: fixture funcs outside PORTED: {sorted(badf)}")
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
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE tui_methods_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_flow.jsonl"
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
        # uniform AttributeError family naming a ported method / seam stub
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            if any(f"'{nm}'" in msg for nm in ported_set):
                bad += 1
                print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_model_provider_params", {"args": {"params": {"model": " m ", "provider": 5}}})
    assert spot["ret"] == ["m", "5"], spot
    spot = drive("_clean_revisions", {"args": {"raw": {"a": -3, "b": True, "c": "x", "d": 7}}})
    assert spot["ret"] == {"a": 0, "d": 7}, spot
    spot = drive("_clean_names", {"args": {"values": [" b ", "a", "", "b", None]}})
    assert spot["ret"] == ["None", "a", "b"], spot
    spot = drive("_bots_title", {"args": {"ui_meta": {"hermes-bots": {"title": "T"}}}})
    assert spot["ret"] == "T", spot
    spot = drive("_mcp_toggle_state", {"args": {"servers": {"s": {"enabled": True}}}})
    assert spot["ret"] == {"s": [True, "<absent>"]}, spot
    spot = drive("_try", {"args": {"default": 9}, "io": {"try_fn": {"__script__": True,
                 "raise": {"type": "ValueError", "message": "x"}}}})
    assert spot["ret"] == 9, spot
    spot = drive("_best_effort", {"io": {"effort_fn": 1}})
    assert spot["ret"] is True, spot
    spot = drive("_latest_message_preview",
                 {"args": {"persistence": {}, "session_id": "s"},
                  "io": {"latest_dialogue_message": "  a   b  "}})
    assert spot["ret"] == "a b", spot
    spot = drive("_display_count_field",
                 {"args": {"persistence": {}, "session_id": 5},
                  "io": {"display_count": {"__script__": True,
                         "raise": {"type": "StoreUnavailable", "message": "locked"}}}})
    assert spot["ret"] == {}, spot
    spot = drive("_resolve_profile", {"args": {"rid": "r1", "params": {"name": "  "}},
                                      "io": {"err": {"jsonrpc": "2.0", "id": "r1",
                                                     "error": {"code": 4063, "message": "name required"}}}})
    assert spot["ret"][0] == "" and spot["trace"][0]["name"] == "err", spot
    thin = [f for f in PORTED if counts.get(f, 0) < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()