#!/usr/bin/env python3
"""Oracle for src/acp_adapter/acp_gateway_server_flow2.c.

The SECOND-WAVE remainder of acp_adapter/gateway_server.py: the 31 faithful class-level
defs NOT claimed by acp_gateway_server_flow (flow1 owns 20 decision helpers).
Reads harness JSONL on stdin, AST-extracts the LIVE defs (syncified), drives each
under the SAME universal seam contract the C harness feeds, and diffs the ordered
trace, the return (or raise spec) and the (empty) state window. Prints
'ORACLE acp_gateway_server_flow2: N cases, M mismatches'.

Case shape (shared with the C harness):
  {"func": <live_name>, "in": {"argv": [self_state, ...args], "kw": {...},
                               "seams": {name: const | [queue]}}}
argv[0] is always the self-state object; the remaining argv entries + kw are the
call's real arguments. Every travelled object stays plain JSON.

Seam contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- collaborator calls are universal seams with a fail-closed RuntimeError raise
  spec ({"__raise__":"RuntimeError","message":"no script for <name>"}) recorded
  as the event ret before it propagates. A scripted null is a valid None.
- full builtin exception hierarchy is used for except matching; BaseException-
  only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError) pass
  straight through `except Exception`.
- method collaborators use seam args {"p":[...], "k":{...}}; `client.rpc` uses
  {"method":..., **kw}; `conn.session_update` uses {"session_id","update"}.
- logger events project as {"msg","args":[...],"exc_info":bool?}.
"""
import ast
import builtins
import contextlib
import copy
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable, Dict, List, Optional
import __future__ as _fut
import asyncio as _asio

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

# The 31 faithful class-level defs ported by acp_gateway_server_flow2.c (nested closures
# `say`/`confirm` are handled inline in their parents, not separate targets).
FUNCS = [
    "__init__", "on_connect", "_observe_client_message", "initialize",
    "authenticate", "new_session", "_client", "_adopt", "_move_workspace",
    "_rebind_editor_mcp", "load_session", "resume_session", "cancel",
    "fork_session", "set_session_model", "_confirm_deferred_pick",
    "_model_confirmer", "set_session_mode", "set_config_option",
    "_idle_control", "list_sessions", "prompt", "_prepare_submit",
    "_refresh_unknown", "_events", "_project", "_gap_guard",
    "_answer_clarify", "_session_fields", "_teardown", "_notify_unknown",
]
assert len(FUNCS) == 31 and len(set(FUNCS)) == 31

# Every self.<method> collaborator that a flow2 body reaches is a seam (a recorder
# bound as an INSTANCE attribute, no self passed). Flow1-claimed siblings are
# seamed here too (never re-ported). Pure leaves read inline in C are transcribed,
# not seamed (they have no collaborator side effect).
SELF_METHODS = [
    "_client", "_teardown", "_transport_closed", "_reattach", "_adopt",
    "_request_live_reasoning", "_notify_unknown", "_permission", "_moved",
    "_move_workspace", "_rebind_editor_mcp", "_cancel_admission",
    "_model_confirmer", "_held_session", "_set_edit_policy", "_idle_control",
    "_has_unknown", "_refresh_unknown_for_prompt", "_refresh_unknown",
    "_unknown_recovery",
    "_prepare_submit", "_run_slash", "_admission_from_receipt", "_after_ack",
    "_blocked_admission", "_settled_receipt", "_finish_reply", "_recover",
    "_project", "_gap_guard", "_project_session_state", "_project_mode",
    "_project_config", "_project_usage", "_project_title", "_project_side_update",
    "_project_tool", "_confirm_deferred_pick", "_retire_unanswered",
    "_answer_clarify", "_answer_permission", "_editor_id", "_session_fields",
    "_resume", "_attach_info", "_chunk", "_close_segment", "_end_message",
    "_pending_event_is_new", "_project_side_update",
    "_observe_client_message", "initialize", "authenticate", "on_connect",
    "cancel", "list_sessions", "prompt", "say", "aclose", "set_session_model",
]

