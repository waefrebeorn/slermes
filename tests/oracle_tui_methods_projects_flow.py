#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_projects.py FLOW port
(src/gateway/tui_methods_projects_flow.c).

Drives the LIVE methods_projects bodies (Projects RPC surface, repo discovery,
sidebar-tree inputs) with SCRIPTED seams: every server global (_ok/_err/
_load_cfg/_completion_cwd/_persistence_for/logger/git_probe), every in-function
import callable (projects_db methods, get_hermes_home, DEFAULT_CONFIG,
record_feature_used, SessionQuery/SessionWindow, project_tree.build_tree), the
os filesystem probes (realpath/expanduser/isdir) and os.walk are recording
stubs fed the same scripts the C harness feeds its universal seam. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the
final return ("ret") and the mutated module-global mirror ("state":
_DIR_EXISTS_CACHE / _registry_pending).

Live defs are AST-extracted from methods_projects.py (module Assign/AnnAssign
constants seeded, plain-Name AND tuple-of-Name targets; the _NoProject ClassDef
and the real HandlerRegistry from method_ctx.py are extracted too; UNDECORATED
FunctionDefs exec'd with the future-annotations flag into a registered
sys.modules module -- py3.14). Decorated `def _` handlers are extracted BARE
(decorators stripped) under the _h_* names. The heavy quarry import chain is
never imported.

Ported siblings compose LIVE on both sides (each is exercised by its own
cases): _pick / _require_project / _projects_payload / _non_workspace_dirs /
_is_repo_junk / _is_session_cwd_junk / _repo_discovery_policy /
_repo_discovery_policy_key / _repo_discovery_policy_is_default /
_project_tree_row / _discover_repos_payload / _project_tree_inputs.

os double: the PURE posixpath algorithms (normcase/join/dirname/basename/
abspath) run REAL (POSIX fixture corpus, os.sep == "/"); realpath/expanduser/
isdir are seams. os.walk is a seam returning a scripted TREE node
{"dirs": {name: subtree}, "files": [...]}; the double walks it top-down
honoring the caller's dirnames[:] pruning exactly as os.walk (yield order =
dict insertion order). json.dumps(sort_keys=True, separators=(",",":")) runs
REAL here; the C port transcribes it over the ASCII corpus.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_projects_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError/BaseExceptionGroup/BaseException) pass through every
  except-Exception site. Unknown custom kinds default to Exception subclasses.
- Scripted raises of plain kinds carry str(exc) == message verbatim. Scripted
  KeyError raises are OUTSIDE the fixture domain (KeyError str() is repr(key);
  the C-emitted KeyError specs carry repr(key) exactly).
- Object doubles (project/entry/row/persistence) are scripted dicts read as
  attributes: a MISSING key raises AttributeError "'types.SimpleNamespace'
  object has no attribute '<key>'". An EMPTY dict project double is TRUTHY
  (SimpleNamespace(**{}) is truthy); only null is falsy for `if proj`.
- Tuples project to lists; sets to sorted lists; integral floats to ints.
- str()-projected values are str|int|null in the fixture corpus; int()/float()
  coercion accepts numbers, bools and simple numeric strings.
