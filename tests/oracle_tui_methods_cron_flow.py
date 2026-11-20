#!/usr/bin/env python3
"""Oracle for the TUI cron-method FLOW port (src/gateway/tui_methods_cron_flow.c).

The task named tui_gateway/methods_cron.py, which DOES NOT EXIST in the quarry
(hermes-agent). The real defining module of the TUI cron method (the cron.manage
JSON-RPC handler) is tui_gateway/methods_tools.py, so every PoP credits that
module. This oracle drives the LIVE cron.manage body, its UNCLAIMED RPC-wrapper
machinery (_profile_scoped_rpc / _guarded / _rpc / _scoped_rpc / _mcp_rpc) and
the cron-adjacent MCP-server / rewind / learning / skills / captured-exec helpers
with SCRIPTED seams: every server global (_ok/_err/_sess/_profile_home/...),
deferred module import (_tools_mod), claimed sibling (_str_arg/_busy_error/
_mcp_plugin_write_error) and seamed leaf sibling is a recording stub fed the same
scripts the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace"), the final return ("ret")
and the module-global mirror ("state"; these flows keep it empty).

Live defs are AST-extracted from methods_tools.py (module Assign/AnnAssign
constants seeded, plain-Name targets only; UNDECORATED FunctionDefs exec'd with
the future-annotations flag into a registered sys.modules module -- py3.14). The
two decorated `def _` handlers (cron.manage / learning.frames) are extracted BARE
(strip decorators, slice from node.lineno) under synthetic names _h_cron_manage /
_h_learning_frames. The heavy quarry import chain is never imported.
is_truthy_value + TRUTHY_STRINGS run REAL (AST-extracted from utils/env.py); the
C port transcribes them. HandlerRegistry is AST-extracted from method_ctx.py so
the real `method(name)` registration runs (decoration-time, silent).

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script is
a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches BaseException
(SystemExit included). Summary: 'ORACLE tui_methods_cron_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site.
- tools_mod scripts return the module NAME string; it resolves to a
  SimpleNamespace of recording stubs (attribute access silent, call traced).
- Tuples project to lists; sets to sorted lists; integral floats to ints; Paths
  to POSIX strings. str()/strip()/lower()/isdigit() fold the ASCII corpus only.
- Seam args drop None-valued keys on both sides (absent-vs-null parity).
- json.loads runs REAL on the cronjob JSON-string result; fixture cronjob results
  are strict integral JSON (no int/float identity drift).
"""
import ast
import asyncio
import contextlib
import copy
import json
import os
import re
import subprocess
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
SRC = os.path.join(Q, "tui_gateway", "methods_tools.py")
ENV_SRC = os.path.join(Q, "utils", "env.py")
MCTX_SRC = os.path.join(Q, "tui_gateway", "method_ctx.py")

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


# -- custom exception classes (real bases so except-clause identity matches) --
ProfileUnavailableError = type("ProfileUnavailableError", (FileNotFoundError,), {})
SessionNotOwned = type("SessionNotOwned", (RuntimeError,), {})

EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "PermissionError": PermissionError,
    "OSError": OSError, "FileNotFoundError": FileNotFoundError, "IndexError": IndexError,
    "LookupError": LookupError, "TimeoutError": TimeoutError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "ProfileUnavailableError": ProfileUnavailableError, "SessionNotOwned": SessionNotOwned,
    "TimeoutExpired": subprocess.TimeoutExpired, "SubprocessError": subprocess.SubprocessError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if kind == "TimeoutExpired":
        # multi-arg ctor (cmd, timeout): bypass __init__ so the instance is a real
        # TimeoutExpired (isinstance matches the except site) without the arg error.
        e = subprocess.TimeoutExpired.__new__(subprocess.TimeoutExpired)
        BaseException.__init__(e, msg)
    else:
        e = cls(msg)
    # Round-trip the scripted spec verbatim: proj_exc must NOT call str(e) for a
    # scripted raise (TimeoutExpired.__str__ reads self.cmd, absent after the
    # __init__ bypass). The C port parks the spec unchanged.
    e.__tmc2_spec__ = {"__raise__": kind, "message": msg}
    return e


def proj_exc(exc):
    spec = getattr(exc, "__tmc2_spec__", None)
    if spec is not None:
        return dict(spec)
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- HandlerRegistry (real, from method_ctx.py; registration is silent) --
def extract_registry():
    src = open(MCTX_SRC, encoding="utf-8").read()
    ns = {"contextlib": contextlib, "types": types}
    for node in ast.parse(src).body:
        if isinstance(node, ast.ClassDef) and node.name == "HandlerRegistry":
            exec_node(src, node, ns, MCTX_SRC)
            return ns["HandlerRegistry"]
    print("FATAL oracle_tui_methods_cron_flow: HandlerRegistry not found")
    sys.exit(2)