# ---------------- exception hierarchy ----------------
EXC = {}
for _n in dir(builtins):
    _o = getattr(builtins, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        EXC[_n] = _o
EXC["CancelledError"] = _asio.CancelledError


class GatewayClientError(ValueError):
    pass


class GatewayRPCError(GatewayClientError):
    pass


class GatewayUnavailableError(GatewayClientError):
    pass


class CommandRefused(Exception):
    pass


class RequestError(Exception):
    pass


EXC.update({
    "GatewayClientError": GatewayClientError,
    "GatewayRPCError": GatewayRPCError,
    "GatewayUnavailableError": GatewayUnavailableError,
    "CommandRefused": CommandRefused,
    "RequestError": RequestError,
})


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
    # Object doubles project to stable markers (C harness cannot emit Python
    # memory addresses). Keep these in sync with the C port's marker strings.
    _cn = x.__class__.__name__
    if _cn == "GatewayDouble":
        return "GW"
    if _cn == "TaskDouble":
        return "TASK"
    if _cn == "SelfDouble":
        return "SELF"
    if _cn in ("ConnectionDouble", "ConnDouble", "MutationDouble", "ChangedDouble"):
        return _cn
    if isinstance(x, types.FunctionType):
        return "<fn>"
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
        if name in self.seams:  # presence, not .get() is None (scripted null is valid)
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


def R(name):
    """Generic method collaborator: seam args {"p":[...], "k":{...}}."""
    def f(*a, **k):
        return _ctx().seam(name, {"p": list(a), "k": k})
    return f


# ---------------- AST syncification + extraction ----------------
class Syncify(ast.NodeTransformer):
    def visit_AsyncFunctionDef(self, node):
        node = self.generic_visit(node)
        node.decorator_list = []
        node.__class__ = ast.FunctionDef
        return node

    def visit_Await(self, node):
        return self.visit(node.value)

    def visit_AsyncWith(self, node):
        node = self.generic_visit(node)
        node.__class__ = ast.With
        return node

    def visit_AsyncFor(self, node):
        node = self.generic_visit(node)
        node.__class__ = ast.For
        return node

    def visit_ImportFrom(self, node):
        return None

    def visit_Import(self, node):
        return None


def _compile_fn(node, ns):
    node = Syncify().visit(copy.deepcopy(node))
    ast.fix_missing_locations(node)
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, str(SRC), "exec", flags=ANN, dont_inherit=True), ns)


NS = {}


class RecLogger:
    def _log(self, level, msg, *args, **kwargs):
        d = {"msg": msg, "args": list(args)}
        if "exc_info" in kwargs:
            d["exc_info"] = kwargs["exc_info"]
        if args and isinstance(args[-1], BaseException):
            d["args"] = list(args[:-1]) + [str(args[-1])]
        return _ctx().seam("logger." + level, d)

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, *a, **k)

    def info(self, msg, *a, **k):
        return self._log("info", msg, *a, **k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, *a, **k)


# module constants (hard-coded in the C port)
MODE_TO_EDIT_APPROVAL_POLICY = {
    "default": "ask", "ask": "ask", "acceptEdits": "acceptEdits",
    "bypassPermissions": "bypassPermissions",
}
EDIT_APPROVAL_POLICY_TO_MODE = {
    "ask": "ask", "acceptEdits": "acceptEdits", "bypassPermissions": "bypassPermissions",
}
EDIT_APPROVAL_POLICY_CONFIG_ID = "edit_approval_policy"
REASONING_CONFIG_ID = "reasoning_effort"
SERVICE_TIER_CONFIG_ID = "service_tier"
SERVICE_TIER_LABELS = ("fast", "slow")
_OPTION_ID_TO_HERMES = {
    "allow_once": "once", "allow_session": "session", "allow_always": "always",
    "deny": "deny", "deny_always": "deny",
}
ELICITATION_METHOD = "elicitation/create"
SKIP_OPTION = "skip"


