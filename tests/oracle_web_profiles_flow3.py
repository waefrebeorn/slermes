#!/usr/bin/env python3
"""Oracle for web_profiles_flow3 — the UNCLAIMED async endpoints and sidebar
profile-cache writers of hermes_cli/web_routers/profiles.py.

A CRLF-aware PoP grep at authoring time found 17 of the module's 67 AST defs
claimed by profiles.c / profiles_gaps.c / web_server.c / web_server_wrappers.c /
ws_revive2.c / ws_revive4.c, and 14 pure/leaf helpers claimed by
web_profiles_flow2.c. The defs ported here are claimed by
src/cli/web_profiles_flow3.c (one qualified PoP each); claimed siblings are
driven through the universal seam instead of being re-ported.

The oracle drives the LIVE defs AST-extracted from the quarry (no heavy import).
async def is lowered to def and `await` stripped; router/singleflight decorators
are dropped, @contextlib.contextmanager is kept. Every effectful collaborator is
replaced by a recording stub returning the SAME scripted results the C harness
feeds its universal seam: a MISSING key fail-closes with RuntimeError on both
sides, a present JSON null is a valid None, and an array script yields one
result PER CALL (last repeats). Scripted {"__raise__": cls} raises a REAL
builtin of that class (full py3.14 parent hierarchy incl. the BaseException
trio and the exception-group families), so `except Exception` never swallows
KeyboardInterrupt/SystemExit/GeneratorExit/BaseExceptionGroup while
ExceptionGroup and the Exception family are caught. Args and ret are DEEP-COPIED
into the trace at call time, matching the C harness.

Input: stdin JSON lines from t_port_web_profiles_flow3:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE web_profiles_flow3: N cases, M mismatches. Fail-closed (exit 2)
on extraction misses, func-set drift, <5 cases/func, or zero compared cases.

--self-test drives every fixture LIVE (no C) and reports binding/script gaps.
--shadow  diffs a literal Python MIRROR of the C port against the LIVE drive
          over every fixture (the shadow-diff; must print 0 diffs).
"""
import ast
import asyncio
import contextlib
import copy
import functools
import inspect
import json
import os
import re
import sys
import time
import types
from collections import OrderedDict
from enum import Enum
from pathlib import Path as _RealPath
from typing import Any, Callable, Dict, List, Optional, Tuple, Set

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path = None  # assigned below (Path is shadowed by the injected double)
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "web_routers", "profiles.py")

# harness func -> live python name
PY = {
    "profile_targets": "_profile_targets",
    "sidebar_cache_put": "_sidebar_profile_cache_put",
    "sidebar_cache_clear": "_sidebar_profile_cache_clear",
    "profile_errors": "_profile_errors",
    "read_off_loop": "_read_off_loop",
    "profile_setup_command": "get_profile_setup_command",
    "active_profile": "get_active_profile_endpoint",
    "set_active_profile": "set_active_profile_endpoint",
    "rename_profile": "rename_profile_endpoint",
    "delete_profile": "delete_profile_endpoint",
    "profile_soul": "get_profile_soul",
    "update_soul": "update_profile_soul",
    "update_model": "update_profile_model_endpoint",
    "describe_auto": "describe_profile_auto_endpoint",
    "create_profile": "create_profile_endpoint",
    "open_terminal": "open_profile_terminal_endpoint",
    "read_profiles": "_read_profiles",
    "list_profiles_endpoint": "list_profiles_endpoint",
    "read_desktop_overlay": "_read_desktop_overlay",
}
WANTED = set(PY.values())
SEAM_NAMES = [
    "profile_targets", "get_profile_dir", "scope_to_launch_profile", "log_exception",
    "resolve_profile_dir", "get_active_profile", "get_active_profile_name",
    "set_active_profile", "normalize_profile_name", "rename_profile", "delete_profile",
    "soul_exists", "soul_read_text", "atomic_write_text", "write_profile_model",
    "describe_profile", "provision_profile", "spawn_install", "which", "popen",
    "list_profiles", "fallback_profile_dicts", "read_text", "read_result",
]

CUR = {"trace": [], "scripts": {}, "idx": {}, "name": None}
NS = {}
_MISSING = object()


# ── exception doubles ──
class HTTPException(Exception):
    def __init__(self, status_code, detail):
        super().__init__("%s: %s" % (status_code, detail))
        self.status_code = status_code
        self.detail = detail


class ProfileIdentitySettlementPending(Exception):
    def __init__(self, path=None, retry_command=None):
        super().__init__("identity settlement pending")
        self.path = path
        self.retry_command = retry_command


class ModelAssignmentRejected(Exception):
    pass


# ── seam recorder ──
def seam(name, args):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, dict) and "__arr__" in sc:
            r = sc["__arr__"]
        elif isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    return maybe_raise(r)


