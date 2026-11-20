#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_session_control.py FLOW port
(src/gateway/tui_methods_audit_flow.c -- tui_methods_audit_flow).

MODULE RESOLUTION: the task named tui_gateway/methods_audit.py, which does NOT
exist in the quarry. The real defining module of the unclaimed TUI audit/read
surface is tui_gateway/methods_session_control.py (session.control.read is the
allowlisted audit view of persisted goal/loop/heartbeat state). PoP credit in
the C port names that module.

Drives the LIVE methods_session_control bodies with SCRIPTED seams: every
server global (_err / _ok / _emit / _sess_nowait / _session_profile_runtime_scope
/ _methods / logger), every in-function import callable (hermes_cli.goals /
loops / heartbeat), threading.Lock and time.time are recording stubs fed the
same scripts the C harness feeds its universal seam. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
return ("ret") and the mutated state mirror ("state": the session dict).

Live defs are AST-extracted from methods_session_control.py (module
Assign/AnnAssign constants seeded, plain-Name targets only, the registry
machinery skipped; UNDECORATED FunctionDefs exec'd with the future-annotations
flag into a registered sys.modules module -- py3.14). Decorated `def _`
handlers are extracted BARE (decorators stripped) under the _h_* names.
json.dumps / hashlib.sha256 run REAL (the revision digest reference). The
heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_audit_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site; the session.control
  action try catches ONLY (RuntimeError, ValueError, IndexError).
- State doubles (goal/loop/heartbeat/contract/gates/manager) are
  SimpleNamespaces; canon projects them back to dicts (methods stripped).
  Heartbeat pause/resume scripts are null or NON-EMPTY dicts ({} is outside
  the live domain: a present HeartbeatState is always truthy).
- Locks: threading.Lock() scripts {"__lock__": true}; LockDouble projects back
  to that marker. The session dict carries the lock (state mirror).
- sess_nowait scripts [session, err]; element 0 is mirrored into state.session
  at seam time on both sides (even when the unpack then fails).
- Manager markers {"state": {"subgoals": [...]}}; add_subgoal APPENDS its
  scripted return to state.subgoals (len is read after); remove/clear do not
  model the post-mutation list (never read).
- Tuples project to lists; sets to sorted lists; integral floats to ints.
  Fixture numbers are INTS (the revision digest is float-repr sensitive).
  ASCII corpus only (strip ws set includes \\x1c-\\x1f). str() of containers is
  outside the fixture domain. type(index) is int rejects bool.