def _install_stubs():
    NS.clear()
    NS["GatewayClientError"] = GatewayClientError
    NS["GatewayRPCError"] = GatewayRPCError
    NS["CommandRefused"] = CommandRefused
    NS["RequestError"] = RequestError
    NS["logger"] = RecLogger()
    NS["contextlib"] = contextlib
    NS["suppress"] = contextlib.suppress
    NS["SimpleNamespace"] = SimpleNamespace

    # flow1-claimed pure helpers used as module globals by flow2 bodies
    for _n in ("_slash_command", "_tools_changed_notice"):
        NS[_n] = R(_n)

    class _HermesACPAgent:
        @staticmethod
        def authenticate(slf, method_id, **kw):
            return _ctx().seam("authenticate", {"method_id": method_id, **kw})
    NS["HermesACPAgent"] = _HermesACPAgent

    # typing names used in annotations
    NS.update({"Any": Any, "Callable": Callable, "Dict": Dict,
               "List": List, "Optional": Optional})

    # constants
    NS.update({
        "MODE_TO_EDIT_APPROVAL_POLICY": MODE_TO_EDIT_APPROVAL_POLICY,
        "EDIT_APPROVAL_POLICY_TO_MODE": EDIT_APPROVAL_POLICY_TO_MODE,
        "EDIT_APPROVAL_POLICY_CONFIG_ID": EDIT_APPROVAL_POLICY_CONFIG_ID,
        "REASONING_CONFIG_ID": REASONING_CONFIG_ID,
        "SERVICE_TIER_CONFIG_ID": SERVICE_TIER_CONFIG_ID,
        "SERVICE_TIER_LABELS": SERVICE_TIER_LABELS,
        "_OPTION_ID_TO_HERMES": _OPTION_ID_TO_HERMES,
        "ELICITATION_METHOD": ELICITATION_METHOD,
        "SKIP_OPTION": SKIP_OPTION,
    })

    # global collaborators -> seams
    for name in (
        "connect_gateway", "get_hermes_home", "gateway_profile_home",
        "env_launch_flags", "_bypass_route", "_translate_acp_cwd",
        "_normalize_cwd_for_compare", "logical_session_id", "profile_name_for_home",
        "put_behind", "build_auth_methods", "parse_model_input",
        "deferred_pick_text", "refused_pick_text", "_model_text",
        "model_refusal_text", "confirmation_title", "_history_replay_updates",
        "client_supports_form_elicitation", "catalog_sessions",
        "_content_blocks_to_openai_user_content", "submit_fingerprint",
        "stage_user_content", "run_command", "pending_status", "settled",
        "_text", "elicited_answer", "build_clarify_elicitation",
        "build_clarify_permission", "_build_permission_options",
        "_build_permission_tool_call", "build_acp_edit_tool_call",
        "model_state", "provenance_meta", "session_config_options",
        "session_modes", "current_mode", "reasoning_choices", "usage_update",
        "title_update", "available_commands_update", "turn_usage",
    ):
        NS[name] = R(name)

    # uuid
    um = types.ModuleType("uuid")

    def uuid4():
        r = _ctx().seam("uuid", {})
        h = r if isinstance(r, str) else (r.get("hex", "") if isinstance(r, dict) else "")
        return SimpleNamespace(hex=h)
    um.uuid4 = uuid4
    NS["uuid"] = um

    # acp module + schema constructors + acp.* helper seams
    acp = types.ModuleType("acp")
    acp.PROTOCOL_VERSION = "1.0"
    acp.RequestError = RequestError
    acp.update_agent_message_text = lambda text: _ctx().seam("update_agent_message_text", {"text": text})
    acp.update_tool_call = lambda *a, **k: _ctx().seam("update_tool_call", {"p": list(a), "k": k})
    acp.tool_content = lambda *a, **k: SimpleNamespace(**({"p": list(a), "k": k}))
    acp.text_block = lambda *a, **k: SimpleNamespace(**({"p": list(a), "k": k}))
    NS["acp"] = acp
    schema = types.ModuleType("acp.schema")
    for _c in (
        "InitializeResponse", "Implementation", "AgentCapabilities",
        "PromptCapabilities", "SessionCapabilities", "SessionForkCapabilities",
        "SessionListCapabilities", "SessionResumeCapabilities", "NewSessionResponse",
        "LoadSessionResponse", "ResumeSessionResponse", "PromptResponse",
        "ForkSessionResponse", "SetSessionModelResponse", "SetSessionModeResponse",
        "SetSessionConfigOptionResponse", "ListSessionsResponse", "SessionInfo",
        "CurrentModeUpdate", "ConfigOptionUpdate", "AllowedOutcome", "PermissionOption",
    ):
        setattr(schema, _c, (lambda **kw: SimpleNamespace(**kw)))
    schema.__path__ = []
    NS["acp.schema"] = schema
    acp.schema = schema
    # `from acp.schema import X` uses sys.modules["acp.schema"] (import stripped by
    # Syncify, so bare names must be in NS directly)
    for _c in (
        "InitializeResponse", "Implementation", "AgentCapabilities",
        "PromptCapabilities", "SessionCapabilities", "SessionForkCapabilities",
        "SessionListCapabilities", "SessionResumeCapabilities", "NewSessionResponse",
        "LoadSessionResponse", "ResumeSessionResponse", "PromptResponse",
        "ForkSessionResponse", "SetSessionModelResponse", "SetSessionModeResponse",
        "SetSessionConfigOptionResponse", "ListSessionsResponse", "SessionInfo",
        "CurrentModeUpdate", "ConfigOptionUpdate", "AllowedOutcome", "PermissionOption",
    ):
        NS[_c] = (lambda **kw: SimpleNamespace(**kw))

    # asyncio stub
    am = types.ModuleType("asyncio")
    am.CancelledError = _asio.CancelledError

    def create_task(coro):
        _ctx().seam("create_task", {"target": norm(coro)})
        return TaskDouble()

    am.create_task = create_task
    am.sleep = lambda t: _ctx().seam("sleep", {"t": t})
    am.to_thread = lambda fn, *a: fn(*a)
    am.gather = lambda *a, **k: _ctx().seam("asyncio.gather", {"p": [norm(x) for x in a], "k": k})
    am.Lock = lambda: NoopCM()
    am.Condition = lambda: NoopCM()
    am.Future = FutureDouble

    class _Loop:
        def create_task(self, coro):
            return create_task(coro)

    am.get_running_loop = lambda: _Loop()
    NS["asyncio"] = am

    # PreparedMutations ctor
    NS["PreparedMutations"] = lambda: MutationDouble(_ctx())

    NS["EditProposal"] = type("EditProposal", (SimpleNamespace,), {})
    NS["AllowedOutcome"] = type("AllowedOutcome", (SimpleNamespace,), {})
    NS["PermissionOption"] = type("PermissionOption", (SimpleNamespace,), {})

    # session_modes returns an object with .current_mode_id (live reads it)
    def _session_modes(info):
        r = _ctx().seam("session_modes", {"p": [info], "k": {}})
        if isinstance(r, dict):
            return SimpleNamespace(**r)
        return r
    NS["session_modes"] = _session_modes
    NS["__version__"] = "1.0"