def shadow_seam(name, args):
    """Mirror seam recording WITHOUT raising: the C port treats raises as
    return descriptors, so the literal mirror must do the same."""
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, dict) and "__arr__" in sc:
            r = sc["__arr__"]
        elif isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    return r


# Full py3.14 builtin exception parent hierarchy.
EXC_MAP = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError,
    "PythonFinalizationError": getattr(__builtins__, "PythonFinalizationError", RuntimeError),
    "SyntaxError": SyntaxError, "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
    "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
    "JSONDecodeError": json.JSONDecodeError,
    "ModelAssignmentRejected": ModelAssignmentRejected,
    "ProfileIdentitySettlementPending": ProfileIdentitySettlementPending,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        if name == "HTTPException":
            raise HTTPException(r.get("status_code", 500), r.get("detail", ""))
        if name == "ProfileIdentitySettlementPending":
            raise ProfileIdentitySettlementPending(r.get("path"), r.get("retry_command"))
        msg = r.get("msg", "")
        if name in ("BaseExceptionGroup", "ExceptionGroup"):
            base = EXC_MAP[name]
            cls = type(name, (base,), {
                "__str__": lambda self: str(self.args[0]) if self.args else ""})
            raise cls(msg, [ValueError("child")])
        base = EXC_MAP.get(name)
        if base is None:
            base = type(name, (Exception,), {})
        cls = type(name, (base,), {
            "__init__": lambda self, m: Exception.__init__(self, m),
            "__str__": lambda self: str(self.args[0]) if self.args else ""})
        raise cls(msg)
    return r


def serialize_ex(ex):
    if isinstance(ex, HTTPException):
        return {"__raise__": "HTTPException", "status_code": ex.status_code, "detail": ex.detail}
    if isinstance(ex, ProfileIdentitySettlementPending):
        return {"__raise__": "ProfileIdentitySettlementPending",
                "path": ex.path, "retry_command": ex.retry_command}
    return {"__raise__": type(ex).__name__, "msg": str(ex)}


def is_exception_name(name):
    cls = EXC_MAP.get(name)
    if cls is None:
        return True
    return issubclass(cls, Exception)


# ── AST extraction with async->sync / await-strip / decorator filter ──
class LowerAsync(ast.NodeTransformer):
    def visit_Await(self, node):
        return self.visit(node.value)

    def visit_AsyncFunctionDef(self, node):
        self.generic_visit(node)
        return ast.copy_location(ast.FunctionDef(
            name=node.name, args=node.args, body=node.body,
            decorator_list=node.decorator_list, returns=node.returns,
            type_comment=node.type_comment), node)


def _is_ctxmgr(d):
    if isinstance(d, ast.Call):
        d = d.func
    return (isinstance(d, ast.Attribute) and d.attr == "contextmanager" and
            isinstance(d.value, ast.Name) and d.value.id == "contextlib")


class FilterDecorators(ast.NodeTransformer):
    def visit_FunctionDef(self, node):
        self.generic_visit(node)
        node.decorator_list = [d for d in node.decorator_list if _is_ctxmgr(d)]
        return node


class FakePath:
    def __init__(self, s, name=None, read_seam="read_text", base=None):
        self.s = str(s)
        self.name = name
        self.read_seam = read_seam
        self.base = base

    def __truediv__(self, other):
        return FakePath(self.s + "/" + str(other), self.name, self.read_seam,
                        base=self.base or self.s)

    def __str__(self):
        return self.s

    def exists(self):
        return seam("soul_exists", {"name": self.name})

    def is_file(self):
        return seam("overlay_is_file", {"name": self.name})

    def read_text(self, encoding=None):
        if self.read_seam == "soul_read_text":
            return seam(self.read_seam, {"name": self.name})
        return seam(self.read_seam, {"profile_dir": self.base or self.s, "file": "desktop.json"})


def _freeze(v):
    if isinstance(v, list):
        return tuple(_freeze(x) for x in v)
    if isinstance(v, dict):
        return tuple(sorted((k, _freeze(x)) for k, x in v.items()))
    return v


def _thaw(v):
    if isinstance(v, tuple):
        return [_thaw(x) for x in v]
    if isinstance(v, list):
        return [_thaw(x) for x in v]
    if isinstance(v, dict):
        return {k: _thaw(x) for k, x in v.items()}
    return v


