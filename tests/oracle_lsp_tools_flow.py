#!/usr/bin/env python3
"""Oracle for lsp_tools_flow — the LSP tools surface FLOW (LIVE authority,
py3.14 quarry venv).

The task names "tools/lsp_tools.py"; the quarry has NO such module. The real
LSP tools surface is the LSP-diagnostics tier of the file tools plus its
unclaimed collaborators. Ports covered (20 UNCLAIMED callables, PoP credits
the DEFINING module):
  tools/file_operations_lint.py: _lsp_service, _lsp_call,
    _local_workspace_untrusted, _run_managed_node_linter, _format_lsp_block,
    _post_deferred, _queue_late_verdict
  agent/lsp/deferred.py: bind_owner, current_owner, post, take, restore,
    pop_notes, track_pending, settled, _owners, settle_for_agent,
    forward_to_agent
  agent/lsp/sync_diagnostics.py: supported, _from_tsserver

Claimed siblings (_lsp_local_only et al.) and foreign collaborators are SEAMED;
unclaimed ported siblings compose for real (_post_deferred -> _queue_late_verdict
-> _format_lsp_block / deferred.post / deferred.restore; settle_for_agent /
forward_to_agent -> pop_notes; restore -> post). In-function imports are SILENT
(stub modules pre-installed in sys.modules); only effectful calls fire seams.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the quarry
(no heavy import), module constants seeded (Assign AND AnnAssign), the _Note /
ExecuteResult / LintResult dataclasses exec'd, every effect replaced by a
recording stub returning the SAME scripted results the C harness feeds its
universal seam. Compares the ordered DECISION SEQUENCE (trace) + return value
(raise envelopes included) + the mutated deferred-queue state (ordered canon).
Scripted {"__raise__": cls} raises a REAL exception of that class; drive()
catches BaseException so KeyboardInterrupt/SystemExit pass through live
except-Exception clauses exactly as the C port propagates them. A MISSING seam
key fail-closes (RuntimeError) on both sides; a scripted null is a valid None.

Input: stdin JSON lines from t_port_lsp_tools_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...][, "state":...]}}
Summary: ORACLE lsp_tools_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import contextlib
import dataclasses
import json
import os
import subprocess as _real_subprocess
import sys
import types
from collections import OrderedDict
from contextvars import ContextVar
from pathlib import Path
from typing import Any, Iterator, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = r"C:\Users\eman5\hermes-agent"
SRC_LINT = os.path.join(QUARRY, "tools", "file_operations_lint.py")
SRC_DEF = os.path.join(QUARRY, "agent", "lsp", "deferred.py")
SRC_SYNC = os.path.join(QUARRY, "agent", "lsp", "sync_diagnostics.py")
SRC_COMMON = os.path.join(QUARRY, "tools", "file_operations_common.py")

# C harness func -> (namespace key, live python name)
PY = {
    "lsp_service": ("lint", "_lsp_service"),
    "lsp_call": ("lint", "_lsp_call"),
    "local_workspace_untrusted": ("lint", "_local_workspace_untrusted"),
    "run_managed_node_linter": ("lint", "_run_managed_node_linter"),
    "format_lsp_block": ("lint", "_format_lsp_block"),
    "post_deferred": ("lint", "_post_deferred"),
    "queue_late_verdict": ("lint", "_queue_late_verdict"),
    "bind_owner": ("def", "bind_owner"),
    "current_owner": ("def", "current_owner"),
    "deferred_post": ("def", "post"),
    "deferred_take": ("def", "take"),
    "deferred_restore": ("def", "restore"),
    "pop_notes": ("def", "pop_notes"),
    "track_pending": ("def", "track_pending"),
    "settled": ("def", "settled"),
    "owners": ("def", "_owners"),
    "settle_for_agent": ("def", "settle_for_agent"),
    "forward_to_agent": ("def", "forward_to_agent"),
    "sync_supported": ("sync", "supported"),
    "from_tsserver": ("sync", "_from_tsserver"),
}
LINT_METHODS = ["_lsp_service", "_lsp_call", "_local_workspace_untrusted",
                "_run_managed_node_linter"]
LINT_FUNCS = ["_format_lsp_block", "_post_deferred", "_queue_late_verdict"]
DEFERRED_FUNCS = ["bind_owner", "current_owner", "post", "take", "restore",
                  "pop_notes", "track_pending", "settled", "_owners",
                  "settle_for_agent", "forward_to_agent"]
SYNC_FUNCS = ["supported", "_from_tsserver"]
STATEFUL = {"post_deferred", "queue_late_verdict", "deferred_post",
            "deferred_take", "deferred_restore", "pop_notes", "track_pending",
            "settled", "settle_for_agent", "forward_to_agent"}

NS_LINT = {}
NS_DEF = {}
NS_SYNC = {}
NS_COMMON = {}
CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {},
       "registry": {}, "next_token": 0}


# ── universal seam ──

def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "FileExistsError": FileExistsError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "TimeoutError": TimeoutError, "BlockingIOError": BlockingIOError,
    "ProcessLookupError": ProcessLookupError, "ChildProcessError": ChildProcessError,
    "InterruptedError": InterruptedError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "AssertionError": AssertionError, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError, "SystemError": SystemError,
    "BufferError": BufferError, "EOFError": EOFError, "Exception": Exception,
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
    "SubprocessError": _real_subprocess.SubprocessError,
    "CalledProcessError": _real_subprocess.CalledProcessError,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ResourceWarning": ResourceWarning,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        if name == "TimeoutExpired":
            raise _real_subprocess.TimeoutExpired(r.get("cmd"), r.get("timeout", 30),
                                                  output=r.get("output"), stderr=r.get("stderr"))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


# ── doubles ──

class RecLogger:
    def debug(self, fmt, *args, exc_info=False):
        return seam("logger_debug", {"fmt": fmt, "args": list(args), "exc_info": exc_info})

    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


class OwnerVar:
    def set(self, value): return seam("owner_set", {"value": value})
    def reset(self, token): return seam("owner_reset", {"token": token})
    def get(self): return seam("owner_get", {})


class InertLock:
    def __enter__(self): return self
    def __exit__(self, *a): return False


class SettledDouble:
    def __enter__(self): return self
    def __exit__(self, *a): return False
    def wait_for(self, pred, timeout=None):
        return seam("settled_wait_for", {"timeout": timeout, "pred": pred()})
    def notify_all(self): return seam("settled_notify_all", {})


class SvcDouble:
    def __init__(self, spec):
        self.__lt_spec__ = spec
        for k in spec:
            setattr(self, k, self._mk(k))

    def _mk(self, k):
        def call(path):
            return seam("svc_call", {"method": k, "path": path})
        return call


class FutureDouble:
    def cancelled(self): return seam("future_cancelled", {})
    def result(self): return mat_result(seam("future_result", {}))


class Token:
    def __init__(self, tid): self.tid = tid


def token_factory():
    tid = CUR["next_token"]
    CUR["next_token"] = tid + 1
    tok = Token(tid)
    CUR["registry"][tid] = tok
    return tok


# ── materialization ──

def mat_self(inp):
    spec = dict(inp.get("self") or {})
    if isinstance(spec.get("env"), dict):
        spec["env"] = types.SimpleNamespace(**spec["env"])
    slf = types.SimpleNamespace(**spec)
    slf._lsp_local_only = lambda: seam("lsp_local_only", {})
    for name in LINT_METHODS:
        setattr(slf, name, types.MethodType(NS_LINT[name], slf))
    return slf


def mat_note(n):
    return NS_DEF["_Note"](n["text"], n.get("baseline_text"))


def mat_carry(arr):
    return tuple(mat_note(n) for n in (arr or []))


def mat_future(f):
    return FutureDouble() if isinstance(f, dict) else f


def mat_result(d):
    if not isinstance(d, dict):
        return d
    ns = types.SimpleNamespace(**d)
    if "carry" in d:
        c = d["carry"]
        ns.carry = None if c is None else tuple(mat_note(n) for n in c)
    return ns


def mat_token(t):
    if t is None:
        return None
    return CUR["registry"].get(t) or Token(t)


def mat_agent(a):
    return types.SimpleNamespace(**a) if isinstance(a, dict) else a


def mat_client(c):
    return types.SimpleNamespace(**c) if isinstance(c, dict) else c


def mat_svc(s):
    return SvcDouble(s) if isinstance(s, dict) else s


def mat_state(inp):
    st = inp.get("state") or {}
    queued = OrderedDict()
    for owner, notes in (st.get("queued") or {}).items():
        od = OrderedDict()
        for path, n in (notes or {}).items():
            od[path] = mat_note(n)
        queued[owner] = od
    pending = {}
    CUR["registry"] = {}
    for owner, tids in (st.get("pending") or {}).items():
        s = set()
        for tid in tids:
            tok = Token(tid)
            CUR["registry"][tid] = tok
            s.add(tok)
        pending[owner] = s
    NS_DEF["_QUEUED"] = queued
    NS_DEF["_PENDING"] = pending
    CUR["next_token"] = st.get("next_token", 0)
    NS_DEF["object"] = token_factory


def echo_state():
    queued = OrderedDict()
    for o, notes in NS_DEF["_QUEUED"].items():
        od = OrderedDict()
        for p, n in notes.items():
            od[p] = {"text": n.text, "baseline_text": n.baseline_text}
        queued[o] = od
    pend = {o: sorted(tok.tid for tok in s) for o, s in NS_DEF["_PENDING"].items()}
    return {"queued": queued, "pending": pend, "next_token": CUR["next_token"]}


# ── normalization ──

def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, Token):
        return o.tid
    if isinstance(o, SvcDouble):
        return norm(o.__lt_spec__)
    if dataclasses.is_dataclass(o) and not isinstance(o, type):
        return {f.name: norm(getattr(o, f.name)) for f in dataclasses.fields(o)}
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if callable(o):
        return {"__callable__": True}
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def canon_ord(o):
    return json.loads(json.dumps(_numnorm(o), ensure_ascii=False, default=str))


# ── extraction ──

def _const_nodes(tree):
    out = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            out.append(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            out.append(node)
    return out


def _exec_node(node, src, ns):
    exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec"), ns)


def _class_funcs(tree, clsname, wanted):
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == clsname:
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in wanted:
                    yield sub


def extract_all():
    import logging
    import tomllib
    from typing import Callable, Dict

    # ── common dataclasses (ExecuteResult, LintResult) ──
    nsc = {"__name__": "tools.file_operations_common_extracted",
           "dataclass": dataclasses.dataclass}
    ctree = ast.parse(open(SRC_COMMON, encoding="utf-8").read())
    for node in ctree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("ExecuteResult", "LintResult"):
            _exec_node(node, SRC_COMMON, nsc)
    for k in ("ExecuteResult", "LintResult"):
        if k not in nsc:
            print("FATAL missing common class: %s" % k)
            sys.exit(2)

    # ── lint tier ──
    nsl = {
        "__name__": "tools.file_operations_lint_extracted",
        "json": json, "os": os, "ast": ast, "tomllib": tomllib, "logging": logging,
        "Callable": Callable, "Dict": Dict, "Optional": Optional,
        "ExecuteResult": nsc["ExecuteResult"], "LintResult": nsc["LintResult"],
        "logger": RecLogger(),
    }
    ltree = ast.parse(open(SRC_LINT, encoding="utf-8").read())
    # claimed in-process linters (referenced by the LINTERS_INPROC constant)
    for nm in ("_lint_python_inproc", "_lint_json_inproc", "_lint_yaml_inproc",
               "_lint_toml_inproc", "_looks_like_linter_unusable"):
        nsl[nm] = lambda *a, _n=nm, **k: seam(_n, {"args": list(a)})
    for node in _const_nodes(ltree):
        try:
            _exec_node(node, SRC_LINT, nsl)
        except Exception:
            pass
    nsl["logger"] = RecLogger()  # re-set after the logger Assign
    for sub in _class_funcs(ltree, "LintMixin", set(LINT_METHODS)):
        _exec_node(sub, SRC_LINT, nsl)
    for node in ltree.body:
        if isinstance(node, ast.FunctionDef) and node.name in LINT_FUNCS:
            _exec_node(node, SRC_LINT, nsl)
    missing = (set(LINT_METHODS) | set(LINT_FUNCS)) - set(nsl)
    if missing:
        print("FATAL missing lint defs: %s" % sorted(missing))
        sys.exit(2)
    if "_MANAGED_NODE_LINTERS" not in nsl:
        print("FATAL missing constant: _MANAGED_NODE_LINTERS")
        sys.exit(2)

    # ── deferred queue ──
    nsd = {
        "__name__": "agent.lsp.deferred_extracted",
        "contextlib": contextlib, "threading": __import__("threading"),
        "OrderedDict": OrderedDict, "ContextVar": ContextVar,
        "dataclass": dataclasses.dataclass,
        "Any": Any, "Iterator": Iterator, "Optional": Optional,
        "object": token_factory,
    }
    dtree = ast.parse(open(SRC_DEF, encoding="utf-8").read())
    for node in dtree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_Note":
            _exec_node(node, SRC_DEF, nsd)
    if "_Note" not in nsd:
        print("FATAL missing _Note dataclass")
        sys.exit(2)
    for node in _const_nodes(dtree):
        try:
            _exec_node(node, SRC_DEF, nsd)
        except Exception:
            pass
    for node in dtree.body:
        if isinstance(node, ast.FunctionDef) and node.name in DEFERRED_FUNCS:
            _exec_node(node, SRC_DEF, nsd)
    missing = set(DEFERRED_FUNCS) - set(nsd)
    if missing:
        print("FATAL missing deferred defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("_MAX_OWNERS", "_MAX_PER_OWNER", "TURN_END_WAIT"):
        if k not in nsd:
            print("FATAL missing deferred constant: %s" % k)
            sys.exit(2)
    nsd["_OWNER"] = OwnerVar()
    nsd["_LOCK"] = InertLock()
    nsd["_SETTLED"] = SettledDouble()

    # ── sync diagnostics leaves ──
    nss = {
        "__name__": "agent.lsp.sync_diagnostics_extracted",
        "logging": logging, "Any": Any, "Optional": Optional, "logger": RecLogger(),
    }
    stree = ast.parse(open(SRC_SYNC, encoding="utf-8").read())
    for node in _const_nodes(stree):
        try:
            _exec_node(node, SRC_SYNC, nss)
        except Exception:
            pass
    nss["logger"] = RecLogger()
    for node in stree.body:
        if isinstance(node, ast.FunctionDef) and node.name in SYNC_FUNCS:
            _exec_node(node, SRC_SYNC, nss)
    missing = set(SYNC_FUNCS) - set(nss)
    if missing:
        print("FATAL missing sync defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("_TS_SEVERITY", "_TS_SERVERS", "_UNSUPPORTED"):
        if k not in nss:
            print("FATAL missing sync constant: %s" % k)
            sys.exit(2)

    global NS_LINT, NS_DEF, NS_SYNC, NS_COMMON
    NS_LINT, NS_DEF, NS_SYNC, NS_COMMON = nsl, nsd, nss, nsc
    return nsl, nsd, nss, nsc


# ── recording stubs ──

def stub_mod(full, **attrs):
    m = types.ModuleType(full)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[full] = m
    parts = full.split(".")
    if len(parts) > 1:
        parent = sys.modules.get(".".join(parts[:-1]))
        if parent is not None:
            setattr(parent, parts[-1], m)
    return m


def pkg(full):
    m = types.ModuleType(full)
    m.__path__ = []
    sys.modules[full] = m
    parts = full.split(".")
    if len(parts) > 1:
        parent = sys.modules.get(".".join(parts[:-1]))
        if parent is not None:
            setattr(parent, parts[-1], m)
    return m


def rec(name, argfn):
    def f(*a, **k):
        return seam(name, argfn(*a, **k))
    return f


def run_stub(cmd, **kw):
    r = seam("subprocess_run", {
        "cmd": cmd, "cwd": kw.get("cwd"), "env": kw.get("env"),
        "stdin": kw.get("stdin"), "stdout": kw.get("stdout"), "stderr": kw.get("stderr"),
        "text": kw.get("text"), "encoding": kw.get("encoding"), "errors": kw.get("errors"),
        "timeout": kw.get("timeout"), "creationflags": kw.get("creationflags"),
        "check": kw.get("check")})
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def get_service_stub():
    return mat_svc(seam("get_service", {}))


def install_stubs():
    pkg("agent")
    pkg("tools")
    pkg("tools.environments")
    pkg("hermes_cli")
    agent_lsp = stub_mod("agent.lsp", get_service=get_service_stub)
    # the ported deferred functions compose for real through `from agent.lsp
    # import deferred` — bind the LIVE extracted functions onto the stub module.
    deferred_mod = types.ModuleType("agent.lsp.deferred")
    for name in DEFERRED_FUNCS:
        setattr(deferred_mod, name, NS_DEF[name])
    sys.modules["agent.lsp.deferred"] = deferred_mod
    agent_lsp.deferred = deferred_mod

    stub_mod("agent.lsp.manager",
             parse_trusted_workspaces=rec("parse_trusted_workspaces", lambda arg: {"arg": arg}))
    stub_mod("agent.lsp.workspace",
             is_trusted_workspace=rec("is_trusted_workspace",
                                      lambda cwd, trusted, roots: {"cwd": cwd, "trusted": trusted, "roots": roots}),
             operator_workspace_roots=rec("operator_workspace_roots", lambda: {}))
    stub_mod("agent.lsp.reporter",
             report_for_file=rec("report_for_file",
                                 lambda path, diagnostics: {"path": path, "diagnostics": diagnostics}),
             truncate=rec("truncate", lambda text: {"text": text}))
    stub_mod("agent.plugin_live_notes",
             queue_plugin_notice=rec("queue_plugin_notice",
                                     lambda agent, key, text: {"agent": CUR["in"].get("agent"), "key": key, "text": text}))
    stub_mod("hermes_cli.config", load_config_readonly=rec("load_config_readonly", lambda: {}))
    stub_mod("hermes_cli._subprocess_compat", windows_hide_flags=rec("windows_hide_flags", lambda: {}))
    stub_mod("hermes_constants", with_hermes_node_path=rec("with_hermes_node_path", lambda env: {"env": env}))
    stub_mod("tools.environments.local",
             _IS_WINDOWS=False,
             _msys_to_windows_path=rec("msys_to_windows_path", lambda path: {"path": path}),
             hermes_subprocess_env=rec("hermes_subprocess_env", lambda: {}))
    stub_mod("shutil", which=rec("shutil_which", lambda tool, path=None: {"tool": tool, "path": path}))
    stub_mod("subprocess", run=run_stub, DEVNULL=-3, PIPE=-1, STDOUT=-2,
             TimeoutExpired=_real_subprocess.TimeoutExpired)


# ── drive ──

def drive(func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    if func == "lsp_service":
        return norm(NS_LINT["_lsp_service"](mat_self(inp)))
    if func == "lsp_call":
        return norm(NS_LINT["_lsp_call"](mat_self(inp), inp.get("method", ""),
                                         inp.get("path", ""), inp.get("default")))
    if func == "local_workspace_untrusted":
        return NS_LINT["_local_workspace_untrusted"](mat_self(inp))
    if func == "run_managed_node_linter":
        sys.modules["tools.environments.local"]._IS_WINDOWS = inp.get("is_windows", False)
        return norm(NS_LINT["_run_managed_node_linter"](mat_self(inp), inp.get("ext", ""), inp.get("path", "")))
    if func == "format_lsp_block":
        return NS_LINT["_format_lsp_block"](inp.get("path", ""), inp.get("diagnostics"), inp.get("header", ""))
    if func == "post_deferred":
        NS_LINT["_post_deferred"](inp.get("owner", ""), inp.get("abs_path", ""),
                                  mat_future(inp.get("future")), mat_carry(inp.get("carry")),
                                  mat_token(inp.get("token")))
        return None
    if func == "queue_late_verdict":
        NS_LINT["_queue_late_verdict"](inp.get("owner", ""), inp.get("abs_path", ""),
                                       mat_future(inp.get("future")), mat_carry(inp.get("carry")))
        return None
    if func == "bind_owner":
        with NS_DEF["bind_owner"](inp.get("owner")):
            body = inp.get("body") or {}
            if body.get("read"):
                NS_DEF["current_owner"]()
            if body.get("raise"):
                maybe_raise(body["raise"])
        return None
    if func == "current_owner":
        return NS_DEF["current_owner"]()
    if func == "deferred_post":
        NS_DEF["post"](inp.get("owner", ""), inp.get("path", ""), inp.get("text"), inp.get("baseline_text"))
        return None
    if func == "deferred_take":
        return norm(NS_DEF["take"](inp.get("owner"), inp.get("path", "")))
    if func == "deferred_restore":
        notes = inp.get("notes")
        arg = None if notes is None else tuple(mat_note(n) for n in notes)
        NS_DEF["restore"](inp.get("owner"), inp.get("path", ""), arg)
        return None
    if func == "pop_notes":
        return norm(NS_DEF["pop_notes"](*(inp.get("owners") or [])))
    if func == "track_pending":
        return norm(NS_DEF["track_pending"](inp.get("owner", "")))
    if func == "settled":
        NS_DEF["settled"](inp.get("owner", ""), mat_token(inp.get("token")))
        return None
    if func == "owners":
        return norm(NS_DEF["_owners"](mat_agent(inp.get("agent"))))
    if func == "settle_for_agent":
        return norm(NS_DEF["settle_for_agent"](mat_agent(inp.get("agent")), timeout=inp.get("timeout", 5.0)))
    if func == "forward_to_agent":
        NS_DEF["forward_to_agent"](mat_agent(inp.get("agent")))
        return None
    if func == "sync_supported":
        return NS_SYNC["supported"](mat_client(inp.get("client")))
    if func == "from_tsserver":
        return norm(NS_SYNC["_from_tsserver"](inp.get("item")))
    raise ValueError("unhandled func: %s" % func)


def reset_cur(inp):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}
    mat_state(inp)
    sys.modules["tools.environments.local"]._IS_WINDOWS = inp.get("is_windows", False)


SEAM_NAMES = [
    "lsp_local_only", "get_service", "svc_call", "load_config_readonly",
    "parse_trusted_workspaces", "operator_workspace_roots", "is_trusted_workspace",
    "with_hermes_node_path", "shutil_which", "msys_to_windows_path",
    "hermes_subprocess_env", "windows_hide_flags", "subprocess_run",
    "report_for_file", "truncate", "logger_debug", "future_cancelled",
    "future_result", "settled_notify_all", "settled_wait_for",
    "queue_plugin_notice", "owner_set", "owner_reset", "owner_get",
]


def self_test():
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    extract_all()
    install_stubs()
    fixture = _REPO / "tests" / "fixtures" / "lsp_tools_flow.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").splitlines() if l.strip()]
    fixture_funcs = {c["func"] for c in cases}
    if fixture_funcs != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(fixture_funcs - set(PY)), sorted(set(PY) - fixture_funcs)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("SELFTEST FATAL <5 cases for: %s" % thin)
        sys.exit(2)
    bad = 0
    failclosed = 0
    names = {v[1] for v in PY.values()} | set(PY) | set(SEAM_NAMES) | set(DEFERRED_FUNCS)
    for c in cases:
        inp = c["in"]
        reset_cur(inp)
        try:
            drive(c["func"], inp)
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec))
                bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec))
                bad += 1
        if any(isinstance(e.get("ret"), dict) and e["ret"].get("msg", "").startswith("unscripted seam:")
               for e in CUR["trace"]):
            failclosed += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors, %d cases hit fail-closed seams"
          % (len(cases), bad, failclosed))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test()
        return
    extract_all()
    install_stubs()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        reset_cur(inp)
        try:
            ret = drive(func, inp)
            want = {"ret": norm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        if func in STATEFUL:
            want["state"] = echo_state()
        got_core = {"ret": got.get("ret"), "trace": got.get("trace", [])}
        want_core = {"ret": want["ret"], "trace": want["trace"]}
        bad = canon(want_core) != canon(got_core)
        if func in STATEFUL:
            bad = bad or canon_ord(want.get("state")) != canon_ord(got.get("state"))
        if bad:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE lsp_tools_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE lsp_tools_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