class NoopCM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class FutureDouble:
    def __init__(self, done_value):
        self._done = bool(done_value)

    def done(self):
        return self._done


class TaskDouble:
    def add_done_callback(self, cb):
        return None

    def cancel(self):
        return None

    def cancelled(self):
        return False

    def exception(self):
        return None

    def result(self):
        return True


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


class MutationDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def apply(self, client, session_id, operation, payload, **kw):
        return self._ctx.seam("mutations.apply", {
            "client": norm(client), "session_id": session_id,
            "operation": operation, "payload": payload, **kw})

    def acknowledge(self, session_id, operation, payload):
        return self._ctx.seam("mutations.acknowledge", {
            "session_id": session_id, "operation": operation, "payload": payload})

    def retire(self, session_id):
        return self._ctx.seam("mutations.retire", {"session_id": session_id})


class GatewayDouble:
    def __init__(self, ctx):
        self._ctx = ctx
        self.events = EventsDouble(ctx)

    def rpc(self, method, **kw):
        return self._ctx.seam("rpc", {"method": method, **kw})

    @property
    def reader(self):
        raw = self._ctx.seam("gateway.reader", {})
        if isinstance(raw, dict) and raw.get("is_future"):
            return FutureDouble(raw.get("done"))
        return raw


class EventsDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def get(self):
        return self._ctx.seam("gateway.events.get", {})


class InnerConnDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def add_observer(self, cb):
        return self._ctx.seam("add_observer", {})


class ConnDouble:
    def __init__(self, ctx):
        self._ctx = ctx
        self._conn = InnerConnDouble(ctx)

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


class ConnectionDouble:
    def __init__(self, ctx):
        self._ctx = ctx
        self._conn = InnerConnDouble(ctx)

    def __aenter__(self):
        return GatewayDouble(self._ctx)

    def __aexit__(self, *a):
        return self._ctx.seam("connection.__aexit__", {"p": list(a)})