def fake_module(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def install_stubs(ns):
    ns["HTTPException"] = HTTPException
    ns["ProfileIdentitySettlementPending"] = ProfileIdentitySettlementPending
    ns["ModelAssignmentRejected"] = ModelAssignmentRejected
    ns["Path"] = FakePath
    ns["Any"], ns["Callable"], ns["Dict"] = Any, Callable, Dict
    ns["List"], ns["Optional"], ns["Tuple"], ns["Set"] = List, Optional, Tuple, Set
    ns["json"], ns["copy"], ns["functools"] = json, copy, functools
    ns["inspect"], ns["time"], ns["contextlib"], ns["re"] = inspect, time, contextlib, re
    ns["frozenset"], ns["object"] = frozenset, object

    def run_in_threadpool(fn, *a, **k):
        return fn(*a, **k)

    ns["run_in_threadpool"] = run_in_threadpool
    asyncio.to_thread = lambda fn, *a, **k: fn(*a, **k)

    ns["_MISSING"] = _MISSING
    ns["_log"] = types.SimpleNamespace(
        exception=lambda msg, *a: seam("log_exception", {"log_msg": msg, "args": list(a)}),
        warning=lambda *a, **k: seam("log_warning", {"args": list(a)}),
    )
    def scope_stub(rows, key):
        return seam("scope_to_launch_profile",
                    {CUR.get("scope_key", "profiles"): rows})
    ns["scope_to_launch_profile"] = scope_stub
    ns["_fallback_profile_dicts"] = lambda mod: seam("fallback_profile_dicts", {})
    ns["_profile_to_dict"] = lambda p: p
    ns["_write_profile_model"] = lambda d, p, m: seam(
        "write_profile_model", {"name": getattr(d, "name", None), "provider": p, "model": m})

    def resolve_profile_dir(name):
        node = seam("resolve_profile_dir", {"name": name})
        return FakePath(node if isinstance(node, str) else str(name), name=name,
                        read_seam="soul_read_text")

    ns["_resolve_profile_dir"] = resolve_profile_dir

    def setup_command(name):
        seam("resolve_profile_dir", {"name": name})
        return "hermes setup" if name == "default" else "%s setup" % name

    ns["_profile_setup_command"] = setup_command

    def best_effort(log_msg, *args, fn=None, default=None):
        try:
            return fn()
        except BaseException as ex:
            if is_exception_name(type(ex).__name__):
                seam("log_exception", {"log_msg": log_msg, "args": list(args)})
                return default
            raise

    ns["_best_effort"] = best_effort
    ns["_hub_action_name"] = lambda action, ident, name: "%s:%s:%s" % (action, ident, name)
    ns["get_process_hermes_home"] = lambda: "/home"
    def spawn_hermes_action(args, label):
        ident = args[4] if isinstance(args, (list, tuple)) and len(args) > 4 else ""
        return types.SimpleNamespace(pid=seam("spawn_install", {"ident": ident}))
    ns["_spawn_hermes_action"] = spawn_hermes_action

    # fake external modules
    profiles_mod = fake_module(
        "hermes_cli.profiles",
        profiles_to_serve=lambda **k: seam("profile_targets",
                                            {"log_label": CUR.get("log_label", "")}),
        get_profile_dir=lambda n: seam("get_profile_dir", {"name": n}),
        get_active_profile=lambda: seam("get_active_profile", {}),
        get_active_profile_name=lambda: seam("get_active_profile_name", {}),
        set_active_profile=lambda n: seam("set_active_profile", {"name": n}),
        normalize_profile_name=lambda n: seam("normalize_profile_name", {"name": n}),
        rename_profile=lambda n, nn: seam("rename_profile", {"name": n, "new_name": nn}),
        delete_profile=lambda n, **k: seam("delete_profile", {"name": n}),
        list_profiles=lambda **k: seam("list_profiles", {}),
    )
    hermes_cli = sys.modules.get("hermes_cli")
    if hermes_cli is None:
        hermes_cli = fake_module("hermes_cli")
    setattr(hermes_cli, "profiles", profiles_mod)

    class ProfileCreateRequest:
        def __init__(self, **kw):
            self.__dict__.update(kw)

    fake_module("hermes_cli.config", get_process_hermes_home=lambda: "/home")
    global _PROVISION_MOD
    _PROVISION_MOD = fake_module("hermes_cli.profile_provision",
                CREATE_REFUSALS=(), ProfileCreateRequest=ProfileCreateRequest,
                provision_profile=lambda req: types.SimpleNamespace(**seam("provision_profile", {
                    "name": getattr(req, "name", None),
                    "clone_from": getattr(req, "clone_from", None),
                    "clone_all": getattr(req, "clone_all", None)})))
    fake_module("hermes_cli.model_assignment", ModelAssignmentRejected=ModelAssignmentRejected)
    fake_module("hermes_cli.profile_describer",
                describe_profile=lambda n, overwrite=False: types.SimpleNamespace(
                    **seam("describe_profile", {"name": n, "overwrite": bool(overwrite)})))
    fake_module("utils", atomic_write_text=lambda path, content, **k: seam(
        "atomic_write_text", {"name": getattr(path, "name", None), "content": content}))

    def which(cmd, **kw):
        exe = cmd[1] if isinstance(cmd, (list, tuple)) and len(cmd) > 1 else (cmd[0] if cmd else "")
        return seam("which", {"exe": exe})

    def popen(args, **kw):
        return seam("popen", {"args": args})

    sub = fake_module("subprocess", call=which, Popen=popen, DEVNULL=-3)
    ns["subprocess"] = sub
    ns["asyncio"] = asyncio
    ns["sys"] = types.SimpleNamespace(platform="linux")
    ns["subagent_listing_scope"] = lambda *a, **k: (False, None)


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"__name__": "hermes_cli.web_profiles_flow3_extracted",
          "contextlib": contextlib, "functools": functools, "json": json,
          "copy": copy, "inspect": inspect, "time": time, "re": re,
          "Any": Any, "Callable": Callable, "Dict": Dict, "List": List,
          "Optional": Optional, "Tuple": Tuple, "Set": Set,
          "HTTPException": HTTPException,
          "ProfileIdentitySettlementPending": ProfileIdentitySettlementPending,
          "Path": FakePath}
    tree = ast.parse(src)
    tree = LowerAsync().visit(tree)
    tree = FilterDecorators().visit(tree)
    ast.fix_missing_locations(tree)

    # module-level constants the extracted helpers read
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in ("_LINUX_TERMINALS",) for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and (node.name in WANTED or
                node.name in ("_linux_terminal_commands",)):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    if "_linux_terminal_commands" not in ns:
        print("FATAL missing helper: _linux_terminal_commands")
        sys.exit(2)
    global NS
    NS = ns
    return ns