# -- is_truthy_value + TRUTHY_STRINGS (real, from utils/env.py) --
def extract_truthy():
    src = open(ENV_SRC, encoding="utf-8").read()
    ns = {"os": os, "re": re}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "TRUTHY_STRINGS" for t in node.targets):
            exec_node(src, node, ns, ENV_SRC)
        if isinstance(node, ast.FunctionDef) and node.name == "is_truthy_value":
            exec_node(src, node, ns, ENV_SRC)
    if "is_truthy_value" not in ns:
        print("FATAL oracle_tui_methods_cron_flow: is_truthy_value not found")
        sys.exit(2)
    return ns["is_truthy_value"], ns.get("TRUTHY_STRINGS")


WANTED = [
    "_profile_scoped_rpc", "_guarded", "_rpc", "_scoped_rpc", "_mcp_rpc",
    "_mcp_server_rows", "_mcp_named_server", "_mcp_config_server_or_error",
    "_user_turn_indices", "_rewind_prelude", "_rewind_or_err",
    "_learning_mutation", "_skills_search", "_skills_browse",
    "_capture_run_kwargs", "_captured_exec",
    "_str_arg", "_tools_mod", "_busy_error", "_mcp_plugin_write_error",
]
HANDLERS = {"cron.manage": "_h_cron_manage", "learning.frames": "_h_learning_frames"}