def load_ns():
    _install_stubs()
    text = SRC.read_text(encoding="utf-8")
    tree = ast.parse(text)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module([node], []), str(SRC), "exec", flags=ANN), NS)
            except Exception:
                pass
    for cls in tree.body:
        if isinstance(cls, ast.ClassDef) and cls.name == "GatewayACPAgent":
            for node in cls.body:
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in FUNCS:
                    _compile_fn(node, NS)
    missing = [f for f in FUNCS if f not in NS]
    if missing:
        print("FATAL oracle_acp_gateway_server_flow2: missing %s" % missing)
        sys.exit(2)
    return NS


# ---------------- self double ----------------
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


DEFAULTS = {
    "_conn": None, "_gateway": None, "_connection": None, "_home": "/h",
    "_snapshots": {}, "_event_task": None, "_terminals": {}, "_streamed": {},
    "_segments": {}, "_failure": None, "_link": 0, "_lost_failure": None,
    "_editor_mcp": {}, "_permissions": {}, "_admissions": {}, "_submitting": set(),
    "_unacked": {}, "_settling": set(), "_receipted": {}, "_cut": {},
    "_inflight": {}, "_lost": {}, "_resumes": {}, "_adopted": {},
    "_pending_cancels": set(), "_tool_args": {}, "_message_ids": {},
    "_aliases": {}, "_form_elicitation": False, "_mutations": None,
    "_titles": {}, "_modes": {}, "_configs": {}, "_usage": {}, "_background": set(),
}


def setup_self(state, ctx):
    d = dict(state)
    for k, v in DEFAULTS.items():
        if k not in d:
            d[k] = copy.deepcopy(v)
    for sk in ("_submitting", "_settling", "_background", "_pending_cancels"):
        if sk in d:
            d[sk] = set(d[sk] or [])
    # JSON object keys are strings; the live key is a tuple -> "|"-joined parts.
    if isinstance(d.get("_permissions"), dict):
        d["_permissions"] = {tuple(str(k).split("|")): v for k, v in d["_permissions"].items()}
    slf = SelfDouble(d)
    # scalar collaborators -> doubles bound as instance attrs (never in echo)
    object.__setattr__(slf, "_changed", ChangedDouble(ctx))
    object.__setattr__(slf, "_connect_lock", NoopCM())
    object.__setattr__(slf, "_mutations", d.get("_mutations") or MutationDouble(ctx))
    conn = ConnDouble(ctx) if d.get("_conn") else None
    object.__setattr__(slf, "_conn", conn)
    gw = GatewayDouble(ctx) if d.get("_gateway") else None
    object.__setattr__(slf, "_gateway", gw)

    def client_double(*a, **k):
        ctx.seam("client", {"p": list(a), "k": k})
        return GatewayDouble(ctx)
    object.__setattr__(slf, "_client", client_double)

    def connect_double():
        ctx.seam("connect_gateway", {})
        return ConnectionDouble(ctx)
    NS["connect_gateway"] = connect_double

    # every other self-method collaborator -> generic recorder
    for m in SELF_METHODS:
        if m in ("_client",):
            continue
        object.__setattr__(slf, m, R(m))
    return slf, d


def invoke(func, argv, kw, slf, ctx):
    # A `client` argument travels as a stable "GW" marker; materialize a double
    # so the live body's `client.rpc(...)` fires the rpc seam.
    if func in ("_move_workspace", "_rebind_editor_mcp") and argv:
        if argv[0] is None or argv[0] == "GW" or isinstance(argv[0], str):
            argv = [GatewayDouble(ctx)] + list(argv[1:])
    return NS[func](slf, *argv, **kw)


def drive(func, argv, kw, seams):
    ctx = Ctx(seams)
    CURRENT["ctx"] = ctx
    try:
        state = copy.deepcopy(argv[0]) if argv and isinstance(argv[0], dict) else {}
        slf, d = setup_self(state, ctx)
        if func == "_teardown":
            if d.get("_event_task"):
                object.__setattr__(slf, "_event_task", TaskDouble())
            if d.get("_connection"):
                object.__setattr__(slf, "_connection", ConnectionDouble(ctx))
        r = invoke(func, argv[1:] if argv else [], kw, slf, ctx)
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
    print("ORACLE acp_gateway_server_flow2: %d cases, %d mismatches" % (len(cases), mismatches))
    return 1 if mismatches else 0


def selftest():
    fx = _HERE.parent / "fixtures" / "acp_gateway_server_flow2.jsonl"
    if not fx.is_file():
        print("SELFTEST: no fixture file")
        return 2
    load_ns()
    bad = []
    names = set(FUNCS)
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
            for n in names:
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
