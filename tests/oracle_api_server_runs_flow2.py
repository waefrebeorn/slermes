#!/usr/bin/env python3
"""Oracle for api_server_runs_flow2 (the second UNCLAIMED FLOW remainder of
gateway/platforms/api_server.py, ported in api_server_runs_flow2.c).

The LIVE defs are AST-extracted from the quarry (no heavy gateway/aiohttp
import); their module constants are seeded (Assign + AnnAssign) and EVERY
collaborator (os.getenv, _coerce_port, the delegated handlers, the import-chain
helpers, web.json_response, the store constructors, the runs delegates, the
background-task set and getattr on session/entry doubles) is replaced by a
recording seam that returns the SAME scripted result the C harness feeds its
universal seam. The oracle compares the DECISION SEQUENCE (ordered trace of
seam calls) plus the return value / mutated state, not just a final scalar.

Input: stdin JSON lines emitted by t_port_api_server_runs_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}}}
Summary: ORACLE api_server_runs_flow2: N cases, M mismatches. Exit 1 on any
mismatch; fail-closed FATAL exit 2 on extraction failure or zero cases.

Exception fidelity: make_exc() builds REAL builtin exception objects (the full
BaseException hierarchy is enumerated in builtin_hierarchy() below), so the
live code's own `except Exception` / `suppress(TypeError, AttributeError)`
sites discriminate BaseException-family raises exactly as the C port's
asr2_exc_parent table does. A BaseException-family seam raise therefore
propagates to the output as a {"__raise__":kind,"message":m} spec.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import json
import os
import sys
import types
from collections import deque

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "platforms", "api_server.py")
NAME = "api_server_runs_flow2"

MODULE_FUNCS = {
    "listen_address", "_content_has_visible_payload", "_session_row",
    "_normalize_chat_content", "_room_grant_delegate", "_run_route_delegate",
}
LEAF_FUNCS = {"_cap_text", "_cap_list"}
METHOD_FUNCS = {
    "_handle_skills", "_handle_toolsets", "_ensure_store", "_store_beside",
    "_current_response_store", "_check_run_auth", "_request_owns_run",
    "_track_background_task",
}
WANTED = MODULE_FUNCS | LEAF_FUNCS | METHOD_FUNCS

STATE_DEFAULTS = {
    "gateway_runner": None,
    "_session_db": None,
    "_response_store_home": "",
    "_response_store": None,
    "_response_stores": {},
    "_background_tasks": [],
}

CUR = {}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


# ── FULL builtin exception hierarchy (real classes + ancestor map) ──────────
def builtin_hierarchy():
    """Enumerate every builtin BaseException subclass and map child->parent.

    Used by make_exc indirectly (real classes) and exposed for the --selftest
    so the C-side asr2_exc_parent table can be verified against live Python.
    """
    parents = {}
    for name in dir(builtins):
        obj = getattr(builtins, name)
        if isinstance(obj, type) and issubclass(obj, BaseException):
            bases = [b for b in obj.__bases__ if issubclass(b, BaseException)]
            if len(bases) == 1:
                parents[obj.__name__] = bases[0].__name__
    parents["CancelledError"] = "BaseException"  # asyncio.CancelledError
    parents["_ProviderAuthResolutionError"] = "RuntimeError"
    return parents


EXC_PARENTS = builtin_hierarchy()


def exc_is(kind, ancestor):
    seen = set()
    while kind and kind not in seen:
        if kind == ancestor:
            return True
        seen.add(kind)
        kind = EXC_PARENTS.get(kind)
    return False


def make_exc(spec):
    """Build a REAL exception instance for a scripted raise spec."""
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "KeyError":
        try:
            key = ast.literal_eval(msg)
        except Exception:
            key = msg
        return KeyError(key)
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    if kind == "_ProviderAuthResolutionError":
        return type(kind, (RuntimeError,), {})(msg)
    return type(kind, (Exception,), {})(msg)


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


# ── script access (mirrors the harness stub semantics, fail-closed) ─────────
def next_script(name):
    io = CUR["io"]
    if name not in io:
        return Raise({"__raise__": "RuntimeError", "message": "no script for " + name})
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            return Raise({"__raise__": "RuntimeError", "message": "script exhausted for " + name})
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def proj(x):
    """Project a live object to a JSON-comparable value for a seam arg."""
    if hasattr(x, "__seam_proj__"):
        return x.__seam_proj__()
    if isinstance(x, dict):
        return {k: proj(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [proj(v) for v in x]
    if isinstance(x, BaseException):
        return str(x)
    if isinstance(x, type):
        return x.__name__
    return x


def seam(name, args):
    r = next_script(name)
    args = proj(args)
    if isinstance(r, Raise):
        CUR["raises"].append(r)
        rec({"ev": name, "args": args, "ret": r.spec})
        raise r.exc
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = make_exc(r)
        CUR["raises"].append(Raise(r))
        raise e
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


# ── object doubles ──────────────────────────────────────────────────────────
class PathDouble:
    def __init__(self, p):
        self.p = str(p)

    def __truediv__(self, other):
        return PathDouble(self.p.rstrip("/") + "/" + str(other))

    def __str__(self):
        return self.p

    def __seam_proj__(self):
        return self.p


class GetattrDouble:
    """Session/entry double: every attribute read fires the getattr seam."""

    def __init__(self, kind):
        self._kind = kind

    def __getattr__(self, key):
        if key.startswith("__") and key.endswith("__"):
            raise AttributeError(key)
        return seam("getattr", {"obj": self._kind, "key": key})


class SetDouble:
    """The adapter's _background_tasks set, backed by a JSON-array state key."""

    def __init__(self, backing):
        self.backing = backing

    def add(self, task):
        seam("bg.add", {"task": proj(task)})
        v = proj(task)
        if v not in self.backing:
            self.backing.append(v)

    def discard(self, task):
        v = proj(task)
        if v in self.backing:
            self.backing.remove(v)

    def __seam_proj__(self):
        return sorted(self.backing)


