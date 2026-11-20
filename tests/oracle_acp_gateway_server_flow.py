#!/usr/bin/env python3
"""Oracle for src/acp_adapter/acp_gateway_server_flow.c.

Reads harness JSONL on stdin. For every case it AST-extracts the LIVE defs of
acp_adapter/gateway_server.py (top-level defs and the GatewayACPAgent methods),
syncifies them (AsyncFunctionDef->FunctionDef, drops ``await``, rewrites
``async with``/``async for`` and strips in-function imports), drives the target
under the SAME universal seams the C harness feeds, and diffs the ordered
trace, the return (or raise spec) and the state window. Prints
'ORACLE acp_gateway_server_flow: N cases, M mismatches'.

Case shape (shared with the C harness):
  {"func": <live_name>, "in": {"argv": [...], "kw": {...}, "seams": {...}}}
For instance methods argv[0] is the self-state object; the remaining argv
entries (and kw) are the call's real arguments. Module-level/static functions
take their real arguments in argv directly.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- ``is True``/``is not None`` identity gates are modelled exactly (JSON_BOOL
  true only for ``is True``; json_obj_get presence for attribute reads).
- collaborator calls are universal seams: one scripted queue per seam name,
  first scripted return per call, last value repeats; an absent/exhausted
  script yields a RuntimeError raise spec recorded as the event ret before it
  propagates. Full builtin exception hierarchy is used for ``except`` matching;
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass straight through ``except Exception``.
- logger events project as {"msg", "args":[...], "exc_info": bool?}.
- rpc (client/gateway) is one seam named "rpc".
- tuples (keys, fences) travel as JSON arrays.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable, Dict, List, Optional
import __future__ as _fut

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
_QUARRY = _REPO.parent / "hermes-agent"
if not (_QUARRY / "acp_adapter" / "gateway_server.py").is_file():
    _QUARRY = Path.home() / "hermes-agent"
SRC = _QUARRY / "acp_adapter" / "gateway_server.py"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
ANN = _fut.annotations.compiler_flag

# The 20 unclaimed defs ported by acp_gateway_server_flow.c.
FUNCS = [
    "_slash_command", "_tools_changed_notice", "_bypass_route",
    "_editor_id", "_moved", "_transport_closed", "_held_session",
    "_has_unknown", "_pending_event_is_new", "_blocked_admission",
    "_unknown_recovery", "_request_live_reasoning", "_after_attach",
    "_admission_from_receipt", "_after_ack", "_project_title",
    "_project_usage", "_permission", "_project_config", "_answer_permission",
]
MODULE_FUNCS = {"_slash_command", "_tools_changed_notice", "_bypass_route", "_moved"}
# Methods that are ported and therefore composed as REAL (syncified) methods on
# the self double rather than as recorder stubs.
PORTED_METHODS = [f for f in FUNCS if f not in MODULE_FUNCS]

# ---------------- exception table (full builtin hierarchy) ----------------
EXC = {}
for _n in dir(builtins):
    _o = getattr(builtins, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        EXC[_n] = _o
try:
    import asyncio as _asio
    EXC["CancelledError"] = _asio.CancelledError
except Exception:
    pass


class GatewayClientError(Exception):
    pass


EXC["GatewayClientError"] = GatewayClientError
EXC["GatewayRPCError"] = type("GatewayRPCError", (GatewayClientError,), {})
EXC["CommandRefused"] = type("CommandRefused", (Exception,), {})


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    try:
        return cls(msg)
    except Exception:
        e = cls.__new__(cls)
        try:
            Exception.__init__(e, msg)
        except Exception:
            pass
        return e


def is_raise(x):
    return isinstance(x, dict) and "__raise__" in x


def norm(x, _d=0):
    if _d > 30:
        return str(x)
    if x is None:
        return None
    if isinstance(x, SimpleNamespace):
        return {k: norm(v, _d + 1) for k, v in vars(x).items()}
    if isinstance(x, BaseException):
        return {"__raise__": type(x).__name__, "message": str(x)}
    if isinstance(x, dict):
        return {k: norm(v, _d + 1) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v, _d + 1) for v in x]
    if isinstance(x, (bool, int, str, float)):
        return x
    return str(x)


# ---------------- universal seam recorder ----------------
CURRENT = {"ctx": None}


class Ctx:
    def __init__(self, seams):
        self.seams = seams or {}
        self.cursors = {}
        self.trace = []

    def seam(self, name, args):
        args = norm(args)
        sc = self.seams.get(name)
        ret = None
        have = False
        if sc is not None:
            if isinstance(sc, list):
                i = self.cursors.get(name, 0)
                if i < len(sc):
                    ret, have = sc[i], True
                    self.cursors[name] = i + 1
            else:
                ret, have = sc, True
        if not have:
            ret = {"__raise__": "RuntimeError", "message": "no script for " + name}
        self.trace.append({"ev": name, "args": args, "ret": norm(ret)})
        if is_raise(ret):
            raise build_exc(ret)
        return ret


def _ctx():
    return CURRENT["ctx"]


# ---------------- AST syncification + extraction ----------------
class Syncify(ast.NodeTransformer):
    """Turn async constructs into their sync equivalents for faithful replay."""

    def visit_AsyncFunctionDef(self, node):
        node = self.generic_visit(node)
        node.decorator_list = []  # strip staticmethod/other wrappers
        node.__class__ = ast.FunctionDef  # identical fields, keeps lineno/end_lineno
        return node

    def visit_Await(self, node):
        return self.visit(node.value)

    def visit_AsyncWith(self, node):
        node = self.generic_visit(node)
        node.__class__ = ast.With  # identical fields, keeps lineno/end_lineno
        return node

    def visit_AsyncFor(self, node):
        node = self.generic_visit(node)
        node.__class__ = ast.For  # identical fields, keeps lineno/end_lineno
        return node

    def visit_ImportFrom(self, node):
        # In-function imports are replaced by pre-seeded global stubs.
        return None

    def visit_Import(self, node):
        return None


def _compile_fn(node, ns):
    node = copy.deepcopy(node)
    node = Syncify().visit(node)
    ast.fix_missing_locations(node)
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, str(SRC), "exec", flags=ANN, dont_inherit=True), ns)


NS = {}


class RecLogger:
    def _log(self, level, msg, *args, **kwargs):
        d = {"msg": msg, "args": list(args)}
        if "exc_info" in kwargs:
            d["exc_info"] = kwargs["exc_info"]
        return _ctx().seam("logger." + level, d)

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, *a, **k)

    def info(self, msg, *a, **k):
        return self._log("info", msg, *a, **k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, *a, **k)


class NoopCM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


_OPTION_ID_TO_HERMES = {
    "allow_once": "once", "allow_session": "session", "allow_always": "always",
    "deny": "deny", "deny_always": "deny",
}
EDIT_APPROVAL_POLICY_CONFIG_ID = "edit_approval_policy"
EDIT_APPROVAL_POLICY_TO_MODE = {
    "ask": "ask", "acceptEdits": "acceptEdits", "bypassPermissions": "bypassPermissions",
}


def _install_stubs():
    NS.clear()
    # os.environ.get
    osm = types.ModuleType("os")
    env = types.SimpleNamespace(
        get=lambda key, default=None: _ctx().seam("os.environ.get", {"key": key, "default": default}))
    osm.environ = env
    NS["os"] = osm

    # acp module
    acp = types.ModuleType("acp")
    acp.update_agent_message_text = lambda text: _ctx().seam("update_agent_message_text", {"text": text})
    acp.RequestError = type("RequestError", (Exception,), {})
    NS["acp"] = acp

    # global collaborator stubs (in-function imports stripped)
    NS["resolve_command"] = lambda name: _build(
        _ctx().seam("resolve_command", {"name": name}))
    NS["_OPTION_ID_TO_HERMES"] = _OPTION_ID_TO_HERMES
    NS["ROUTES"] = RoutesProbe()
    NS["_normalize_cwd_for_compare"] = lambda cwd: _ctx().seam("_normalize_cwd_for_compare", {"cwd": cwd})
    NS["_translate_acp_cwd"] = lambda cwd: _ctx().seam("_translate_acp_cwd", {"cwd": cwd})
    NS["profile_name_for_home"] = lambda home: _ctx().seam("profile_name_for_home", {"home": home})
    NS["available_commands_update"] = lambda: _ctx().seam("available_commands_update", {})
    NS["title_update"] = lambda title: _ctx().seam("title_update", {"title": title})
    NS["usage_update"] = lambda stats: _ctx().seam("usage_update", {"stats": stats})
    NS["put_behind"] = lambda events, frame: _ctx().seam("put_behind", {"events": events, "frame": frame})
    NS["t"] = lambda key: _ctx().seam("t", {"key": key})

    NS["_OPTION_ID_TO_HERMES"] = _OPTION_ID_TO_HERMES
    NS["EDIT_APPROVAL_POLICY_CONFIG_ID"] = EDIT_APPROVAL_POLICY_CONFIG_ID
    NS["EDIT_APPROVAL_POLICY_TO_MODE"] = EDIT_APPROVAL_POLICY_TO_MODE

    class AllowedOutcome(SimpleNamespace):
        pass

    NS["AllowedOutcome"] = AllowedOutcome
    NS["PermissionOption"] = lambda **kw: SimpleNamespace(**kw)
    NS["GatewayClientError"] = GatewayClientError

    NS["_build_permission_options"] = lambda *, allow_permanent, allow_session: _build_list(
        _ctx().seam("build_permission_options",
                    {"allow_permanent": allow_permanent, "allow_session": allow_session}))
    NS["_build_permission_tool_call"] = lambda command, description: _ctx().seam(
        "build_permission_tool_call", {"command": command, "description": description})
    NS["_OPTION_ID_TO_HERMES"] = _OPTION_ID_TO_HERMES

    class EditProposal(SimpleNamespace):
        pass

    NS["EditProposal"] = EditProposal
    NS["build_acp_edit_tool_call"] = lambda proposal: _ctx().seam(
        "build_acp_edit_tool_call", {"proposal": norm(proposal)})

    # asyncio stub: create_task / sleep / CancelledError
    am = types.ModuleType("asyncio")

    def create_task(coro):
        _ctx().seam("create_task", {"target": coro})
        return TaskDouble()

    am.create_task = create_task
    am.sleep = lambda t: _ctx().seam("sleep", {"t": t})
    am.to_thread = lambda fn, *a: fn(*a)
    am.CancelledError = _asio.CancelledError
    am.Future = FutureDouble
    am.Lock = lambda: NoopCM()
    am.Condition = lambda: NoopCM()
    NS["asyncio"] = am

    NS["current_mode"] = lambda payload: _ctx().seam("current_mode", {"payload": payload})
    NS["model_state"] = lambda shown: _ctx().seam("model_state", {"shown": shown})
    NS["session_config_options"] = lambda gateway, sid, models, shown, mode: _ctx().seam(
        "session_config_options", {"sid": sid, "models": models, "shown": shown, "mode": mode})
    NS["ConfigOptionUpdate"] = lambda **kw: SimpleNamespace(**kw)
    NS["logger"] = RecLogger()
    NS["SimpleNamespace"] = SimpleNamespace

    # typing names used in annotations
    NS.update({"Any": Any, "Callable": Callable, "Dict": Dict, "List": List, "Optional": Optional})


class FutureDouble:
    def __init__(self, done_value):
        self._done = bool(done_value)

    def done(self):
        return self._done


def _build(v):
    if isinstance(v, dict):
        return SimpleNamespace(**v)
    return v


def _build_list(v):
    if isinstance(v, list):
        return [_build(x) for x in v]
    return v


class RoutesProbe:
    def __contains__(self, name):
        v = _ctx().seam("ROUTES", {"name": name})
        return name in v if isinstance(v, list) else False


def load_ns():
    _install_stubs()
    text = SRC.read_text(encoding="utf-8")
    tree = ast.parse(text)
    # module constants (Assign/AnnAssign) with simple Name targets
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module([node], []), str(SRC), "exec", flags=ANN), NS)
            except Exception:
                pass
    # module-level functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            _compile_fn(node, NS)
    # GatewayACPAgent methods (incl. staticmethods) as top-level names
    for cls in tree.body:
        if isinstance(cls, ast.ClassDef) and cls.name == "GatewayACPAgent":
            for node in cls.body:
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    _compile_fn(node, NS)
    missing = [f for f in FUNCS if f not in NS]
    if missing:
        print("FATAL oracle_acp_gateway_server_flow: missing %s" % missing)
        sys.exit(2)
    return NS


# ---------------- self double + doubles ----------------
class SelfDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", d)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % k)

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v


class ConnDouble:
    def __init__(self, ctx):
        self._ctx = ctx
        self._conn = SimpleNamespace(
            send_request=lambda method, params: ctx.seam("send_request", {"method": method, "params": params}))

    def session_update(self, *, session_id, update):
        return self._ctx.seam("conn.session_update", {"session_id": session_id, "update": update})

    def request_permission(self, *, session_id, tool_call, options):
        raw = self._ctx.seam("request_permission",
                             {"session_id": session_id, "tool_call": tool_call, "options": options})
        if isinstance(raw, dict) and isinstance(raw.get("outcome"), dict):
            oc = raw["outcome"]
            cls = NS["AllowedOutcome"] if oc.get("allowed_outcome") else SimpleNamespace
            raw = SimpleNamespace(**{**raw, "outcome": cls(**{k: v for k, v in oc.items()
                                                              if k != "allowed_outcome"})})
        return raw


class GatewayDouble:
    def __init__(self, ctx, events=None):
        self._ctx = ctx
        self.events = events

    def rpc(self, method, **kw):
        return self._ctx.seam("rpc", {"method": method, **kw})

    @property
    def reader(self):
        raw = self._ctx.seam("gateway.reader", {})
        if isinstance(raw, dict) and raw.get("is_future"):
            return FutureDouble(raw.get("done"))
        return raw


class TaskDouble:
    def add_done_callback(self, cb):
        return None


class ChangedDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def wait_for(self, predicate):
        return self._ctx.seam("changed.wait_for", {})

    def notify_all(self):
        return None


def _client_double(ctx):
    def client():
        raw = ctx.seam("client", {})
        return GatewayDouble(ctx)
    return client


def setup_self(state, ctx):
    d = dict(state)
    # GatewayACPAgent.__init__ always seeds these; make the double faithful so
    # composed helpers like _editor_id never hit a missing-attribute error.
    defaults = {
        "_aliases": {}, "_snapshots": {}, "_modes": {}, "_configs": {}, "_titles": {},
        "_usage": {}, "_admissions": {}, "_permissions": {}, "_pending_cancels": [],
        "_submitting": [], "_settling": [], "_receipted": {}, "_resumes": {},
        "_adopted": {}, "_cut": {}, "_streamed": {}, "_segments": {}, "_inflight": {},
        "_editor_mcp": {}, "_background": [], "_failure": None, "_link": 0,
        "_gateway": None, "_conn": None, "_home": "", "_form_elicitation": False,
    }
    for k, v in defaults.items():
        if k not in d:
            d[k] = copy.deepcopy(v)
    # set-valued state keys travel as JSON arrays; materialize as real sets so
    # .discard()/.add() in the live bodies work (mirrored by the C array helpers).
    if "_permissions" in d:
        # JSON object keys are strings; the live key is a (session, prompt, gen)
        # tuple. Encode as "|"-joined parts so membership is faithfully modelled.
        d["_permissions"] = {tuple(str(k).split("|")): v for k, v in (d["_permissions"] or {}).items()}
    for sk in ("_pending_cancels", "_submitting", "_settling", "_background"):
        if sk in d:
            d[sk] = set(d[sk] or [])
    slf = SelfDouble(d)
    # scalar-state -> collaborator doubles, bound as instance attrs so they
    # never pollute the emitted state window.
    object.__setattr__(slf, "_changed", ChangedDouble(ctx))
    object.__setattr__(slf, "_client", _client_double(ctx))
    conn = ConnDouble(ctx) if d.get("_conn") else None
    object.__setattr__(slf, "_conn", conn)
    gw = d.get("_gateway")
    events = gw.get("events") if isinstance(gw, dict) else None
    object.__setattr__(slf, "_gateway", GatewayDouble(ctx, events) if gw else None)
    # the two answer methods appear as recorder stubs when _permission registers
    # a task (the coroutine body is NOT executed at create_task time).
    object.__setattr__(slf, "_answer_permission",
                       lambda session_id, prompt: ctx.seam("answer.approval",
                                                           {"session_id": session_id, "prompt": prompt}))
    object.__setattr__(slf, "_answer_clarify",
                       lambda session_id, prompt: ctx.seam("answer.clarify",
                                                           {"session_id": session_id, "prompt": prompt}))
    object.__setattr__(slf, "_settled_receipt",
                       lambda client, held_id, session_id, receipt, saved=None: ctx.seam(
                           "settled_receipt", {"client": "client", "held_id": held_id,
                                               "session_id": session_id, "receipt": receipt}))
    object.__setattr__(slf, "_close_card",
                       lambda editor_id, tool_call, options, response: ctx.seam(
                           "close_card", {"editor_id": editor_id, "tool_call": tool_call,
                                          "options": options, "response": response}))
    # bind the other ported methods that are composed on the self double
    for meth in PORTED_METHODS:
        if meth == "_answer_permission":
            continue  # recorder stub already bound above; driven directly
        object.__setattr__(slf, meth, types.MethodType(NS[meth], slf))
    return slf, d


def invoke(func, argv, kw, slf, d, ctx):
    if func in MODULE_FUNCS:
        return NS[func](*argv, **kw)
    # Call the real underlying function with slf as self, so the driven method
    # uses its live body even when a same-named recorder stub is bound on slf
    # for composed call sites. A passed `client` argument is a gateway double.
    if func in ("_request_live_reasoning", "_admission_from_receipt") and argv:
        argv = [GatewayDouble(ctx)] + list(argv[1:])
    return NS[func](slf, *argv, **kw)


def drive(func, argv, kw, seams):
    ctx = Ctx(seams)
    CURRENT["ctx"] = ctx
    try:
        state = copy.deepcopy(argv[0]) if argv and isinstance(argv[0], dict) else {}
        if func in MODULE_FUNCS:
            # module/static functions: argv[0] is the real first argument
            slf, d = None, {}
            r = invoke(func, argv, kw, slf, d, ctx)
        else:
            slf, d = setup_self(state, ctx)
            r = invoke(func, argv[1:] if argv else [], kw, slf, d, ctx)
        ret = norm(r)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    finally:
        CURRENT["ctx"] = None
    return {"ret": ret, "trace": ctx.trace, "st": {}}


def canon(x):
    if isinstance(x, bool):
        return x
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    if isinstance(x, list):
        return [canon(v) for v in x]
    return x


# ---------------- main ----------------
def main():
    raw = sys.stdin.read()
    cases = []
    for line in raw.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    load_ns()
    mismatches = 0
    shown = 0
    for rec in cases:
        func = rec.get("func", "")
        ini = rec.get("in", {}) or {}
        argv = ini.get("argv", []) or []
        kw = ini.get("kw", {}) or {}
        seams = ini.get("seams", {}) or {}
        if func not in FUNCS:
            exp = {"ret": {"__raise__": "RuntimeError", "message": "unknown func " + func},
                   "trace": [], "st": {}}
        else:
            try:
                exp = drive(func, argv, kw, seams)
            except BaseException as e:
                exp = {"ret": {"__raise__": type(e).__name__, "message": str(e)},
                       "trace": [], "st": {}}
        got = {"ret": rec.get("ret"), "trace": rec.get("trace", []), "st": rec.get("st", {})}
        ok = True
        for k in ("ret", "trace", "st"):
            if canon(got.get(k)) != canon(exp.get(k)):
                ok = False
                break
        if not ok:
            mismatches += 1
            if shown < 120:
                shown += 1
                print("MISMATCH func=%s argv=%r" % (func, argv), file=sys.stderr)
                for k in ("ret", "trace", "st"):
                    if canon(got.get(k)) != canon(exp.get(k)):
                        print("  %s\n    got=%r\n    exp=%r" % (k, got.get(k), exp.get(k)), file=sys.stderr)
    print("ORACLE acp_gateway_server_flow: %d cases, %d mismatches" % (len(cases), mismatches))
    return 1 if mismatches else 0


def selftest():
    fx = _HERE.parent / "fixtures" / "acp_gateway_server_flow.jsonl"
    if not fx.is_file():
        print("SELFTEST: no fixture file")
        return 2
    load_ns()
    bad = []
    names = set(FUNCS)
    seam_names = {
        "resolve_command", "ROUTES", "os.environ.get", "_normalize_cwd_for_compare",
        "_translate_acp_cwd", "gateway.reader", "profile_name_for_home", "rpc",
        "logger.debug", "logger.info", "available_commands_update", "update_agent_message_text",
        "conn.session_update", "client", "sleep", "usage_update", "title_update",
        "changed.wait_for", "put_behind", "t", "settled_receipt", "answer.approval",
        "answer.clarify", "create_task", "build_permission_options", "build_permission_tool_call",
        "build_acp_edit_tool_call", "request_permission", "close_card", "send_request",
    }
    total = 0
    for line in fx.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        case = json.loads(line)
        total += 1
        func = case.get("func")
        if func not in names:
            bad.append("%s: func not in extraction map" % func)
            continue
        ini = case.get("in", {})
        r = drive(func, ini.get("argv", []), ini.get("kw", {}), ini.get("seams", {}))
        ret = r["ret"]
        if is_raise(ret) and ret.get("__raise__") in ("AttributeError", "NameError", "TypeError"):
            msg = ret.get("message", "")
            for n in list(names) + list(seam_names):
                if ("'%s'" % n) in msg or n in msg:
                    bad.append("%s: %s (%s)" % (func, ret["__raise__"], msg[:110]))
                    break
    print("SELFTEST: %d cases, %d suspicious" % (total, len(bad)))
    for b in bad[:25]:
        print("  " + b)
    return 1 if bad else 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    sys.exit(main())