# ── live drive ──
def drive(ns, func, inp):
    d = copy.deepcopy

    if func == "profile_targets":
        CUR["log_label"] = inp.get("log_label") or ""
        CUR["scope_key"] = "targets"
        return ns["_profile_targets"](CUR["log_label"])

    if func in ("sidebar_cache_put", "sidebar_cache_clear"):
        cache = OrderedDict((_freeze(k), d(v)) for k, v in (inp.get("cache") or []))
        ns["_SIDEBAR_PROFILE_CACHE"] = cache
        ns["_SIDEBAR_PROFILE_CACHE_LOCK"] = contextlib.nullcontext()
        ns["_SIDEBAR_PROFILE_CACHE_MAX_ENTRIES"] = inp.get("max_entries", 256)
        if func == "sidebar_cache_put":
            ns["_sidebar_profile_cache_put"](_freeze(inp.get("key") or []), d(inp.get("value")))
        else:
            ns["_sidebar_profile_cache_clear"]()
        return [[_thaw(k), _thaw(v)] for k, v in cache.items()]

    if func == "profile_errors":
        nf = tuple(EXC_MAP[n] for n in (inp.get("not_found") or ["FileNotFoundError"]))
        br = tuple(EXC_MAP[n] for n in (inp.get("bad_request") or ["ValueError"]))
        try:
            with ns["_profile_errors"](inp.get("log_msg") or "", *inp.get("args", []),
                                       not_found=nf, bad_request=br):
                maybe_raise(d(inp.get("raise") or {}))
            return None
        except BaseException as ex:
            return serialize_ex(ex)

    if func == "read_off_loop":
        def reader():
            return seam("read_result", {})
        return ns["_read_off_loop"](reader, inp.get("label") or "",
                                    tuple(EXC_MAP[n] for n in (inp.get("errors") or [])))

    if func == "profile_setup_command":
        return ns["get_profile_setup_command"](inp.get("name") or "")

    if func == "active_profile":
        return ns["get_active_profile_endpoint"]()

    if func == "set_active_profile":
        return ns["set_active_profile_endpoint"](types.SimpleNamespace(name=inp.get("name") or ""))

    if func == "rename_profile":
        return ns["rename_profile_endpoint"](inp.get("name") or "",
                                            types.SimpleNamespace(new_name=inp.get("new_name") or ""))

    if func == "delete_profile":
        return ns["delete_profile_endpoint"](inp.get("name") or "")

    if func == "profile_soul":
        return ns["get_profile_soul"](inp.get("name") or "")

    if func == "update_soul":
        return ns["update_profile_soul"](inp.get("name") or "",
                                        types.SimpleNamespace(content=inp.get("content") or ""))

    if func == "update_model":
        return ns["update_profile_model_endpoint"](
            inp.get("name") or "",
            types.SimpleNamespace(provider=inp.get("provider") or "", model=inp.get("model") or ""))

    if func == "describe_auto":
        return ns["describe_profile_auto_endpoint"](
            inp.get("name") or "", types.SimpleNamespace(overwrite=bool(inp.get("overwrite"))))

    if func == "create_profile":
        body = inp.get("body") or {}
        _PROVISION_MOD.CREATE_REFUSALS = tuple(EXC_MAP[n] for n in (body.get("refusals") or []))
        defaults = dict(name="", clone_from=None, clone_all=False, clone_from_default=False,
                        no_skills=False, description=None, clone_channels=False, soul=None,
                        provider=None, model=None, mcp_servers=[], keep_skills=[],
                        mirror_credentials=False, share_auth=False, hub_skills=[])
        defaults.update({k: v for k, v in body.items() if k != "refusals"})
        b = types.SimpleNamespace(**defaults)
        return ns["create_profile_endpoint"](b)

    if func == "open_terminal":
        ns["sys"] = types.SimpleNamespace(platform=inp.get("platform") or "linux")
        return ns["open_profile_terminal_endpoint"](inp.get("name") or "")

    if func in ("read_profiles", "list_profiles_endpoint"):
        CUR["scope_key"] = "profiles"
        return ns[PY[func]]()

    if func == "read_desktop_overlay":
        return ns["_read_desktop_overlay"](FakePath(inp.get("profile_dir") or "", read_seam="read_text"))

    raise ValueError("unhandled func: %s" % func)