- Seam args drop None-valued keys on both sides (absent-vs-null parity).
"""
import ast
import asyncio
import copy
import hashlib
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
SRC = os.path.join(Q, "tui_gateway", "methods_session_control.py")

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


HANDLERS = {
    "session.control.read": "_h_control_read",
    "session.control": "_h_control",
}

PORTED = [
    "_safe_goal_snapshot", "_extract_wait_barrier", "_safe_loop_snapshot",
    "_safe_heartbeat_snapshot", "_snapshot_control", "_snapshot_revision",
    "_snapshot_updated_at", "_load_goal_state", "_load_loop_state",
    "_load_heartbeat_state", "_goal_blocks_loop_tick", "_session_publish_lock",
    "_publish_session_control_snapshot", "_validate_action_args",
    "_dispatch_command", "_execute_manager_action", "_execute_subgoal_action",
    "_execute_heartbeat_action", "_manager_error_message", "_dispatch_envelope",
    "register",
] + list(HANDLERS.values())


# -- methods_session_control.py namespace (live defs + seeded constants) --
def extract():
    mod = new_module("tma_oracle_methods_session_control")
    ns = mod.__dict__
    import logging
    import threading
    import time
    ns.update({"hashlib": hashlib, "json": json, "threading": threading,
               "time": time, "sys": sys, "os": os, "types": types,
               "copy": copy, "Path": Path})
    ns["logging"] = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            debug=lambda *a, **k: None, exception=lambda *a, **k: None,
            info=lambda *a, **k: None, warning=lambda *a, **k: None))
    ns["HandlerRegistry"] = lambda: types.SimpleNamespace(
        method=lambda name: (lambda fn: fn), profile_scoped=lambda fn: fn)
    ns["bind_module"] = lambda *a, **k: None
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    skip = {"_registry", "method", "_profile_scoped", "logger"}
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
                print(f"FATAL oracle_tui_methods_audit_flow: unknown handler {rpc}")
                sys.exit(2)
            name = HANDLERS[rpc]
            text = "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])
            text = text.replace("def _(", f"def {name}(", 1)
            exec(compile(text, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
    required = PORTED + ["_ACTION_COMMAND_MAP", "_MANAGER_ACTIONS", "_VALID_ACTIONS",
                         "_PUBLISH_LOCK_GUARD"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_audit_flow: methods_session_control missing: {missing}")
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
    "CancelledError": asyncio.CancelledError, "NotImplementedError": NotImplementedError,
    "UnicodeError": UnicodeError, "LookupError": LookupError, "ArithmeticError": ArithmeticError,
    "ZeroDivisionError": ZeroDivisionError, "StopIteration": StopIteration,
    "AssertionError": AssertionError, "MemoryError": MemoryError, "EOFError": EOFError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        # str(KeyError(msg)) is repr(msg); the script contract is
        # message == str(exc), so materialize a KeyError whose str IS msg
        cls = type("KeyError", (KeyError,),
                   {"__str__": lambda self: self.args[0] if self.args else ""})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- doubles --
class LockDouble:
    """threading.Lock() double: silent CM; canon projects to the marker."""
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class SilentCM:
    """Silent context-manager double carrying its script marker."""
    def __init__(self, marker):
        self.__cm_marker__ = marker

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def is_lock_marker(raw):
    return isinstance(raw, dict) and "__lock__" in raw


def lock_build(raw):
    return LockDouble() if is_lock_marker(raw) else raw


def cm_build(raw):
    return SilentCM(raw) if isinstance(raw, dict) else raw


def session_build(raw):
    if not isinstance(raw, dict):
        return raw
    out = dict(raw)
    if is_lock_marker(out.get("control_publish_lock")):
        out["control_publish_lock"] = LockDouble()
    return out


def goal_state_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    ns = types.SimpleNamespace(**raw)
    if isinstance(raw.get("contract"), dict):
        cns = types.SimpleNamespace(**raw["contract"])
        cns.to_dict = types.MethodType(rec(ctx, "contract_to_dict", lambda self: {}), cns)
        ns.contract = cns
    if isinstance(raw.get("gates"), list):
        ns.gates = [types.SimpleNamespace(**g) if isinstance(g, dict) else g
                    for g in raw["gates"]]
    return ns


def plain_state_build(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def goal_manager_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    # SimpleNamespace double: missing-attr errors name types.SimpleNamespace
    # (the C port reads the marker dict with the same attribute semantics)
    ns = types.SimpleNamespace()
    if isinstance(raw.get("state"), dict):
        ns.state = types.SimpleNamespace(**raw["state"])
    elif "state" in raw:
        ns.state = raw["state"]
    # absent "state": no attribute -> live AttributeError 'state'

    def add(self, text):
        r = ctx.call("manager_add_subgoal", {"text": text})
        self.state.subgoals.append(r)  # the live append (len read after)
        return r

    def remove(self, index):
        # the post-remove list is never read: the pop is not modeled
        return ctx.call("manager_remove_subgoal", {"index": index})

    def clear(self):
        return ctx.call("manager_clear_subgoals", {})

    ns.add_subgoal = types.MethodType(add, ns)
    ns.remove_subgoal = types.MethodType(remove, ns)
    ns.clear_subgoals = types.MethodType(clear, ns)
    return ns


def hb_manager_build(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    ns = types.SimpleNamespace()
    ns.pause = types.MethodType(
        lambda self: plain_state_build(ctx.call("manager_pause", {})), ns)
    ns.resume = types.MethodType(
        lambda self: plain_state_build(ctx.call("manager_resume", {})), ns)
    ns.clear = types.MethodType(lambda self: ctx.call("manager_clear", {}), ns)
    return ns


def methods_get_build(raw, ctx):
    if isinstance(raw, dict):
        return rec(ctx, "dispatch_handler",
                   lambda rid, params: {"rid": rid, "params": params})
    return raw  # null -> None; non-dict -> live TypeError at the call site


# -- canonical projection (compare parsed JSON, never raw strings) --
def canon(v):
    def conv(x):
        if isinstance(x, BaseException):
            return proj_exc(x)
        if isinstance(x, LockDouble):
            return {"__lock__": True}
        if isinstance(x, SilentCM):
            return conv(x.__cm_marker__)
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()
                    if not callable(val) and not isinstance(val, types.MethodType)}
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


# -- per-case script backend (mirrors the C tma_call exactly) --
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


# -- in-function import stub modules (registered once, rebound per case) --
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


GOALS_MOD = stub_mod("hermes_cli.goals")
LOOPS_MOD = stub_mod("hermes_cli.loops")
HB_MOD = stub_mod("hermes_cli.heartbeat")

# -- real defs restored before every case (seam slots are per-case) --
REAL = {nm: NS[nm] for nm in PORTED}


def install_stubs(ctx, holder):
    NS["_err"] = rec(ctx, "err",
                     lambda rid, code, msg, data=None:
                     {"rid": rid, "code": code, "message": msg, "data": data})
    NS["_ok"] = rec(ctx, "ok", lambda rid, result: {"rid": rid, "result": result})
    NS["_emit"] = rec(ctx, "emit",
                      lambda event, sid, payload:
                      {"event": event, "sid": sid, "payload": payload})

    def sess_pair_build(raw):
        if isinstance(raw, list) and len(raw) >= 1:
            holder["session"] = session_build(raw[0])
            return [holder["session"]] + list(raw[1:])
        return raw

    NS["_sess_nowait"] = rec(ctx, "sess_nowait",
                             lambda params, rid: {"params": params, "rid": rid},
                             build=sess_pair_build)
    NS["_session_profile_runtime_scope"] = rec(
        ctx, "session_profile_runtime_scope", lambda session: {"session": session},
        build=cm_build)
    NS["_methods"] = types.SimpleNamespace(
        get=rec(ctx, "methods_get", lambda name: {"name": name},
                build=lambda raw: methods_get_build(raw, ctx)))
    NS["logger"] = types.SimpleNamespace(
        debug=rec(ctx, "logger_debug", lambda fmt, *a, **k: {"msg": fmt % a}))
    NS["threading"] = types.SimpleNamespace(
        Lock=rec(ctx, "new_lock", lambda: {}, build=lock_build))
    NS["time"] = types.SimpleNamespace(time=rec(ctx, "time_time", lambda: {}))
    NS["bind_module"] = rec(ctx, "bind_module",
                            lambda globals_, server, skip: {"skip": list(skip)})
    GOALS_MOD.load_goal = rec(ctx, "load_goal",
                              lambda session_key: {"session_key": session_key},
                              build=lambda raw: goal_state_build(raw, ctx))
    GOALS_MOD.GoalManager = rec(ctx, "goal_manager_new",
                                lambda session_id: {"session_id": session_id},
                                build=lambda raw: goal_manager_build(raw, ctx))
    LOOPS_MOD.load_loop = rec(ctx, "load_loop",
                              lambda session_key: {"session_key": session_key},
                              build=plain_state_build)
    LOOPS_MOD.goal_blocks_loop_tick = rec(
        ctx, "goal_blocks_loop_tick", lambda session_key: {"session_key": session_key})
    HB_MOD.load_heartbeat = rec(ctx, "load_heartbeat",
                                lambda session_key: {"session_key": session_key},
                                build=plain_state_build)
    HB_MOD.HeartbeatManager = rec(ctx, "heartbeat_manager_new",
                                  lambda session_id: {"session_id": session_id},
                                  build=lambda raw: hb_manager_build(raw, ctx))
    HB_MOD.format_interval = rec(ctx, "format_interval",
                                 lambda seconds: {"interval_seconds": seconds})


def drive(func, inp):
    args = copy.deepcopy(inp.get("args") or {})
    ctx = Ctx(inp.get("io"))
    state_in = copy.deepcopy(inp.get("state") or {})
    holder = {}
    for nm, real in REAL.items():
        NS[nm] = real
    install_stubs(ctx, holder)
    # direct-case session materialization (the state mirror IS the session)
    if func in ("_session_publish_lock", "_publish_session_control_snapshot"):
        if "session" in state_in:
            state_in["session"] = session_build(state_in["session"])
    try:
        if func == "_safe_goal_snapshot":
            ret = NS[func](goal_state_build(args.get("state"), ctx))
        elif func == "_extract_wait_barrier":
            ret = NS[func](goal_state_build(args.get("state"), ctx))
        elif func == "_safe_loop_snapshot":
            ret = NS[func](plain_state_build(args.get("state")),
                           deferred_by_goal=args.get("deferred_by_goal", False))
        elif func == "_safe_heartbeat_snapshot":
            ret = NS[func](plain_state_build(args.get("state")))
        elif func in ("_snapshot_control", "_load_goal_state", "_load_loop_state",
                      "_load_heartbeat_state", "_goal_blocks_loop_tick"):
            ret = NS[func](args.get("session_key", ""))
        elif func == "_snapshot_revision":
            ret = NS[func](args.get("goal"), args.get("loop"), args.get("heartbeat"))
        elif func == "_snapshot_updated_at":
            ret = NS[func](plain_state_build(args.get("goal_state")),
                           plain_state_build(args.get("loop_state")),
                           plain_state_build(args.get("heartbeat_state")))
        elif func == "_session_publish_lock":
            ret = NS[func](state_in.get("session"))
        elif func == "_publish_session_control_snapshot":
            ret = NS[func](args.get("sid"), state_in.get("session"),
                           only_if_present=args.get("only_if_present", False),
                           only_if_changed=args.get("only_if_changed", False))
        elif func in ("_h_control_read", "_h_control"):
            ret = NS[func](args.get("rid"), args.get("params"))
        elif func == "_validate_action_args":
            ret = NS[func](args.get("rid"), args.get("action", ""), args.get("args"))
        elif func == "_dispatch_command":
            ret = NS[func](args.get("rid"), session_id=args.get("session_id"),
                           name=args.get("name", ""), arg=args.get("arg", ""))
        elif func in ("_execute_manager_action", "_execute_subgoal_action"):
            ret = NS[func](args.get("session_key", ""), args.get("action", ""),
                           args.get("args"))
        elif func == "_execute_heartbeat_action":
            ret = NS[func](args.get("session_key", ""), args.get("action", ""))
        elif func == "_manager_error_message":
            exc = args.get("exc")
            if isinstance(exc, dict) and "__raise__" in exc:
                exc = make_exc({"type": exc["__raise__"], "message": exc.get("message", "")})
            ret = NS[func](args.get("action", ""), exc)
        elif func == "_dispatch_envelope":
            ret = NS[func](args.get("response"))
        elif func == "register":
            ret = NS[func](object())
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state_out = dict(state_in)
    if "session" in holder:
        state_out["session"] = holder["session"]
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# -- three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) --
SEAM_NAMES = [
    "err", "ok", "emit", "sess_nowait", "session_profile_runtime_scope",
    "methods_get", "dispatch_handler", "logger_debug", "load_goal", "load_loop",
    "load_heartbeat", "goal_blocks_loop_tick", "contract_to_dict", "time_time",
    "new_lock", "goal_manager_new", "manager_add_subgoal", "manager_remove_subgoal",
    "manager_clear_subgoals", "heartbeat_manager_new", "manager_pause",
    "manager_resume", "manager_clear", "format_interval", "bind_module",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_audit_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_audit_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tma_call\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_audit_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*\n\s*ctx,\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_audit_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_audit_flow.jsonl"
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
            print(f"FATAL oracle_tui_methods_audit_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_tui_methods_audit_flow: fixture funcs outside PORTED: {sorted(badf)}")
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
    print(f"ORACLE tui_methods_audit_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_audit_flow.jsonl"
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
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_dispatch_envelope",
                 {"args": {"response": {"result": {"type": "exec", "output": "ok"}}}})
    assert spot["ret"] == {"type": "exec", "output": "ok", "notice": None,
                           "message": None, "display": None}, spot
    spot = drive("_manager_error_message",
                 {"args": {"action": "subgoal.add",
                           "exc": {"__raise__": "ValueError", "message": "boom"}}})
    assert spot["ret"] == "/subgoal: boom", spot
    spot = drive("_validate_action_args",
                 {"args": {"rid": "r1", "action": "subgoal.remove", "args": {"index": 3}}})
    assert spot["ret"] == [{"index": 3}, None], spot
    spot = drive("_snapshot_revision", {"args": {}})
    assert spot["ret"] == "", spot
    canon_str = '{"goal":{"a":1},"heartbeat":null,"loop":null}'
    want_rev = hashlib.sha256(canon_str.encode("utf-8")).hexdigest()
    spot = drive("_snapshot_revision", {"args": {"goal": {"a": 1}}})
    assert spot["ret"] == want_rev, spot
    spot = drive("_safe_heartbeat_snapshot", {"args": {"state": None}})
    assert spot["ret"] is None, spot
    spot = drive("_extract_wait_barrier",
                 {"args": {"state": {"waiting_reason": "r", "waiting_until": 100,
                                     "waiting_on_session": None, "waiting_on_pid": None}},
                  "io": {"time_time": 50}})
    assert spot["ret"] == {"type": "until", "until_at": 100, "reason": "r"}, spot
    spot = drive("_snapshot_control",
                 {"args": {"session_key": "sk"},
                  "io": {"load_goal": None, "load_loop": None, "load_heartbeat": None}})
    assert spot["ret"] == {"goal": None, "loop": None, "heartbeat": None,
                           "revision": "", "updated_at": 0}, spot
    assert [e["name"] for e in spot["trace"]] == ["load_goal", "load_loop", "load_heartbeat"], spot
    spot = drive("_h_control",
                 {"args": {"rid": "r1", "params": {"action": "bogus"}},
                  "io": {"err": {"jsonrpc": "2.0", "id": "r1",
                                 "error": {"code": 4004, "message": "unknown action: bogus"}}}})
    assert spot["ret"]["error"]["code"] == 4004, spot
    spot = drive("_load_goal_state", {"args": {"session_key": "sk"}, "io": {}})
    assert spot["ret"] == {"__raise__": "RuntimeError", "message": "no script for load_goal"}, spot
    assert len(spot["trace"]) == 1, spot
    spot = drive("_load_goal_state",
                 {"args": {"session_key": "sk"},
                  "io": {"load_goal": {"__script__": True,
                                       "raise": {"type": "SystemExit", "message": "bye"}}}})
    assert spot["ret"] == {"__raise__": "SystemExit", "message": "bye"}, spot
    thin = [f for f in PORTED if counts.get(f, 0) < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_audit_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