- Fixture paths are absolute POSIX; normcase is the POSIX identity.
- Seam args drop None-valued keys on both sides (absent-vs-null parity).
"""
import ast
import asyncio
import contextlib
import copy
import json
import os
import posixpath
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
SRC = os.path.join(Q, "tui_gateway", "methods_projects.py")
CTX_SRC = os.path.join(Q, "tui_gateway", "method_ctx.py")

_FLAGS = _fut.annotations.compiler_flag

PORTED = [
    "_pick", "_project_tree_row", "_non_workspace_dirs", "_is_repo_junk",
    "_is_session_cwd_junk", "_repo_discovery_policy", "_repo_discovery_policy_key",
    "_repo_discovery_policy_is_default", "_dir_exists_cached", "_require_project",
    "_projects_payload", "_projects_method", "_register_project_mutator",
    "_h_projects_list", "_h_projects_get", "_h_projects_create",
    "_h_projects_archive", "_h_projects_delete", "_h_projects_set_active",
    "_h_projects_for_cwd", "_scan_discovered_repos_remote",
    "_discover_repos_payload", "_project_tree_inputs", "_build_project_tree",
]

# fixture func name -> live def name (the bare handlers are all `def _`)
KINDS = {f: ("_" if f.startswith("_h_") else f) for f in PORTED}

HANDLERS = {
    "projects.list": "_h_projects_list",
    "projects.get": "_h_projects_get",
    "projects.create": "_h_projects_create",
    "projects.archive": "_h_projects_archive",
    "projects.delete": "_h_projects_delete",
    "projects.set_active": "_h_projects_set_active",
    "projects.for_cwd": "_h_projects_for_cwd",
}


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


def stub_pkg(name):
    if name not in sys.modules:
        new_module(name)


def stub_mod(full):
    parts = full.split(".")
    for i in range(1, len(parts)):
        stub_pkg(".".join(parts[:i]))
    mod = new_module(full) if full not in sys.modules else sys.modules[full]
    parent = sys.modules.get(".".join(parts[:-1]))
    if parent is not None:
        setattr(parent, parts[-1], mod)
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            ok = all(isinstance(t, ast.Name) for t in tgts) or (
                len(tgts) == 1 and isinstance(tgts[0], ast.Tuple)
                and all(isinstance(e, ast.Name) for e in tgts[0].elts))
            if not ok:
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


# ── methods_projects.py namespace (live defs + seeded constants) ──
def extract():
    mod = new_module("tmp_oracle_methods_projects")
    ns = mod.__dict__
    ns.update({"contextlib": contextlib, "sys": sys, "os": os, "json": json,
               "types": types, "Path": Path})
    # the real HandlerRegistry (extracted from method_ctx.py) so the deferred
    # registration list is observable; bind_module is install plumbing (unused)
    ctx_src = open(CTX_SRC, encoding="utf-8").read()
    for node in ast.parse(ctx_src).body:
        if isinstance(node, ast.ClassDef) and node.name == "HandlerRegistry":
            exec_node(ctx_src, node, ns, CTX_SRC)
    if "HandlerRegistry" not in ns:
        print("FATAL oracle_tui_methods_projects_flow: HandlerRegistry not found")
        sys.exit(2)
    ns["bind_module"] = lambda *a, **k: None
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # the _NoProject exception class (identity shared with make_exc)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_NoProject":
            exec_node(src, node, ns, SRC)
    seed_constants(src, ns, SRC)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            exec_node(src, node, ns, SRC)
    # decorated `def _` handlers: strip decorators, rename, exec the bare body
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_" and node.decorator_list:
            rpc = None
            for d in node.decorator_list:
                if isinstance(d, ast.Call) and d.args and isinstance(d.args[0], ast.Constant):
                    rpc = d.args[0].value
                    break
            if rpc not in HANDLERS:
                print(f"FATAL oracle_tui_methods_projects_flow: unknown handler {rpc}")
                sys.exit(2)
            name = HANDLERS[rpc]
            text = "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])
            text = text.replace("def _(", f"def {name}(", 1)
            exec(compile(text, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
    required = PORTED + ["_NoProject", "_registry", "method",
                         "_E_PROJECTS", "_E_NO_PROJECT", "_E_PROJECT_ARG",
                         "_MESSAGING_SESSION_SOURCES", "_PROJECT_TREE_EXCLUDED_SOURCES",
                         "_DIR_EXISTS_CACHE"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_projects_flow: methods_projects missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── exception materialization / projection ──
EXC_CLASSES = {
    "BaseException": BaseException,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "Exception": Exception, "ExceptionGroup": ExceptionGroup,
    "BaseExceptionGroup": BaseExceptionGroup,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError,
    "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "TypeError": TypeError,
    "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
    "_NoProject": NS["_NoProject"],
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
class QDouble:
    """SessionQuery/SessionWindow ctor double: projects back to its kwargs."""
    def __init__(self, kw):
        self.__kwargs__ = kw


def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return proj_exc(x)
        if isinstance(x, QDouble):
            return conv(x.__kwargs__)
        if isinstance(x, types.SimpleNamespace):
            if hasattr(x, "__proj__"):
                return conv(x.__proj__)
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


# ── object doubles ──
class YieldCM:
    """CM double whose __enter__ yields the script value (connect_closing /
    persistence_for model the YIELDED handle; null == yielded None)."""
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self.value

    def __exit__(self, *a):
        return False


def proj_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    ns = types.SimpleNamespace(**raw)
    ns.__proj__ = raw
    ns.to_dict = rec(ctx, "to_dict", lambda: {})
    return ns


def entry_build(raw):
    if not isinstance(raw, dict):
        return raw
    d = dict(raw)
    if isinstance(d.get("session"), dict):
        d["session"] = types.SimpleNamespace(**d["session"])
    return types.SimpleNamespace(**d)


def persistence_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    sessions = types.SimpleNamespace(
        distinct_session_cwds=rec(
            ctx, "persistence.distinct_session_cwds", lambda: {},
            build=lambda rows: [types.SimpleNamespace(**r) if isinstance(r, dict) else r
                                for r in rows] if isinstance(rows, list) else rows),
        backfill_repo_roots=rec(
            ctx, "persistence.backfill_repo_roots",
            lambda cwd_to_root: {"cwd_to_root": cwd_to_root}),
        list_sessions=rec(
            ctx, "persistence.list_sessions",
            lambda query, window, compact_rows:
            {"query": query.__kwargs__, "window": window.__kwargs__,
             "compact_rows": compact_rows},
            build=lambda rows: [entry_build(r) for r in rows]
            if isinstance(rows, list) else rows))
    d = {k: raw[k] for k in ("read_only",) if k in raw}
    return types.SimpleNamespace(sessions=sessions, **d)


def walk_double(root, node):
    """os.walk over a scripted tree, honoring dirnames[:] pruning exactly as
    the real top-down walk (iterate the SAME list the caller mutates)."""
    if not isinstance(node, dict):
        return
    dirs = node.get("dirs") or {}
    dirnames = list(dirs)
    yield (root, dirnames, list(node.get("files") or []))
    for d in dirnames:
        sub = dirs.get(d)
        if sub is not None:
            yield from walk_double(root + "/" + d, sub)


def os_double(ctx):
    def walk_stub(root):
        raw = ctx.call("os.walk", {"root": root})
        return walk_double(root, raw)
    return types.SimpleNamespace(
        sep="/",
        path=types.SimpleNamespace(
            expanduser=rec(ctx, "os.path.expanduser", lambda path: {"path": path}),
            realpath=rec(ctx, "os.path.realpath", lambda path: {"path": path}),
            isdir=rec(ctx, "os.path.isdir", lambda path: {"path": path}),
            normcase=posixpath.normcase,
            join=posixpath.join,
            dirname=posixpath.dirname,
            basename=posixpath.basename,
            abspath=posixpath.abspath),
        walk=walk_stub)


def git_probe_double(ctx):
    def resolve_marker(*a, **k):
        raise AssertionError("git_probe.resolve must not be called in fixtures")
    return types.SimpleNamespace(
        branch=rec(ctx, "git_probe.branch", lambda cwd: {"cwd": cwd}),
        warm_roots=rec(ctx, "git_probe.warm_roots",
                       lambda cwds, max_workers=None: {"roots": list(cwds)}),
        common_repo_root=rec(ctx, "git_probe.common_repo_root", lambda cwd: {"cwd": cwd}),
        resolve=resolve_marker)


PICK_KEYS = ("slug", "primary_path", "description", "icon", "color", "board_slug")

# ── in-function import stub modules (registered once, rebound per case) ──
PDB_MOD = stub_mod("hermes_cli.projects_db")
CONFIG_MOD = stub_mod("hermes_cli.config")
CONST_MOD = stub_mod("hermes_constants")
SIGNALS_MOD = stub_mod("hermes_cli.observability.shared_metrics_signals")
SESS_MOD = stub_mod("persistence.conversation.sessions")
PTREE_MOD = stub_mod("tui_gateway.project_tree")

CUR = {}


def _config_getattr(name):
    if name == "DEFAULT_CONFIG":
        return CUR["ctx"].call("default_config", {})
    raise AttributeError(f"module 'hermes_cli.config' has no attribute '{name}'")


CONFIG_MOD.__getattr__ = _config_getattr


def install_stubs(ctx, args):
    CUR["ctx"] = ctx
    NS["_ok"] = rec(ctx, "ok", lambda rid, result: {"rid": rid, "result": result})
    NS["_err"] = rec(ctx, "err", lambda rid, code, msg: {"rid": rid, "code": code, "message": msg})
    NS["_load_cfg"] = rec(ctx, "load_cfg", lambda: {})
    NS["_completion_cwd"] = rec(ctx, "completion_cwd", lambda arg: {"arg": arg})
    NS["_persistence_for"] = rec(ctx, "persistence_for", lambda db: {"db": db},
                                 build=lambda raw: YieldCM(raw))
    NS["logger"] = types.SimpleNamespace(
        debug=rec(ctx, "logger.debug",
                  lambda fmt, *a, exc_info=None:
                  {"fmt": fmt, "args": list(a), "exc_info": exc_info}))
    NS["git_probe"] = git_probe_double(ctx)
    NS["os"] = os_double(ctx)
    CONST_MOD.get_hermes_home = rec(ctx, "get_hermes_home", lambda: {})
    SIGNALS_MOD.record_feature_used = rec(ctx, "record_feature_used",
                                          lambda feature: {"feature": feature})
    PDB_MOD.connect_closing = rec(ctx, "connect_closing", lambda: {},
                                  build=lambda raw: YieldCM(raw))
    PDB_MOD.list_projects = rec(
        ctx, "pdb.list_projects",
        lambda conn, include_archived=False:
        {"conn": conn, "include_archived": include_archived},
        build=lambda raw: [proj_build(r, ctx) for r in raw]
        if isinstance(raw, list) else raw)
    PDB_MOD.get_active_id = rec(ctx, "pdb.get_active_id", lambda conn: {"conn": conn})
    PDB_MOD.get_project = rec(ctx, "pdb.get_project",
                              lambda conn, id: {"conn": conn, "id": id},
                              build=lambda raw: proj_build(raw, ctx))
    PDB_MOD.create_project = rec(
        ctx, "pdb.create_project",
        lambda conn, name, folders, **kw:
        {"conn": conn, "name": name, "folders": folders,
         **{k: kw[k] for k in PICK_KEYS if k in kw}})
    PDB_MOD.set_active = rec(ctx, "pdb.set_active", lambda conn, id: {"conn": conn, "id": id})
    PDB_MOD.archive_project = rec(ctx, "pdb.archive_project", lambda conn, id: {"conn": conn, "id": id})
    PDB_MOD.restore_project = rec(ctx, "pdb.restore_project", lambda conn, id: {"conn": conn, "id": id})
    PDB_MOD.delete_project = rec(ctx, "pdb.delete_project", lambda conn, id: {"conn": conn, "id": id})
    PDB_MOD.project_for_path = rec(ctx, "pdb.project_for_path",
                                   lambda conn, cwd, include_archived=False:
                                   {"conn": conn, "cwd": cwd},
                                   build=lambda raw: proj_build(raw, ctx))
    PDB_MOD.record_discovered_repos = rec(
        ctx, "pdb.record_discovered_repos",
        lambda conn, pairs, replace, policy_key:
        {"conn": conn, "pairs": pairs, "replace": replace, "policy_key": policy_key})
    PDB_MOD.list_discovered_repos = rec(ctx, "pdb.list_discovered_repos",
                                        lambda conn: {"conn": conn})
    PDB_MOD.reconcile_discovered_repos_policy = rec(
        ctx, "pdb.reconcile_discovered_repos_policy",
        lambda conn, policy_key, preserve_unversioned:
        {"conn": conn, "policy_key": policy_key,
         "preserve_unversioned": preserve_unversioned})
    fn_name = args.get("fn_name")
    if fn_name:
        setattr(PDB_MOD, fn_name, rec(
            ctx, "pdb.mutator",
            lambda conn, pid, *a, _fn=fn_name, **k:
            {"fn_name": _fn, "conn": conn, "id": pid, "args": list(a), "kwargs": k}))
    SESS_MOD.SessionOrder = types.SimpleNamespace(LAST_ACTIVE="LAST_ACTIVE")

    def query_ctor(**kw):
        ctx.call("SessionQuery", kw)
        return QDouble(kw)

    def window_ctor(**kw):
        ctx.call("SessionWindow", kw)
        return QDouble(kw)

    SESS_MOD.SessionQuery = query_ctor
    SESS_MOD.SessionWindow = window_ctor

    def build_tree_args(projects, sessions, discovered, resolve=None, *,
                        preview_limit=3, hydrate=False, is_junk_root=None,
                        is_junk_cwd=None, exists=None):
        return {"projects": projects, "sessions": sessions, "discovered": discovered,
                "resolve": "git_probe.resolve", "preview_limit": preview_limit,
                "hydrate": hydrate,
                "is_junk_root": is_junk_root.__name__ if is_junk_root else None,
                "is_junk_cwd": is_junk_cwd.__name__ if is_junk_cwd else None,
                "exists": exists.__name__ if exists else None}

    PTREE_MOD.build_tree = rec(ctx, "project_tree.build_tree", build_tree_args)
    # fresh deferred registry per case (the real extracted HandlerRegistry)
    NS["_registry"] = NS["HandlerRegistry"]()
    NS["method"] = NS["_registry"].method


STATE_CACHE_FUNCS = {"_dir_exists_cached", "_build_project_tree"}
STATE_REG_FUNCS = {"_projects_method", "_register_project_mutator"}


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    state_in = copy.deepcopy(inp.get("state") or {})
    install_stubs(ctx, args)
    if func in STATE_CACHE_FUNCS:
        NS["_DIR_EXISTS_CACHE"] = dict(state_in.get("_DIR_EXISTS_CACHE") or {})
    if func in STATE_REG_FUNCS:
        NS["_registry"]._pending = [
            (pair[0], None) for pair in (state_in.get("_registry_pending") or [])]
    rid = args.get("rid")
    params = args.get("params")
    conn = args.get("conn")
    try:
        if func == "_pick":
            ret = NS[func](params, *(args.get("keys") or []))
        elif func == "_project_tree_row":
            ret = NS[func](entry_build(args.get("entry")))
        elif func == "_non_workspace_dirs":
            ret = NS[func]()
        elif func == "_is_repo_junk":
            ret = NS[func](args.get("root"))
        elif func == "_is_session_cwd_junk":
            ret = NS[func](args.get("cwd"))
        elif func == "_repo_discovery_policy":
            ret = NS[func](args.get("raw"))
        elif func == "_repo_discovery_policy_key":
            ret = NS[func](args.get("policy"))
        elif func == "_repo_discovery_policy_is_default":
            ret = NS[func](args.get("policy"))
        elif func == "_dir_exists_cached":
            ret = NS[func](args.get("path"))
        elif func == "_require_project":
            ret = NS[func](PDB_MOD, conn, params)
        elif func == "_projects_payload":
            ret = NS[func](conn)
        elif func == "_projects_method":
            decorator = NS[func](args.get("name"))
            fn = rec(ctx, "projects_fn",
                     lambda r, p, pdb, c: {"rid": r, "params": p, "conn": c})
            handler = decorator(fn)
            ret = handler(rid, params)
        elif func == "_register_project_mutator":
            kwargs_of = rec(ctx, "kwargs_of", lambda p: {"params": p})
            NS[func](args.get("suffix"), args.get("fn_name"),
                     args.get("takes_path", False), kwargs_of)
            handler = NS["_registry"]._pending[-1][1]
            ret = handler(rid, params)
        elif func in HANDLERS.values():
            ret = NS[func](rid, params, PDB_MOD, conn)
        elif func == "_scan_discovered_repos_remote":
            ret = NS[func](conn, args.get("policy"))
        elif func == "_discover_repos_payload":
            persistence = persistence_build(args.get("persistence"), ctx)
            ret = NS[func](args.get("db"), conn=conn,
                           backfill=args.get("backfill", True),
                           include_cached=args.get("include_cached", True),
                           persistence=persistence)
        elif func == "_project_tree_inputs":
            persistence = persistence_build(args.get("persistence"), ctx)
            ret = NS[func](persistence, args.get("session_limit", 2000),
                           include_discovered=args.get("include_discovered", True))
        elif func == "_build_project_tree":
            persistence = persistence_build(args.get("persistence"), ctx)
            ret = NS[func](args.get("db"),
                           preview_limit=args.get("preview_limit", 3),
                           hydrate=args.get("hydrate", False),
                           session_limit=args.get("session_limit", 2000),
                           include_discovered=args.get("include_discovered", True),
                           persistence=persistence)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state_out = dict(state_in)
    if func in STATE_CACHE_FUNCS:
        state_out["_DIR_EXISTS_CACHE"] = NS["_DIR_EXISTS_CACHE"]
    if func in STATE_REG_FUNCS:
        state_out["_registry_pending"] = [[n, "handler"] for n, _f in NS["_registry"]._pending]
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    "ok", "err", "load_cfg", "completion_cwd", "persistence_for", "logger.debug",
    "git_probe.branch", "git_probe.warm_roots", "git_probe.common_repo_root",
    "os.path.expanduser", "os.path.realpath", "os.path.isdir", "os.walk",
    "get_hermes_home", "default_config", "connect_closing",
    "pdb.list_projects", "pdb.get_active_id", "pdb.get_project", "to_dict",
    "pdb.create_project", "pdb.set_active", "pdb.archive_project",
    "pdb.restore_project", "pdb.delete_project", "pdb.project_for_path",
    "pdb.mutator", "pdb.record_discovered_repos", "pdb.list_discovered_repos",
    "pdb.reconcile_discovered_repos_policy", "record_feature_used",
    "SessionQuery", "SessionWindow", "persistence.list_sessions",
    "persistence.distinct_session_cwds", "persistence.backfill_repo_roots",
    "project_tree.build_tree", "projects_fn", "kwargs_of",
]

# seams fired through custom stubs (not the rec() helper)
DIRECT_CALL_NAMES = {"os.walk", "default_config", "SessionQuery", "SessionWindow"}


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_projects_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_projects_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmp_call\(ctx,\s*"([^"]+)"', csrc))
    for a, b in re.findall(r'\?\s*"([^"]+)"\s*:\s*"([^"]+)"', csrc):
        c_seams.add(a)
        c_seams.add(b)
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_projects_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n\s*ctx,\s*"([^"]+)"', osrc))
    used |= DIRECT_CALL_NAMES
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_projects_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_projects_flow.jsonl"
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
            print(f"FATAL oracle_tui_methods_projects_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        bad_funcs = fix_funcs - set(KINDS)
        if bad_funcs:
            print(f"FATAL oracle_tui_methods_projects_flow: fixture funcs outside KINDS: {sorted(bad_funcs)}")
            sys.exit(2)
        absent = set(KINDS) - fix_funcs
        if absent:
            print(f"FATAL oracle_tui_methods_projects_flow: KINDS funcs without fixtures: {sorted(absent)}")
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
    print(f"ORACLE tui_methods_projects_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_projects_flow.jsonl"
    n = bad = 0
    counts = {}
    guarded = set(PORTED) | {"to_dict", "list_projects", "get_project", "get_active_id",
                             "create_project", "set_active", "archive_project",
                             "restore_project", "delete_project", "project_for_path",
                             "record_discovered_repos", "list_discovered_repos",
                             "reconcile_discovered_repos_policy", "connect_closing",
                             "distinct_session_cwds", "backfill_repo_roots",
                             "list_sessions", "build_tree", "warm_roots",
                             "common_repo_root", "branch", "get_hermes_home",
                             "record_feature_used", "expanduser", "realpath", "isdir",
                             "walk", "debug"}
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
            mm = re.search(r"has no attribute '([^']+)'", str(r.get("message", "")))
            if mm and mm.group(1) in guarded:
                bad += 1
                print(f"SELFTEST BINDING GAP {func} case {n}: {r['message']}")
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_pick", {"args": {"params": {"a": 1, "b": 2}, "keys": ["a", "c"]}})
    assert spot["ret"] == {"a": 1, "c": None}, spot
    spot = drive("_is_repo_junk", {"args": {"root": ""}})
    assert spot["ret"] is True, spot
    spot = drive("_require_project", {"args": {"conn": "c1", "params": {"id": "p1"}},
                                      "io": {"pdb.get_project": None}})
    assert spot["ret"] == {"__raise__": "_NoProject", "message": ""}, spot
    spot = drive("_repo_discovery_policy", {
        "args": {"raw": {"enabled": False, "roots": [" /a ", "", 5],
                         "exclude_paths": ["/x"]}},
        "io": {"default_config": {"desktop": {
            "repo_scan_enabled": True, "repo_scan_roots": ["/d"],
            "repo_scan_exclude_paths": []}}}})
    assert spot["ret"] == {"enabled": False, "roots": ["/a"], "exclude_paths": ["/x"]}, spot
    spot = drive("_repo_discovery_policy_key", {
        "args": {"policy": {"enabled": True, "roots": ["/a", "/a"], "exclude_paths": []}},
        "io": {"os.path.expanduser": {"__script__": True, "ret": "/home/u"}}})
    assert spot["ret"] == '{"enabled":true,"exclude_paths":[],"roots":["/a"]}', spot
    spot = drive("_project_tree_row", {"args": {"entry": {
        "session": {"session_id": "s1", "parent_session_id": None, "reset_from": None,
                    "branched_from": None, "title": "t", "started_at": 0, "ended_at": None,
                    "source": "cli", "archived": 0, "message_count": 0, "tool_call_count": 0,
                    "input_tokens": 0, "output_tokens": 0, "actual_cost_usd": None,
                    "estimated_cost_usd": None, "model": None, "cwd": None,
                    "git_branch": None, "git_repo_root": None},
        "lineage_root_id": "s1", "lineage_ids": [], "preview": "p", "last_active": 0}}})
    assert spot["ret"]["id"] == "s1" and spot["ret"]["_lineage_ids"] is None, spot
    assert spot["ret"]["is_active"] is False and spot["ret"]["archived"] is False, spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_projects_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()