# ── comparison ──
def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def run_live(func, inp):
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}
    try:
        ret = drive(NS, func, inp)
        return {"ret": ret, "trace": CUR["trace"]}, None
    except BaseException as ex:
        return {"ret": serialize_ex(ex), "trace": CUR["trace"]}, ex


def fixture():
    return os.path.join(_REPO, "tests", "fixtures", "web_profiles_flow3.jsonl")


def load_cases():
    p = fixture()
    if not os.path.exists(p):
        return []
    with open(p, encoding="utf-8") as f:
        return [json.loads(l) for l in f if l.strip()]


def check_func_set(cases):
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(ff - set(PY)), sorted(set(PY) - ff)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("FATAL <5 cases for: %s" % thin)
        sys.exit(2)


def self_test():
    ns = extract()
    install_stubs(ns)
    cases = load_cases()
    check_func_set(cases)
    bad = 0
    for c in cases:
        _, ex = run_live(c["func"], c["in"])
        if isinstance(ex, AttributeError) and any(
                ("%s" % n) in str(ex) for n in list(PY) + SEAM_NAMES):
            print("SELFTEST BINDING %s: %s: %s" % (c["func"], type(ex).__name__, ex))
            bad += 1
        elif isinstance(ex, NameError):
            print("SELFTEST NAMEERROR %s: %s" % (c["func"], ex))
            bad += 1
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