class TaskDouble:
    def __init__(self, data):
        self.data = data

    def add_done_callback(self, cb):
        return seam("bg.add_done_callback", {"task": proj(self)})

    def __seam_proj__(self):
        return self.data


class Adapter:
    """Marker self passed to runs.* delegate seams."""

    def __seam_proj__(self):
        return "<adapter>"


ADAPTER = Adapter()


class DelegateModule:
    def __init__(self, seamname):
        self._seamname = seamname

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)

        async def f(*a, **k):
            return seam(self._seamname, {"name": name})

        return f


class RunsModule:
    def _check_run_auth(self, adapter, request, *, permission, _api_server=None):
        return seam("runs._check_run_auth",
                    {"a0": "<adapter>", "a1": proj(request), "permission": permission})

    def _request_owns_run(self, adapter, request, run_id):
        return seam("runs._request_owns_run",
                    {"a0": "<adapter>", "a1": proj(request), "a2": run_id})

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)

        async def f(*a, **k):
            return seam("run_route_call", {"name": name})

        return f


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class SilentLogger:
    def __getattr__(self, _k):
        def noop(*a, **kw):
            return None
        return noop


def install_stubs(ns):
    """Seed every global the extracted defs resolve at call time."""
    ns["os"] = types.SimpleNamespace(
        getenv=lambda name, default=None: seam("getenv", {"name": name, "default": proj(default)}))
    ns["_coerce_port"] = lambda value, default: seam(
        "_coerce_port", {"value": proj(value), "default": default})
    ns["suppress"] = contextlib.suppress
    ns["asyncio"] = asyncio
    ns["Any"] = object
    ns["Optional"] = object

    def json_response(body, status=200, headers=None):
        return seam("web.json_response", {"body": proj(body)})

    ns["web"] = types.SimpleNamespace(json_response=json_response, Response=object,
                                      Request=object, StreamResponse=object)
    ns["_error_response"] = lambda message, status, err_type=None: seam(
        "_error_response", {"message": message, "status": status, "err_type": err_type})
    ns["logger"] = SilentLogger()

    ns["_room_grants"] = DelegateModule("room_grant_call")
    ns["_api_runs"] = RunsModule()
    ns["sys"] = sys
    ns["_openai_error"] = lambda message, **kwargs: {"message": message, **kwargs}
    ns["_api_request_profile"] = types.SimpleNamespace(get=lambda: None)

    def get_hermes_home_stub():
        return PathDouble(seam("get_hermes_home", {}))

    def selected_session_db_stub(adapter, home):
        return seam("selected_session_db", {"a0": "<adapter>", "a1": proj(home)})

    def open_and_cache_stub(home):
        return seam("_open_and_cache_session_db", {"a0": proj(home)})

    def adapter_persistence_stub(adapter):
        return seam("adapter_persistence", {"a0": "<adapter>"})

    ns["get_hermes_home"] = get_hermes_home_stub
    ns["selected_session_db"] = selected_session_db_stub
    ns["_open_and_cache_session_db"] = open_and_cache_stub
    ns["adapter_persistence"] = adapter_persistence_stub
    ns["ResponseStore"] = lambda **kw: seam("ResponseStore", {"db_path": kw.get("db_path")})

    def pkg(name):
        m = sys.modules.setdefault(name, types.ModuleType(name))
        m.__path__ = []
        return m

    def mod(name):
        m = types.ModuleType(name)
        sys.modules[name] = m
        return m

    hc = mod("hermes_constants")
    hc.get_hermes_home = get_hermes_home_stub
    pkg("gateway")
    gp = pkg("gateway.platforms")
    store = mod("gateway.platforms.api_server_store")
    store.selected_session_db = selected_session_db_stub
    store.adapter_persistence = adapter_persistence_stub

    # in-function import surfaces
    pkg("tools")
    skills = mod("tools.skills_tool")
    skills.find_all_skills = lambda **kw: seam(
        "find_all_skills", {"skip_disabled": kw.get("skip_disabled"),
                            "include_editorial": kw.get("include_editorial")})
    skills.sort_skills = lambda skills: seam("sort_skills", {"a0": proj(skills)})

    pkg("hermes_cli")
    cfg = mod("hermes_cli.config")
    cfg.load_config = lambda: seam("load_config", {})
    tc = mod("hermes_cli.tools_config")
    tc._get_platform_tools = lambda config, platform, include_default_mcp_servers=False: seam(
        "_get_platform_tools", {"a0": proj(config), "a1": platform,
                                "include_default_mcp_servers": include_default_mcp_servers})
    tc.get_nous_subscription_features = lambda config: seam(
        "get_nous_subscription_features", {"a0": proj(config)})
    tc._get_effective_configurable_toolsets = lambda: seam(
        "_get_effective_configurable_toolsets", {})
    tc._toolset_has_keys = lambda name, config, features=None: seam(
        "_toolset_has_keys", {"a0": name, "a1": proj(config), "features": proj(features)})
    ts = mod("toolsets")
    ts.resolve_toolset = lambda name: seam("resolve_toolset", {"a0": name})