def extract():
    mod = new_module("tmc2_oracle_methods_tools")
    ns = mod.__dict__
    ns.update({"contextlib": contextlib, "sys": sys, "os": os, "re": re,
               "json": json, "subprocess": subprocess, "Path": Path,
               "HandlerRegistry": extract_registry(),
               "ProfileUnavailableError": ProfileUnavailableError,
               "SessionNotOwned": SessionNotOwned})
    truthy, truthy_set = extract_truthy()
    ns["is_truthy_value"] = truthy
    ns["TRUTHY_STRINGS"] = truthy_set
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            if node.name in WANTED:
                exec_node(src, node, ns, SRC)
        if isinstance(node, ast.FunctionDef) and node.name == "_" and node.decorator_list:
            dsrc = ast.unparse(node.decorator_list[0])
            newname = None
            for marker, nm in HANDLERS.items():
                if marker in dsrc:
                    newname = nm
                    break
            if newname is None:
                continue
            text = "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])
            text = text.replace("def _(", f"def {newname}(", 1)
            exec(compile(text, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
    required = WANTED + list(HANDLERS.values()) + ["_stripped", "_nonempty", "_NAME", "method"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_cron_flow: methods_tools missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# -- canonical projection (compare parsed JSON, never raw strings) --
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            if hasattr(x, "__tmc2_module__"):
                return x.__tmc2_module__
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
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# -- per-case script backend (mirrors the C tmc2_call exactly) --
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


# -- module doubles (tools_mod resolves the scripted module name) --
MODULE_ATTRS = {
    "tools.cronjob_tools": {"cronjob": "cronjob"},
    "hermes_cli.mcp_config": {"_get_mcp_servers": "mcp_config._get_mcp_servers"},
    "tui_gateway.mcp_rpc_helpers": {
        "server_configs_with_sources": "mcp_rpc_helpers.server_configs_with_sources"},
    "session_rules.turns": {"user_originated_turn_view": "turns.user_originated_turn_view"},
    "agent.learning_graph": {"build_learning_graph": "learning_graph.build_learning_graph"},
    "agent.learning_graph_render": {"render_frames": "learning_graph_render.render_frames"},
    "tools.skills_hub_search": {
        "unified_search": "skills_hub_search.unified_search",
        "create_source_router": "skills_hub_search.create_source_router"},
    "tools.skills_hub_github": {"GitHubAuth": "skills_hub_github.GitHubAuth"},
    "hermes_cli.skills_hub": {"browse_skills": "skills_hub.browse_skills"},
    "hermes_cli._subprocess_compat": {"windows_hide_flags": "subprocess_compat.windows_hide_flags"},
}

CAPTURE_KEYS = ["capture_output", "text", "encoding", "errors", "timeout", "stdin", "creationflags"]


def subprocess_run_args(cmd, **k):
    return {"cmd": cmd, "cwd": k.get("cwd"), "shell": k.get("shell"), "env": k.get("env"),
            "kwargs": {kk: k[kk] for kk in CAPTURE_KEYS if kk in k}}


def skills_rows_build(raw):
    if not isinstance(raw, list):
        return raw
    out = []
    for d in raw:
        if isinstance(d, dict):
            out.append(types.SimpleNamespace(
                name=d.get("name"), describe=(lambda d=d: d.get("description"))))
        else:
            out.append(d)
    return out


class LearningMutationsDouble:
    def __init__(self, ctx, name):
        self.__tmc2_module__ = name
        self._ctx = ctx

    def __getattr__(self, name):
        ctx = self._ctx

        def f(*a):
            return ctx.call("learning_mutations.call", {"fn": name, "args": list(a)})
        return f


def module_double(name, ctx):
    if name == "agent.learning_mutations":
        return LearningMutationsDouble(ctx, name)
    spec = MODULE_ATTRS.get(name)
    if spec is None:
        return types.SimpleNamespace(__tmc2_module__=name)
    attrs = {}
    for attr, seam in spec.items():
        build = skills_rows_build if seam == "skills_hub_search.unified_search" else None
        attrs[attr] = rec(ctx, seam, ARGS_FN[seam], build)
    return types.SimpleNamespace(__tmc2_module__=name, **attrs)


ARGS_FN = {
    "cronjob": lambda **k: dict(k),
    "mcp_config._get_mcp_servers": lambda: {},
    "mcp_rpc_helpers.server_configs_with_sources": lambda config_servers: {"config_servers": config_servers},
    "turns.user_originated_turn_view": lambda m: {"m": m},
    "learning_graph.build_learning_graph": lambda: {},
    "learning_graph_render.render_frames": lambda graph, cols, rows, frames: {
        "graph": graph, "cols": cols, "rows": rows, "frames": frames},
    "skills_hub_search.unified_search": lambda query, router, source_filter, limit: {
        "query": query, "router": router, "source_filter": source_filter, "limit": limit},
    "skills_hub_search.create_source_router": lambda auth: {"auth": auth},
    "skills_hub_github.GitHubAuth": lambda: {},
    "skills_hub.browse_skills": lambda page, page_size: {"page": page, "page_size": page_size},
    "subprocess_compat.windows_hide_flags": lambda: {},
}


# -- real defs restored before every case (seam slots are per-case) --
LEAF_SIBLINGS = ("_mcp_server_rows", "_mcp_named_server", "_user_turn_indices",
                 "_capture_run_kwargs")
REAL = {nm: NS[nm] for nm in LEAF_SIBLINGS}


def build_required(raw):
    out = []
    for pair in (raw or []):
        key, pred = pair[0], pair[1]
        out.append((key, NS["_nonempty"] if pred == "nonempty" else NS["_stripped"]))
    return tuple(out)


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    state_in = copy.deepcopy(inp.get("state") or {})
    for nm, real in REAL.items():
        NS[nm] = real
    NS["_ok"] = rec(ctx, "ok", lambda rid, result: {"rid": rid, "result": result})
    NS["_err"] = rec(ctx, "err",
                     lambda rid, code, msg, data=None:
                     {"rid": rid, "code": code, "message": msg, "data": data})
    NS["_tools_mod"] = rec(ctx, "tools_mod", lambda module: {"module": module},
                           build=lambda raw: module_double(raw, ctx))
    NS["_str_arg"] = rec(ctx, "str_arg", lambda params, key: {"params": params, "key": key})
    NS["_sess"] = rec(ctx, "sess", lambda params, rid: {"params": params, "rid": rid})
    NS["_profile_home"] = rec(ctx, "profile_home", lambda profile: {"profile": profile})
    NS["_session_profile_runtime_scope"] = rec(
        ctx, "session_profile_runtime_scope", lambda session: {"session": session},
        build=lambda raw: contextlib.nullcontext())
    NS["_mcp_server_rows"] = rec(ctx, "mcp_server_rows", lambda: {})
    NS["_mcp_named_server"] = rec(ctx, "mcp_named_server", lambda rid, params: {"rid": rid, "params": params})
    NS["_mcp_plugin_write_error"] = rec(
        ctx, "mcp_plugin_write_error", lambda rid, name, plugins: {"rid": rid, "name": name, "plugins": plugins})
    NS["_busy_error"] = rec(ctx, "busy_error", lambda rid, session, cmd: {"rid": rid, "session": session, "cmd": cmd})
    NS["_user_turn_indices"] = rec(ctx, "user_turn_indices", lambda session: {"session": session})
    NS["_history_without_ephemeral_scaffolding"] = rec(
        ctx, "history_without_ephemeral_scaffolding", lambda history: {"history": history})
    NS["_rewind_active_session_history"] = rec(
        ctx, "rewind_active_session_history", lambda session, keep, **kw: {"session": session, "keep": keep, "kw": kw})
    NS["_session_not_owned_err"] = rec(ctx, "session_not_owned_err", lambda rid, exc: {"rid": rid, "exc": proj_exc(exc)})
    NS["_capture_run_kwargs"] = rec(ctx, "capture_run_kwargs", lambda timeout: {"timeout": timeout})
    NS["subprocess"] = types.SimpleNamespace(
        run=rec(ctx, "subprocess.run", subprocess_run_args),
        TimeoutExpired=subprocess.TimeoutExpired, DEVNULL=subprocess.DEVNULL)
    NS["os"] = types.SimpleNamespace(getcwd=rec(ctx, "os.getcwd", lambda: {}))
    if func in LEAF_SIBLINGS:
        NS[func] = REAL[func]
    rid = args.get("rid")
    params = args.get("params")
    try:
        if func in ("_profile_scoped_rpc", "_guarded", "_rpc", "_scoped_rpc", "_mcp_rpc"):
            body_rec = rec(ctx, "body",
                           lambda *a: {"rid": a[0], "params": a[1],
                                       "session": a[2] if len(a) > 2 else None})
            required = build_required(args.get("required"))
            if func == "_profile_scoped_rpc":
                kw = {}
                for k in ("catch_resolve", "prefix", "scoped", "live_session"):
                    if k in args:
                        kw[k] = args[k]
                if "required" in args:
                    kw["required"] = required
                deco = NS[func](args.get("fail_code", 5024), **kw)
            elif func == "_guarded":
                deco = NS[func](args.get("fail_code", 5024), args.get("prefix", ""),
                                live_session=args.get("live_session", False))
            elif func == "_rpc":
                deco = NS[func](args.get("name", "m"), args.get("fail_code", 5024),
                                args.get("prefix", ""), live_session=args.get("live_session", False))
            elif func == "_scoped_rpc":
                kw = {}
                for k in ("catch_resolve", "prefix", "scoped", "live_session"):
                    if k in args:
                        kw[k] = args[k]
                if "required" in args:
                    kw["required"] = required
                deco = NS[func](args.get("name", "m"), args.get("fail_code", 5024), **kw)
            else:  # _mcp_rpc
                req = required if "required" in args else NS["_NAME"]
                deco = NS[func](args.get("name", "m"), required=req)
            ret = deco(body_rec)(rid, params)
        elif func == "_h_cron_manage":
            ret = NS[func](rid, params)
        elif func == "_mcp_server_rows":
            ret = NS[func]()
        elif func in ("_mcp_named_server", "_mcp_config_server_or_error"):
            ret = NS[func](rid, params)
        elif func == "_user_turn_indices":
            ret = NS[func](args.get("session"))
        elif func == "_rewind_prelude":
            ret = NS[func](rid, args.get("session"), args.get("cmd", ""), args.get("empty_msg", ""))
        elif func == "_rewind_or_err":
            ret = NS[func](rid, args.get("session"), args.get("keep"),
                           args.get("value_err"), args.get("fail_prefix", ""), **(args.get("kw") or {}))
        elif func == "_learning_mutation":
            body = NS[func](args.get("fn_name", ""), tuple(args.get("arg_keys") or []))
            ret = body(rid, params)
        elif func == "_h_learning_frames":
            ret = NS[func](rid, params)
        elif func == "_skills_search":
            ret = NS[func](rid, params, args.get("query"))
        elif func == "_skills_browse":
            ret = NS[func](rid, params, args.get("query", ""))
        elif func == "_capture_run_kwargs":
            ret = NS[func](args.get("timeout"))
        elif func == "_captured_exec":
            ret = NS[func](rid, args.get("cmd"), args.get("timeout"),
                           on_result=rec(ctx, "on_result", lambda r: {"result": r}),
                           timeout_err=tuple(args.get("timeout_err") or [5008, "timeout"]),
                           fail_code=args.get("fail_code", 5008),
                           shell=args.get("shell", False), env=args.get("env"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state_out = dict(state_in)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# -- three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) --
SEAM_NAMES = [
    "ok", "err", "tools_mod", "str_arg", "sess", "profile_home",
    "session_profile_runtime_scope", "body", "cronjob",
    "mcp_server_rows", "mcp_named_server", "mcp_plugin_write_error",
    "mcp_config._get_mcp_servers", "mcp_rpc_helpers.server_configs_with_sources",
    "busy_error", "user_turn_indices", "history_without_ephemeral_scaffolding",
    "turns.user_originated_turn_view", "rewind_active_session_history",
    "session_not_owned_err", "learning_mutations.call",
    "learning_graph.build_learning_graph", "learning_graph_render.render_frames",
    "skills_hub_github.GitHubAuth", "skills_hub_search.create_source_router",
    "skills_hub_search.unified_search", "skills_hub.browse_skills",
    "os.getcwd", "capture_run_kwargs", "subprocess_compat.windows_hide_flags",
    "subprocess.run", "on_result",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_cron_flow.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_cron_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmc2_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_cron_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n?\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(v for v in ARGS_FN)
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_cron_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_cron_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_cron_flow: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE tui_methods_cron_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_cron_flow.jsonl"
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
    target_funcs = list(counts)
    thin = [f for f in target_funcs if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_cron_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