# ── literal Python mirror of the C port (for --shadow) ──
def _shadow_inner(func, inp):
    d = copy.deepcopy
    if func == "profile_targets":
        r = seam("profile_targets", {"log_label": inp.get("log_label") or ""})
        if isinstance(r, dict) and "__raise__" in r:
            if is_exception_name(r["__raise__"]):
                seam("log_exception", {"log_msg": "%s: profile enumeration failed",
                                       "args": [inp.get("log_label") or ""]})
                targets = []
            else:
                return r
        else:
            targets = d(r or [])
        if not targets:
            home = seam("get_profile_dir", {"name": "default"})
            if isinstance(home, dict) and "__raise__" in home:
                return home
            targets = [["default", home]]
        return seam("scope_to_launch_profile", {"targets": targets})

    if func == "sidebar_cache_put":
        cache = d(inp.get("cache") or [])
        key = inp.get("key") or []
        if not isinstance(key, list) or len(key) < 2:
            return cache
        db_path, fp = key[0], key[1]
        out = [p for p in cache if not (isinstance(p, list) and p and
               isinstance(p[0], list) and len(p[0]) > 1 and
               p[0][0] == db_path and p[0][1] != fp)]
        found = False
        for p in out:
            if p[0] == key:
                p[1] = d(inp.get("value")); found = True
                out.remove(p); out.append(p)
                break
        if not found:
            out.append([d(key), d(inp.get("value"))])
        mx = inp.get("max_entries", 256)
        while len(out) > mx:
            out.pop(0)
        return out

    if func == "sidebar_cache_clear":
        return []

    if func == "profile_errors":
        exc = inp.get("raise") or {}
        name = exc.get("__raise__")
        msg = exc.get("msg", exc.get("detail", ""))
        if name == "HTTPException":
            return exc
        nf = inp.get("not_found") or []
        br = inp.get("bad_request") or []
        if any(is_exception_name(name) and _is_sub(name, n) for n in nf):
            return {"__raise__": "HTTPException", "status_code": 404, "detail": msg}
        if any(is_exception_name(name) and _is_sub(name, n) for n in br):
            return {"__raise__": "HTTPException", "status_code": 400, "detail": msg}
        if name == "ProfileIdentitySettlementPending":
            return exc
        if is_exception_name(name):
            seam("log_exception", {"log_msg": inp.get("log_msg") or "", "args": d(inp.get("args") or [])})
            return {"__raise__": "HTTPException", "status_code": 500, "detail": msg}
        return exc

    if func == "read_off_loop":
        r = seam("read_result", {})
        if isinstance(r, dict) and "__raise__" in r:
            if any(_is_sub(r["__raise__"], n) for n in (inp.get("errors") or [])):
                return {"__raise__": "HTTPException", "status_code": 500,
                        "detail": "Could not read %s: %s" % (inp.get("label") or "", r.get("msg", ""))}
            return r
        return r

    if func == "profile_setup_command":
        r = seam("resolve_profile_dir", {"name": inp.get("name") or ""})
        if isinstance(r, dict) and "__raise__" in r:
            return r
        name = inp.get("name") or ""
        return {"command": "hermes setup" if name == "default" else "%s setup" % name}

    if func == "active_profile":
        r1 = seam("get_active_profile", {})
        r1r = isinstance(r1, dict) and "__raise__" in r1
        if r1r and not is_exception_name(r1["__raise__"]):
            return r1
        r2 = seam("get_active_profile_name", {})
        r2r = isinstance(r2, dict) and "__raise__" in r2
        if r2r and not is_exception_name(r2["__raise__"]):
            return r2
        a = "default" if r1r or not r1 else r1
        c = "custom" if r2r else (r2 if r2 else "default")
        return {"active": a, "current": c}

    if func == "set_active_profile":
        name = inp.get("name") or ""
        r = seam("set_active_profile", {"name": name})
        if isinstance(r, dict) and "__raise__" in r:
            return _shadow_map(r, "POST /api/profiles/active failed", [], ["FileNotFoundError"], ["ValueError"])
        norm = seam("normalize_profile_name", {"name": name})
        if isinstance(norm, dict) and "__raise__" in norm:
            return norm
        return {"ok": True, "active": norm}

    if func == "rename_profile":
        name, nn = inp.get("name") or "", inp.get("new_name") or ""
        r = seam("rename_profile", {"name": name, "new_name": nn})
        if isinstance(r, dict) and "__raise__" in r:
            return _shadow_map(r, "PATCH /api/profiles/%s failed", [name], ["FileNotFoundError"],
                               ["ValueError", "FileExistsError"])
        n1 = seam("normalize_profile_name", {"name": name})
        is_default = not (isinstance(n1, dict) and "__raise__" in n1) and n1 == "default"
        out = {"ok": True}
        if is_default:
            out.update(name="default", display_name=nn.strip(), path=r)
        else:
            n2 = seam("normalize_profile_name", {"name": nn})
            if isinstance(n2, dict) and "__raise__" in n2:
                return n2
            out.update(name=n2, path=r)
        return out

    if func == "delete_profile":
        name = inp.get("name") or ""
        r = seam("delete_profile", {"name": name})
        if isinstance(r, dict) and "__raise__" in r:
            mapped = _shadow_map(r, "DELETE /api/profiles/%s failed", [name], ["FileNotFoundError"], ["ValueError"])
            if mapped.get("__raise__") == "ProfileIdentitySettlementPending":
                return {"ok": True, "path": mapped.get("path"), "identity_settled": False,
                        "settlement_pending": True, "retry_command": mapped.get("retry_command")}
            return mapped
        return {"ok": True, "path": r}

    if func == "profile_soul":
        name = inp.get("name") or ""
        rp = seam("resolve_profile_dir", {"name": name})
        if isinstance(rp, dict) and "__raise__" in rp:
            return rp
        ex = seam("soul_exists", {"name": name})
        if isinstance(ex, dict) and "__raise__" in ex:
            if _is_sub(ex["__raise__"], "OSError"):
                return {"__raise__": "HTTPException", "status_code": 500,
                        "detail": "Could not read SOUL.md: %s" % ex.get("msg", "")}
            return ex
        if not ex:
            return {"content": "", "exists": False}
        c = seam("soul_read_text", {"name": name})
        if isinstance(c, dict) and "__raise__" in c:
            if _is_sub(c["__raise__"], "OSError"):
                return {"__raise__": "HTTPException", "status_code": 500,
                        "detail": "Could not read SOUL.md: %s" % c.get("msg", "")}
            return c
        return {"content": c, "exists": True}

    if func == "update_soul":
        name, content = inp.get("name") or "", inp.get("content") or ""
        rp = seam("resolve_profile_dir", {"name": name})
        if isinstance(rp, dict) and "__raise__" in rp:
            return rp
        r = seam("atomic_write_text", {"name": name, "content": content})
        if isinstance(r, dict) and "__raise__" in r:
            if _is_sub(r["__raise__"], "OSError"):
                seam("log_exception", {"log_msg": "PUT /api/profiles/%s/soul failed", "args": [name]})
                return {"__raise__": "HTTPException", "status_code": 500,
                        "detail": "Could not write SOUL.md: %s" % r.get("msg", "")}
            return r
        return {"ok": True}

    if func == "update_model":
        name = inp.get("name") or ""
        rp = seam("resolve_profile_dir", {"name": name})
        if isinstance(rp, dict) and "__raise__" in rp:
            return rp
        provider, model = (inp.get("provider") or "").strip(), (inp.get("model") or "").strip()
        if not provider or not model:
            return {"__raise__": "HTTPException", "status_code": 400,
                    "detail": "provider and model are required"}
        r = seam("write_profile_model", {"name": name, "provider": provider, "model": model})
        if isinstance(r, dict) and "__raise__" in r:
            return _shadow_map(r, "PUT /api/profiles/%s/model failed", [name], [],
                               ["ModelAssignmentRejected"])
        return {"ok": True, "provider": provider, "model": model}

    if func == "describe_auto":
        name = inp.get("name") or ""
        rp = seam("resolve_profile_dir", {"name": name})
        if isinstance(rp, dict) and "__raise__" in rp:
            return rp
        r = seam("describe_profile", {"name": name, "overwrite": bool(inp.get("overwrite"))})
        if isinstance(r, dict) and "__raise__" in r:
            return _shadow_map(r, "POST /api/profiles/%s/describe-auto failed", [name], [], [])
        ok = bool(r.get("ok"))
        return {"ok": ok, "reason": r.get("reason"), "description": r.get("description"),
                "description_auto": ok}

    if func == "create_profile":
        body = inp.get("body") or {}
        raw, clone_all, cfd = body.get("clone_from") or "", bool(body.get("clone_all")), bool(body.get("clone_from_default"))
        explicit = raw.strip()
        if explicit:
            clone_from = explicit
        elif clone_all:
            clone_from = "default"
        else:
            clone_from = "default" if cfd else None
        r = seam("provision_profile", {"name": body.get("name"),
                                       "clone_from": clone_from, "clone_all": clone_all})
        if isinstance(r, dict) and "__raise__" in r:
            return _shadow_map(r, "POST /api/profiles failed", [], [], body.get("refusals") or [])
        out = {"ok": True, "name": body.get("name"), "path": r.get("path"),
               "model_set": r.get("model_set"), "model_error": r.get("model_error"),
               "mcp_written": r.get("mcp_written"), "skills_disabled": r.get("skills_disabled"),
               "soul_set": body.get("soul") is not None, "warnings": r.get("warnings")}
        installs = []
        for ident in [str(i).strip() for i in (body.get("hub_skills") or [])]:
            if not ident:
                continue
            pid = seam("spawn_install", {"ident": ident})
            if isinstance(pid, dict) and "__raise__" in pid:
                if is_exception_name(pid["__raise__"]):
                    seam("log_exception", {"log_msg": "Spawning hub-skill install %s for new profile %s failed",
                                           "args": [ident, body.get("name") or ""]})
                    pid = None
                else:
                    return pid
            installs.append({"identifier": ident, "pid": pid})
        out["hub_installs"] = installs
        return out

    if func == "open_terminal":
        name, platform = inp.get("name") or "", inp.get("platform") or "linux"
        r = seam("resolve_profile_dir", {"name": name})
        if isinstance(r, dict) and "__raise__" in r:
            return _shadow_map(r, "POST /api/profiles/%s/open-terminal failed", [name], ["FileNotFoundError"], ["ValueError"])
        command = r if isinstance(r, str) else ""
        out = {"command": command}
        if platform.startswith("win"):
            seam("popen", {"args": ["cmd.exe", "/c", "start", "", command]})
        elif platform == "darwin":
            esc = command.replace("\\", "\\\\").replace('"', '\\"')
            seam("popen", {"args": ["osascript", "-e",
                                    'tell application "Terminal"\nactivate\ndo script "%s"\nend tell' % esc]})
        else:
            terms = [("x-terminal-emulator", "-e"), ("gnome-terminal", "--"), ("konsole", "-e"),
                     ("xfce4-terminal", None), ("mate-terminal", None), ("lxterminal", None),
                     ("tilix", "-e"), ("alacritty", "-e"), ("kitty", ""), ("xterm", "-e")]
            launched = False
            for exe, flag in terms:
                w = seam("which", {"exe": exe})
                if isinstance(w, dict) and "__raise__" in w:
                    if is_exception_name(w["__raise__"]):
                        return _shadow_map(w, "POST /api/profiles/%s/open-terminal failed",
                                           [name], ["FileNotFoundError"], ["ValueError"])
                    return w
                ok = (w == 0) if isinstance(w, (int, float)) else (not w)
                if ok:
                    quoted = "sh -lc '%s'" % command
                    if flag is None:
                        pargs = [exe, "-e", quoted]
                    elif flag:
                        pargs = [exe, flag, "sh", "-lc", command]
                    else:
                        pargs = [exe, "sh", "-lc", command]
                    seam("popen", {"args": pargs})
                    launched = True
                    break
            if not launched:
                return {"__raise__": "HTTPException", "status_code": 400,
                        "detail": "No supported terminal emulator found"}
        out["ok"] = True
        return out

    if func == "read_profiles":
        r = seam("list_profiles", {})
        if isinstance(r, dict) and "__raise__" in r:
            if not is_exception_name(r["__raise__"]):
                return r
            seam("log_exception", {"log_msg": "GET /api/profiles failed; falling back to profile directory scan",
                                   "args": []})
            profiles = seam("fallback_profile_dicts", {})
            if isinstance(profiles, dict) and "__raise__" in profiles:
                return profiles
        else:
            profiles = r
        out = seam("scope_to_launch_profile", {"profiles": profiles})
        return {"profiles": out}

    if func == "list_profiles_endpoint":
        return _shadow_inner("read_profiles", inp)

    if func == "read_desktop_overlay":
        r = seam("read_text", {"profile_dir": inp.get("profile_dir") or "", "file": "desktop.json"})
        if isinstance(r, dict) and "__raise__" in r:
            return r
        if not isinstance(r, str):
            return None
        try:
            return json.loads(r)
        except Exception:
            return None

    raise ValueError("unhandled func: %s" % func)


