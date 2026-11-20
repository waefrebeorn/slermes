#!/usr/bin/env python3
"""Oracle for tui_hosted_room_service_flow — 20 UNCLAIMED FLOW defs of
tui_gateway/hosted_room_service.py (LIVE authority, py3.14 quarry venv).

Port covered (harness name -> live def), all with PoP annotations in
src/gateway/tui_hosted_room_service_flow.c:
  task_member_id, _approvable_attempts, attempt_action, _member_of,
  _authority, _grant_revoke_is_terminal, _hosted_room_turn_timeout_seconds,
  _hook, disband_fence, route_publication_lock, _member_activity,
  _attempt_actions, _stop_pending_refusal, _policy_snapshot, profile_homes,
  _route_statuses, _events, _append_room_status, publish_terminal,
  peer_admission_receipt

Drives the LIVE defs with SCRIPTED I/O: each def is AST-extracted from the
quarry (no heavy import of the module's gateway/web dependencies), module
constants are seeded, and every effect is replaced by a recording stub
returning the SAME scripted results the C harness feeds its universal seam.
Compares the ordered DECISION SEQUENCE (seam-call trace) plus the canonical
return value (raise envelopes included). Scripted {"__raise__": cls} raises a
REAL exception (full builtin hierarchy); drive() catches BaseException so
KeyboardInterrupt/SystemExit/GeneratorExit pass through live `except` blocks
exactly as the C full-hierarchy model propagates them. A MISSING seam key
fail-closes (RuntimeError) on both sides; a scripted null is a valid None.

Input: stdin JSON lines from t_port_tui_hosted_room_service_flow:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE tui_hosted_room_service_flow: N cases, M mismatches.
Fail-closed (exit 2) when extraction misses a def or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import builtins
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
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "tui_hosted_room_service_flow_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tui_gateway", "hosted_room_service.py")

# harness name -> live python def name
PY = {
    "task_member_id": "task_member_id",
    "approvable_attempts": "_approvable_attempts",
    "attempt_action": "attempt_action",
    "member_of": "_member_of",
    "authority": "_authority",
    "grant_revoke_is_terminal": "_grant_revoke_is_terminal",
    "hosted_room_turn_timeout_seconds": "_hosted_room_turn_timeout_seconds",
    "hook": "_hook",
    "disband_fence": "disband_fence",
    "route_publication_lock": "route_publication_lock",
    "member_activity": "_member_activity",
    "attempt_actions": "_attempt_actions",
    "stop_pending_refusal": "_stop_pending_refusal",
    "policy_snapshot": "_policy_snapshot",
    "profile_homes": "profile_homes",
    "route_statuses": "_route_statuses",
    "events": "_events",
    "append_room_status": "_append_room_status",
    "publish_terminal": "publish_terminal",
    "peer_admission_receipt": "peer_admission_receipt",
}
WANTED = set(PY.values())
# Ported siblings that other ported defs call: must be live-extracted and bound
# into the namespace so composed cases exercise the reference logic.
SIBLINGS = {"task_member_id", "attempt_action", "_member_of", "_authority"}
METHOD_NAMES = {
    "_stop_pending_refusal", "_policy_snapshot", "profile_homes",
    "_route_statuses", "_events", "_append_room_status", "publish_terminal",
    "peer_admission_receipt", "_attempt_actions",
}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}

# Names that may legitimately appear in an AttributeError (used by self-test).
SEAM_NAMES = [
    "os.getenv", "discussion.turn_round", "task_progress", "self.local_profiles",
    "policy_checkpoint.snapshot", "hosted_rooms.read_events",
    "hosted_rooms.append_event", "self._member_is_peer", "forget_task_progress",
    "self._prepare_tracked", "self.runtime.wakeup", "self._room",
    "hosted_rooms.remote_run_receipt", "disbanding",
]
CONSTANTS = [
    "_APPROVABLE_STATUSES", "_RETRYABLE_STATUSES",
    "_HOSTED_ROOM_TERMINAL_GRACE_SECONDS",
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
        self.__ws5_callable__ = marker

    def __call__(self, *a, **k):
        if self.__ws5_callable__ == "disbanding":
            return seam("disbanding", {"room_id": a[0] if a else k.get("room_id")})
        return seam(self.__ws5_callable__, dict(k) if not a else {"args": list(a), **k})


def obj_double(d):
    """Attribute double from a JSON object: nested dicts become SimpleNamespace
    and {"__callable__": m} entries become callable doubles."""
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


def task_double(d):
    """A task is a dict (subscript reads); only its 'identity' is an attribute
    double."""
    if not isinstance(d, dict):
        return d
    t = dict(d)
    if isinstance(t.get("identity"), dict):
        t["identity"] = obj_double(t["identity"])
    return t


def norm(o):
    if o is None or isinstance(o, (bool, int, str, float)):
        return o
    if isinstance(o, PurePosixPath):
        return str(o)
    if isinstance(o, contextlib.nullcontext):
        return {"__nullcontext__": True}
    if isinstance(o, CallableDouble):
        return {"__callable__": o.__ws5_callable__}
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


def _seed_constant(ns, name, src_tree, src):
    for node in src_tree.body:
        tgt = None
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            tgt = node.targets[0].id
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            tgt = node.target.id
        if tgt == name:
            exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec"), ns)
            return True
    return False


def extract():
    import __future__

    flags = __future__.annotations.compiler_flag
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("tui_hosted_room_service_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({"__name__": "tui_hosted_room_service_flow_extracted",
               "json": json, "os": None, "contextlib": contextlib,
               "types": types})

    tree = ast.parse(src)
    for name in CONSTANTS:
        if not _seed_constant(ns, name, tree, SRC):
            print("FATAL missing constant: %s" % name)
            sys.exit(2)

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
        n = copy_node(node)
        n.decorator_list = []
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec", flags), ns)

    for name in sorted(need):
        if name not in ns:
            print("FATAL extraction missed def: %s" % name)
            sys.exit(2)
    global NS
    NS = ns
    return ns


def copy_node(node):
    import copy
    return copy.copy(node)


def install_stubs(ns):
    """Namespace collaborators -> recorders (same scripts as the C seam)."""
    ns["os"] = types.SimpleNamespace(
        getenv=lambda key, default=None: seam("os.getenv", {"key": key, "default": default}))

    ns["discussion"] = types.SimpleNamespace(
        turn_round=lambda turn_id: seam("discussion.turn_round", {"turn_id": turn_id}))

    def _task_progress(room_id, task_id, execution_generation):
        return seam("task_progress", {"room_id": room_id, "task_id": task_id,
                                      "execution_generation": execution_generation})
    ns["task_progress"] = _task_progress

    def _forget(room_id, task_id, execution_generation):
        return seam("forget_task_progress", {"room_id": room_id, "task_id": task_id,
                                             "execution_generation": execution_generation})
    ns["forget_task_progress"] = _forget

    def _read_events(store, placement=None, room_id=None, since_seq=0, limit=500, **kw):
        return seam("hosted_rooms.read_events",
                    {"room_id": room_id, "since_seq": since_seq, "limit": limit})

    def _append_event(store=None, placement=None, **kw):
        keep = ("room_id", "event_id", "kind", "actor", "payload",
                "authority_gateway_id", "authority_epoch")
        return seam("hosted_rooms.append_event", {k: kw.get(k) for k in keep})

    def _remote_receipt(store=None, placement=None, record=None):
        return seam("hosted_rooms.remote_run_receipt", {"record": record})

    ns["hosted_rooms"] = types.SimpleNamespace(
        MAX_LOG_LIMIT=500, read_events=_read_events,
        append_event=_append_event, remote_run_receipt=_remote_receipt)


def self_double(inp):
    raw = inp.get("self") or {}
    slf = types.SimpleNamespace()
    slf.root = PurePosixPath(raw.get("root", ""))
    slf.store = None                       # passed through to seamed store calls
    slf.placement = "shared"               # likewise
    slf._state_lock = contextlib.nullcontext()  # leaf lock, inert in the seam model
    slf._policy_lock = contextlib.nullcontext()
    states = raw.get("_peer_route_status") or []
    slf._peer_route_status = {(d.get("room_id"), d.get("member_id")): d.get("status")
                              for d in states}
    routes = raw.get("peer_routes") or []
    slf.peer_routes = {(d.get("room_id"), d.get("member_id")): obj_double(d) for d in routes}
    slf._pending_actions = {}
    slf.local_profiles = lambda: seam("self.local_profiles", {})

    def _member_is_peer(room_id, member_id):
        return seam("self._member_is_peer", {"room_id": room_id, "member_id": member_id})
    slf._member_is_peer = _member_is_peer
    slf._prepare_tracked = lambda binding: seam("self._prepare_tracked", {"binding": binding})
    slf._room = lambda room_id: seam("self._room", {"room_id": room_id})

    def _snapshot(room_id=None, latest_seq=None):
        return seam("policy_checkpoint.snapshot",
                    {"room_id": room_id, "latest_seq": latest_seq})
    slf.policy_checkpoint = types.SimpleNamespace(snapshot=_snapshot)
    slf.runtime = types.SimpleNamespace(
        wakeup=lambda: seam("self.runtime.wakeup", {}),
        _record_error=lambda msg: None,
    )
    return slf


def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    fn = ns[PY[func]]
    method = PY[func] in METHOD_NAMES
    slf = self_double(inp) if method else None

    if func == "task_member_id":
        return fn(task_double(inp["task"]))
    if func == "approvable_attempts":
        return fn([task_double(t) for t in inp.get("tasks", [])])
    if func == "attempt_action":
        return fn(inp.get("kind", "retry"), task_double(inp["task"]))
    if func == "member_of":
        return fn(task_double(inp["task"]))
    if func == "authority":
        return fn(inp["room"])
    if func == "grant_revoke_is_terminal":
        return fn(obj_double(inp["exc"]))
    if func == "hosted_room_turn_timeout_seconds":
        return fn()
    if func == "hook":
        return fn(obj_double(inp.get("obj") or {}), inp.get("name", ""))
    if func == "disband_fence":
        return fn(obj_double(inp.get("service") or {}), inp.get("room_id", ""))
    if func == "route_publication_lock":
        return fn(obj_double(inp.get("service") or {}))
    if func == "member_activity":
        return fn(task_double(inp["task"]))
    if func == "attempt_actions":
        return fn(slf, inp.get("room_id", ""), [task_double(t) for t in inp.get("tasks_shown", [])])
    if func == "stop_pending_refusal":
        return fn(slf, inp.get("room_id", ""), inp.get("deferred", []))
    if func == "policy_snapshot":
        return fn(slf, inp["room"])
    if func == "profile_homes":
        return fn(slf)
    if func == "route_statuses":
        return fn(slf, inp.get("room_id"))
    if func == "events":
        return fn(slf, inp.get("room_id", ""))
    if func == "append_room_status":
        return fn(slf, inp["room"], obj_double(inp["decision"]))
    if func == "publish_terminal":
        return fn(slf, obj_double(inp["binding"]), task_double(inp["task"]))
    if func == "peer_admission_receipt":
        return fn(slf, inp.get("room_id", ""), task_double(inp["task"]))
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
    fixture = _REPO / "tests" / "fixtures" / "tui_hosted_room_service_flow.jsonl"
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
    names = set(PY.values()) | set(PY) | set(SEAM_NAMES) | {"payload", "identity"}
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
        print("ORACLE tui_hosted_room_service_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE tui_hosted_room_service_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