def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("api_server_runs_flow2_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({"__name__": "api_server_runs_flow2_extracted", "json": json,
               "contextlib": contextlib, "copy": copy, "builtins": builtins})
    tree = ast.parse(src)
    import __future__
    flags = __future__.annotations.compiler_flag

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)

    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                exec_node(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in (MODULE_FUNCS | LEAF_FUNCS):
            node.decorator_list = []
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "APIServerAdapter":
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name in METHOD_FUNCS:
                    m = copy.copy(m)
                    m.decorator_list = []
                    exec_node(m)
    missing = WANTED - set(ns)
    if missing:
        raise RuntimeError("extraction missed: %s" % sorted(missing))
    install_stubs(ns)
    ns["logger"] = SilentLogger()
    CUR["ns"] = ns
    return ns


def setup_case(func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    state = copy.deepcopy(inp.get("state") or {})
    for k, v in STATE_DEFAULTS.items():
        state.setdefault(k, copy.deepcopy(v))
    CUR["state"] = state
    sd = types.SimpleNamespace(**state)
    sd._background_tasks = SetDouble(state["_background_tasks"])
    sd._open_and_cache_session_db = CUR["ns"]["_open_and_cache_session_db"]
    sd._response_store_lock = NoopLock()
    CUR["self"] = sd
    return sd


def drive(func, inp):
    ns = CUR["ns"]
    sd = CUR["self"]
    if func == "listen_address":
        return ns["listen_address"](inp.get("extra") or {})
    if func == "content_has_visible_payload":
        return ns["_content_has_visible_payload"](inp.get("content"))
    if func == "session_row":
        entry = GetattrDouble("entry") if inp.get("entry") is not None else None
        return ns["_session_row"](GetattrDouble("session"), entry)
    if func == "normalize_chat_content":
        return ns["_normalize_chat_content"](inp.get("content"))
    if func == "room_grant_delegate":
        handler = ns["_room_grant_delegate"](inp.get("name", "handler"))
        return asyncio.run(handler(ADAPTER, object()))
    if func == "run_route_delegate":
        handler = ns["_run_route_delegate"](inp.get("name", "handler"))
        return asyncio.run(handler(ADAPTER, object()))
    if func == "handle_skills":
        return asyncio.run(ns["_handle_skills"](sd, object()))
    if func == "handle_toolsets":
        return asyncio.run(ns["_handle_toolsets"](sd, object()))
    if func == "ensure_store":
        return ns["_ensure_store"](sd)
    if func == "store_beside":
        return ns["_store_beside"](sd, inp.get("db"))
    if func == "current_response_store":
        return ns["_current_response_store"](sd)
    if func == "check_run_auth":
        return ns["_check_run_auth"](sd, inp.get("request"), permission=inp.get("permission", "status"))
    if func == "request_owns_run":
        return ns["_request_owns_run"](sd, inp.get("request"), inp.get("run_id", ""))
    if func == "track_background_task":
        return ns["_track_background_task"](sd, TaskDouble(inp.get("task")),
                                           tolerate_missing=inp.get("tolerate_missing", False))
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    setup_case(func, inp)
    try:
        want_ret = proj(drive(func, inp))
    except BaseException as e:
        want_ret = None
        for r in CUR["raises"]:
            if e is r.exc:
                want_ret = r.spec
                break
        if want_ret is None:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": want_ret, "trace": CUR["trace"], "state": proj(CUR["state"])}


def canon(x):
    """Order-insensitive-for-objects, representation-insensitive comparison."""
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def selftest():
    bad = []
    for child, parent in EXC_PARENTS.items():
        if parent and parent not in EXC_PARENTS and parent != "BaseException":
            bad.append((child, parent))
    print("SELFTEST exception hierarchy: %d builtin kinds, %d suspicious" %
          (len(EXC_PARENTS), len(bad)))
    return 0 if not bad else 1


def main():
    if "--selftest" in sys.argv:
        return selftest()
    try:
        ns = extract()
    except BaseException as e:
        print("FATAL extraction failed: %r" % e)
        return 2
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
        want = run_case(ns, func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, cases, mism))
    if cases == 0:
        print("FATAL zero cases compared")
        return 2
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