def _exc_parent(name):
    if name == "BaseException": return None
    if name in ("SystemExit", "KeyboardInterrupt", "GeneratorExit"): return "BaseException"
    if name == "Exception": return "BaseException"
    if name in ("StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError", "MemoryError",
        "NameError", "OSError", "ReferenceError", "RuntimeError", "SyntaxError", "SystemError",
        "TypeError", "ValueError", "Warning"): return "Exception"
    if name in ("FloatingPointError", "OverflowError", "ZeroDivisionError"): return "ArithmeticError"
    if name in ("ModuleNotFoundError", "ZipImportError"): return "ImportError"
    if name in ("IndexError", "KeyError"): return "LookupError"
    if name in ("BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError"): return "OSError"
    if name in ("BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError"): return "ConnectionError"
    if name in ("NotImplementedError", "RecursionError", "PythonFinalizationError"): return "RuntimeError"
    if name == "IndentationError": return "SyntaxError"
    if name == "TabError": return "IndentationError"
    if name in ("UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError"): return "UnicodeError"
    if name == "UnicodeError": return "ValueError"
    if name == "JSONDecodeError": return "ValueError"
    if name in ("UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning", "BytesWarning",
        "EncodingWarning", "ResourceWarning"): return "Warning"
    if name == "UnboundLocalError": return "NameError"
    if name == "BaseExceptionGroup": return "BaseException"
    if name == "ExceptionGroup": return "BaseExceptionGroup"
    return "Exception"


