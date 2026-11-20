#!/usr/bin/env python3
"""Oracle for tui_hosted_room_service_flow2 — 15 disjoint UNCLAIMED FLOW defs
of tui_gateway/hosted_room_service.py (LIVE authority, py3.14 quarry venv).

Harness name -> live def (all PoP-annotated in
src/gateway/tui_hosted_room_service_flow2.c):
  live_room_services      -> live_room_services
  member_is_peer          -> _member_is_peer
  owned_authority         -> _owned_authority
  route_authority         -> route_authority
  bindings                -> bindings
  list_tasks              -> _list_tasks
  turn_lock               -> _turn_lock
  start                   -> start
  stop                    -> stop
  wakeup                  -> wakeup
  save_link               -> _save_link
  dispatch_approval       -> _dispatch_approval
  room_profiles           -> room_profiles
  make_rpc                -> _make_rpc
  set_route_status        -> _set_route_status

Drives the LIVE defs with SCRIPTED I/O: each def is AST-extracted from the
quarry (no heavy import of gateway/web dependencies), collaborators are seeded
and every effect is replaced by a recording stub returning the SAME scripted
results the C harness feeds its universal seam. Compares the ordered DECISION
SEQUENCE (seam-call trace) + the canonical return value (raise envelopes
included). Scripted {"__raise__": cls} raises a REAL exception (full builtin
hierarchy); drive() catches BaseException so KeyboardInterrupt/SystemExit/
GeneratorExit pass through live `except` blocks exactly as the C full-hierarchy
model propagates them. A MISSING seam key fail-closes (RuntimeError) on both
sides; a scripted null is a valid None.

Input: stdin JSON lines from t_port_tui_hosted_room_service_flow2:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE tui_hosted_room_service_flow2: N cases, M mismatches.
Fail-closed (exit 2) when extraction misses a def or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import builtins
import collections.abc
import contextlib
import json
import os
import sys
import types
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tui_gateway", "hosted_room_service.py")

# harness name -> live python def name
PY = {
    "live_room_services": "live_room_services",
    "member_is_peer": "_member_is_peer",
    "owned_authority": "_owned_authority",
    "route_authority": "route_authority",
    "bindings": "bindings",
    "list_tasks": "_list_tasks",
    "turn_lock": "_turn_lock",
    "start": "start",
    "stop": "stop",
    "wakeup": "wakeup",
    "save_link": "_save_link",
    "dispatch_approval": "_dispatch_approval",
    "room_profiles": "room_profiles",
    "make_rpc": "_make_rpc",
    "set_route_status": "_set_route_status",
}
WANTED = set(PY.values())
# Ported siblings that these defs call: live-extracted into the namespace.
SIBLINGS = {"_authority", "_hook"}
METHOD_NAMES = {
    "_member_is_peer", "_owned_authority", "route_authority", "bindings",
    "_list_tasks", "_turn_lock", "start", "stop", "wakeup", "_save_link",
    "_dispatch_approval", "room_profiles", "_make_rpc", "_set_route_status",
}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}

# EXACT seam-name set the C core calls and the stubs below fire (set equality).
SEAM_NAMES = [
    "live_services.snapshot", "self._room", "local_authority_gateway_id",
    "list_rooms", "driver.list_tasks", "acquire_turn_lock",
    "self.runtime.start", "self.runtime.stop", "self.runtime.wakeup",
    "hosted_room_links.make_stored_link", "hosted_room_links.save_room_link",
    "approve_receipt", "rpc.approve", "self.local_profiles",
    "HostedRoomServerRPC", "mark_room_link_status",
]


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


# Not a CPython builtin (lives in zipimport); safe fallback for the hierarchy.
ZipImportError = getattr(builtins, "ZipImportError",
                         type("ZipImportError", (ImportError,), {}))

# Exceptions this module defines (bound below), registered for scripted raises.
class AuthorityConflictError(Exception):
    pass


class AuthorityRefusal(Exception):
    pass


# FULL CPython 3.14 builtin exception hierarchy (identity matters for catches).
EXC_MAP = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "LookupError": LookupError, "MemoryError": MemoryError,
    "NameError": NameError, "OSError": OSError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "SyntaxError": SyntaxError, "SystemError": SystemError,
    "TypeError": TypeError, "ValueError": ValueError, "Warning": Warning,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError,
    "ModuleNotFoundError": ModuleNotFoundError, "ZipImportError": ZipImportError,
    "IndexError": IndexError, "KeyError": KeyError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "PythonFinalizationError": PythonFinalizationError,
    "IndentationError": IndentationError, "TabError": TabError,
    "_IncompleteInputError": _IncompleteInputError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "JSONDecodeError": json.JSONDecodeError,
    "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
    "UnboundLocalError": UnboundLocalError,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "AuthorityConflictError": AuthorityConflictError, "AuthorityRefusal": AuthorityRefusal,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        if name == "SystemExit":
            raise SystemExit(r.get("code", r.get("msg", "")))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        if cls in (UnicodeDecodeError, UnicodeEncodeError, UnicodeTranslateError):
            e = cls.__new__(cls)
            Exception.__init__(e, r.get("msg", ""))
            raise e
        raise cls(r.get("msg", ""))
    return r


NS = {}


class CallableDouble:
    """A callable attribute double: __call__ fires the universal seam. norm()
    projects it back to {"__callable__": marker}, matching the C port."""
    def __init__(self, marker):
        self.__ws5_marker__ = marker

    def __call__(self, *a, **k):
        if a and not k:
            return seam(self.__ws5_marker__, {"args": list(a)})
        return seam(self.__ws5_marker__, dict(k))


def obj_double(d):
    if not isinstance(d, dict):
        return d
    out = {}
    for k, v in d.items():
        if isinstance(v, dict) and set(v) == {"__callable__"}:
            out[k] = CallableDouble(v["__callable__"])
        elif isinstance(v, dict):
            out[k] = obj_double(v)
        elif isinstance(v, list):
            out[k] = [obj_double(x) if isinstance(x, dict) else x for x in v]
        else:
            out[k] = v
    return types.SimpleNamespace(**out)


def norm(o):
    if o is None or isinstance(o, (bool, int, str, float)):
        return o
    if isinstance(o, PurePosixPath):
        return str(o)
    if isinstance(o, contextlib.nullcontext):
        return {"__nullcontext__": True}
    if isinstance(o, CallableDouble):
        return {"__callable__": o.__ws5_marker__}
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, tuple):
        return [norm(v) for v in o]
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [norm(v) for v in o]
    return str(o)


def extract():
    import __future__
    import copy

    flags = __future__.annotations.compiler_flag
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("tui_hosted_room_service_flow2_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({"__name__": "tui_hosted_room_service_flow2_extracted",
               "contextlib": contextlib, "types": types,
               "Mapping": collections.abc.Mapping})

    tree = ast.parse(src)
    top = {n.name: n for n in tree.body
           if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
    cls = next((n for n in tree.body
                if isinstance(n, ast.ClassDef) and n.name == "HostedRoomService"), None)
    methods = ({n.name: n for n in cls.body if isinstance(n, ast.FunctionDef)}
               if cls is not None else {})

    need = WANTED | SIBLINGS
    for name in sorted(need):
        node = top.get(name) or methods.get(name)
        if node is None:
            print("FATAL missing live def: %s" % name)
            sys.exit(2)
        n = copy.copy(node)
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec", flags), ns)

    for name in sorted(need):
        if name not in ns:
            print("FATAL extraction missed def: %s" % name)
            sys.exit(2)
    global NS
    NS = ns
    return ns


def _install_module(name, attrs):
    parts = name.split(".")
    cur = None
    for i in range(1, len(parts) + 1):
        pname = ".".join(parts[:i])
        m = sys.modules.get(pname)
        if m is None:
            m = types.ModuleType(pname)
            sys.modules[pname] = m
        if cur is not None:
            setattr(cur, parts[i - 1], m)
        cur = m
    for k, v in attrs.items():
        setattr(cur, k, v)


def install_stubs(ns):
    """Namespace collaborators -> recording stubs (same scripts as the C seam)."""
    ns["hosted_rooms"] = types.SimpleNamespace(
        local_authority_gateway_id=lambda: seam("local_authority_gateway_id", {}),
        list_rooms=lambda store, placement=None: seam("list_rooms", {}))
    ns["driver"] = types.SimpleNamespace(
        list_tasks=lambda store, placement=None, room_id=None, status=None:
            seam("driver.list_tasks", {"room_id": room_id, "status": status}))
    ns["hosted_room_links"] = types.SimpleNamespace(
        make_stored_link=lambda **kw: seam("hosted_room_links.make_stored_link", kw),
        save_room_link=lambda store, link, placement=None:
            seam("hosted_room_links.save_room_link", {"stored": link}),
        mark_room_link_status=lambda store, placement=None, room_id=None,
                                     member_id=None, status=None:
            seam("mark_room_link_status", {"room_id": room_id, "member_id": member_id,
                                           "status": status}))
    ns["HostedRoomServerRPC"] = (
        lambda server, db_path, root, placement:
            seam("HostedRoomServerRPC", {"server": server, "db_path": db_path,
                                         "root": str(root), "placement": placement}))
    ns["AuthorityConflictError"] = AuthorityConflictError

    # module-level WeakSet + lock used by live_room_services
    class _WeakSetDouble:
        def __iter__(self):
            return iter(seam("live_services.snapshot", {}) or [])
    ns["_live_services"] = _WeakSetDouble()
    ns["_live_services_lock"] = contextlib.nullcontext()

    # function-local imports
    _install_module("persistence.core.errors", {"AuthorityRefusal": AuthorityRefusal})
    _install_module("tools.bot_relay", {
        "acquire_turn_lock": lambda root, profile:
            seam("acquire_turn_lock", {"root": str(root), "profile": profile})})

    HostedRoomBinding = collections.namedtuple(
        "HostedRoomBinding", "room_id gateway_id authority_epoch")
    ns["HostedRoomBinding"] = HostedRoomBinding


def self_double(inp):
    raw = inp.get("self") or {}
    slf = types.SimpleNamespace()
    slf.root = PurePosixPath(raw.get("root", ""))
    slf._root = slf.root
    slf.db_path = raw.get("db_path", "")
    slf.placement = raw.get("placement", "shared")
    slf.store = None
    slf._policy_lock = contextlib.nullcontext()
    slf._state_lock = contextlib.nullcontext()
    slf._peer_route_status = {
        (d.get("room_id"), d.get("member_id")): d.get("status")
        for d in (raw.get("_peer_route_status") or [])}
    slf._disbanding_rooms = set(raw.get("_disbanding_rooms") or [])
    slf._room = lambda room_id: seam("self._room", {"room_id": room_id})
    slf._owned_authority = lambda room_id: NS["_owned_authority"](slf, room_id)
    slf.local_profiles = lambda: seam("self.local_profiles", {})
    slf.runtime = types.SimpleNamespace(
        start=lambda: seam("self.runtime.start", {}),
        stop=lambda timeout=5.0: seam("self.runtime.stop", {"timeout": timeout}),
        wakeup=lambda: seam("self.runtime.wakeup", {}))
    slf.rpc = types.SimpleNamespace(
        approve=lambda **kw: seam("rpc.approve", kw))
    return slf


def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    fn = ns[PY[func]]
    method = PY[func] in METHOD_NAMES
    slf = self_double(inp) if method else None

    if func == "live_room_services":
        return fn()
    if func == "member_is_peer":
        return fn(slf, inp.get("room_id", ""), inp.get("member_id", ""))
    if func == "owned_authority":
        return fn(slf, inp.get("room_id", ""))
    if func == "route_authority":
        return fn(slf, inp.get("room_id", ""))
    if func == "bindings":
        return fn(slf)
    if func == "list_tasks":
        return list(fn(slf, inp.get("room_id", ""), inp.get("statuses", [])))
    if func == "turn_lock":
        return fn(slf, inp.get("profile", ""))
    if func == "start":
        return fn(slf)
    if func == "stop":
        return fn(slf, timeout=inp.get("timeout", 5.0))
    if func == "wakeup":
        return fn(slf)
    if func == "save_link":
        return fn(slf, **(inp.get("link") or {}))
    if func == "dispatch_approval":
        return fn(slf, obj_double(inp.get("route")), obj_double(inp.get("client")),
                  inp.get("action") or {}, task_id=inp.get("task_id", ""),
                  execution_generation=inp.get("execution_generation", 0),
                  request_id=inp.get("request_id", ""), choice=inp.get("choice", ""))
    if func == "room_profiles":
        # Live signature is room_profiles(self, room: Mapping); the body only
        # returns self.local_profiles(). Pass the room mapping so the call
        # reaches the body instead of raising a binding TypeError.
        return fn(slf, inp.get("room") or {})
    if func == "make_rpc":
        return fn(slf, inp.get("server"))
    if func == "set_route_status":
        return fn(slf, inp.get("room_id", ""), inp.get("member_id", ""), inp.get("status", ""))
    print("FATAL unhandled func: %s" % func)
    sys.exit(2)


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


def self_test(ns):
    import faulthandler

    faulthandler.dump_traceback_later(60, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "tui_hosted_room_service_flow2.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").split("\n") if l.strip()]
    fixture_funcs = {c["func"] for c in cases}
    if fixture_funcs != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(fixture_funcs - set(PY)), sorted(set(PY) - fixture_funcs)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 6}
    if thin:
        print("SELFTEST FATAL <6 cases for: %s" % thin)
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    names = (set(PY.values()) | set(PY) | set(SEAM_NAMES) |
             {"payload", "identity", "self", "room_id", "member_id", "status",
              "statuses", "profile", "route", "client", "action", "task_id",
              "execution_generation", "request_id", "choice", "server", "link",
              "timeout", "members", "member_id", "target", "kind", "grant",
              "session_id", "approve_receipt", "disbanding", "authority_gateway_id",
              "authority_epoch", "root", "db_path", "placement"})
    for c in cases:
        CUR["in"] = c["in"]
        CUR["trace"] = []
        CUR["scripts"] = c["in"].get("seams", {})
        CUR["idx"] = {}
        try:
            drive(ns, c["func"], c["in"])
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec))
                bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec))
                bad += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(extract())
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
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm(ret), "trace": norm(CUR["trace"])}
        except BaseException as ex:
            er = {"__raise__": type(ex).__name__, "msg": str(ex)}
            if isinstance(ex, SystemExit):
                er["code"] = ex.code
            want = {"ret": er, "trace": norm(CUR["trace"])}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE tui_hosted_room_service_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE tui_hosted_room_service_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