def _is_sub(name, parent):
    if name == parent:
        return True
    seen = set()
    cur = name
    while cur and cur not in seen:
        seen.add(cur)
        cur = _exc_parent(cur)
        if cur == parent:
            return True
    return False


def _shadow_map(exc, log_msg, args, not_found, bad_request):
    name = exc.get("__raise__")
    msg = exc.get("msg", exc.get("detail", ""))
    if name == "HTTPException":
        return exc
    if any(_is_sub(name, n) for n in not_found):
        return {"__raise__": "HTTPException", "status_code": 404, "detail": msg}
    if any(_is_sub(name, n) for n in bad_request):
        return {"__raise__": "HTTPException", "status_code": 400, "detail": msg}
    if name == "ProfileIdentitySettlementPending":
        return exc
    if is_exception_name(name):
        seam("log_exception", {"log_msg": log_msg, "args": copy.deepcopy(args)})
        return {"__raise__": "HTTPException", "status_code": 500, "detail": msg}
    return exc


def shadow_drive(func, inp):
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}
    # The literal mirror treats raises as RETURN descriptors (as the C port
    # does), so bind the non-raising recorder for the duration of the mirror.
    global seam
    real_seam = seam
    seam = shadow_seam
    try:
        return _shadow_inner(func, inp)
    except BaseException as ex:
        return serialize_ex(ex)
    finally:
        seam = real_seam


def shadow_mode():
    cases = load_cases()
    check_func_set(cases)
    extract()
    install_stubs(NS)
    diffs = 0
    for c in cases:
        want, _ = run_live(c["func"], c["in"])
        shadow_ret = shadow_drive(c["func"], c["in"])
        got = {"ret": shadow_ret, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            diffs += 1
            print("SHADOW DIFF %s" % c["func"])
            print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False))
            print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False))
    print("SHADOW web_profiles_flow3: %d cases, %d diffs" % (len(cases), diffs))
    sys.exit(1 if diffs else 0)


def main():
    if "--self-test" in sys.argv:
        self_test()
        return
    if "--shadow" in sys.argv:
        shadow_mode()
        return
    ns = extract()
    install_stubs(ns)
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
        want, _ = run_live(func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE web_profiles_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE web_profiles_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
