#!/usr/bin/env python3
"""Oracle for the gateway/run.py FLOW cores, fourth wave
(src/gateway/gateway_run_flow4.c).

AST-extracts the 14 LIVE GatewayRunner methods (no heavy gateway import),
seeds the module globals they need (t, threading, SessionState,
_SESSION_DB_UNPINNED, _write_runtime_status_quiet) and replaces every live I/O
dependency with recording stubs driven by the SAME per-name scripts the C
harness feeds its universal seams (in.io). Compares the ordered seam-call
trace plus the return value.

Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
call, last element repeats after exhaustion); any other value is reused;
a scripted null is None. FAIL-CLOSED: a missing or empty script raises
RuntimeError("no script for <name>"). BaseException kinds pass through every
`except Exception` site; the drive wrapper catches BaseException into a
{"__raise__": kind, "msg": str(exc)} ret spec.

Modeling (twin of the port header): runner DATA attributes are in in.state
(set as real attributes on the double so `self.__dict__.get` / `getattr(self,
X, default)` behave); METHOD-presence markers for seamed siblings are in
in.runner (missing -> the live-shaped AttributeError). Seam results are
deep-copied (the C harness json_copy's them) so a flow that mutates a
returned map still leaves the recorded trace intact. `_session_db` sentinel
travels as {"__unpinned__": true}; move_to_end (OrderedDict) and the lock CM
enter/exit are modeled as seams; await/executor doubles project back to
markers. float()/int() coercions accept int/float/bool; integral floats
normalize on both sides.

Input: stdin JSON lines from t_port_gateway_run_flow4.
Summary: ORACLE gateway_run_flow4: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import builtins
import contextlib
import copy
import json
import os
import sys
import textwrap
import types
import __future__

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

import pathlib as _pathlib
_REPO = _pathlib.Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
Path = _pathlib.PurePosixPath

CUR = {"func": None, "in": {}, "trace": [], "idx": {}}

# Seam-name set (must equal the C port's seam table + fixture io keys).
SEAM_NAMES = {
    "t", "_sessions_map", "SessionState", "_peek_session_state",
    "_session_key_for_source", "move_to_end", "lock.enter", "lock.exit",
    "Lock", "make_pool", "shutdown", "_describe_active_work",
    "_active_work_count", "_write_runtime_status_quiet",
    "_open_session_db_for_active_scope",
}

_BUILTIN_EXC = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "Exception": Exception, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "ArithmeticError": ArithmeticError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError,
    "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError, "MemoryError": MemoryError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
    "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "PythonFinalizationError": getattr(builtins, "PythonFinalizationError", RuntimeError),
    "_IncompleteInputError": getattr(builtins, "_IncompleteInputError", SyntaxError),
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "EnvironmentError": OSError, "IOError": OSError, "WindowsError": OSError,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "CancelledError":
        return _cancelled(msg)
    cls = _BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if kind == "ExceptionGroup":
        return cls(msg, [ValueError(msg)])
    if kind == "BaseExceptionGroup":
        return cls(msg, [SystemExit(msg)])
    return cls(msg)


try:
    _cancelled = __import__("asyncio").CancelledError
except Exception:
    _cancelled = type("CancelledError", (Exception,), {})


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    io = CUR["in"].get("io") or {}
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": copy.deepcopy(args),
                         "ret": copy.deepcopy(r)})
    return maybe_raise(copy.deepcopy(r))


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class MarkerDouble:
    def __init__(self, marker):
        self._marker = marker


class LockDouble(MarkerDouble):
    def __enter__(self):
        seam("lock.enter", {})
        return self

    def __exit__(self, *a):
        seam("lock.exit", {})
        return False


class SourcesDouble:
    """OrderedDict double: .get reads, .move_to_end fires a seam."""
    def __init__(self, d):
        self._d = dict(d)

    def __bool__(self):
        return bool(self._d)

    def get(self, key):
        return self._d.get(key)

    def move_to_end(self, key):
        return seam("move_to_end", {"key": key})


class ExecutorDouble:
    def __init__(self, d):
        self._d = dict(d)
        self._threads = d.get("_threads")

    def shutdown(self, **kw):
        return seam("shutdown", {k: v for k, v in kw.items() if v is not None})


class GatewayRunnerDouble:
    """Data attrs are real attributes; methods not bound as real attrs are
    resolved via __getattr__ (method-presence markers from in.runner)."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return runner_method_rec(name)
        raise AttributeError("'GatewayRunner' object has no attribute '%s'" % name)


def runner_method_rec(name):
    def rec(*a, **k):
        if name == "_sessions_map":
            r = seam(name, {})
            if isinstance(r, dict):
                return {kk: mat_state(vv) for kk, vv in r.items()}
            return r
        if name == "_peek_session_state":
            r = seam(name, {"session_key": denorm(a[0]) if a else ""})
            return mat_state(r)
        if name == "_session_key_for_source":
            return seam(name, {"source": denorm(a[0])})
        if name == "_open_session_db_for_active_scope":
            return seam(name, {})
        if a or k:
            return seam(name, {"args": [denorm(x) for x in a],
                               "kwargs": {kk: denorm(vv) for kk, vv in k.items()}})
        return seam(name, {})
    return rec


UNPINNED = object()


def mat_state(v):
    if not isinstance(v, dict):
        return v
    out = dict(v)
    turn = v.get("turn")
    if isinstance(turn, dict):
        out["turn"] = types.SimpleNamespace(**turn)
    return types.SimpleNamespace(**out)


def mat_state_value(k, v):
    if k == "_session_db_pinned" and isinstance(v, dict) and v.get("__unpinned__"):
        return UNPINNED
    if k == "_session_sources" and isinstance(v, dict):
        return SourcesDouble(v)
    if k == "_executor_lock" and v is not None:
        return LockDouble(v if isinstance(v, str) else "lock")
    if k in ("_executor", "_housekeeping_executor") and isinstance(v, dict):
        return types.SimpleNamespace(**v)
    return v


def denorm(x):
    if isinstance(x, MarkerDouble):
        return x._marker
    if isinstance(x, SourcesDouble):
        return denorm(x._d)
    if isinstance(x, GatewayRunnerDouble):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, types.SimpleNamespace):
        return {k: denorm(v) for k, v in vars(x).items()}
    if isinstance(x, _pathlib.PurePath):
        return x.as_posix()
    if isinstance(x, BaseException):
        return raise_spec(x)
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(denorm(v) for v in x)
    return x


def canon(x):
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


def run_flow(fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": None}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": None}


# ── live-def extraction ──
WANTED_METHODS = (
    "_status_action_label", "_status_action_gerund", "_sessions_map",
    "_peek_session_state", "_session_state", "_is_session_running",
    "_running_agent_items", "_running_agent_count",
    "_resolve_session_key_or_none", "_get_cached_session_source",
    "_stop_pool", "_get_or_create_pool", "_update_runtime_status",
    "_session_db",
)


def extract_run(pre):
    path = os.path.join(QUARRY, "gateway/run.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "gr4_extracted_gateway_run_py", "__file__": path}
    ns.update(pre)
    tree = ast.parse(src)

    def seed_assigns():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not (isinstance(node.target, ast.Name) and node.value is not None):
                    continue
            else:
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
            except Exception:
                pass

    seed_assigns()
    flags = __future__.annotations.compiler_flag
    lines = src.splitlines()
    seen = set()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayRunner":
            for m in node.body:
                if (isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef))
                        and m.name in WANTED_METHODS and m.name not in seen):
                    seen.add(m.name)
                    seg = textwrap.dedent("\n".join(lines[m.lineno - 1:m.end_lineno]))
                    mnode = ast.parse(seg).body[0]
                    mnode.decorator_list = []
                    exec(compile(ast.Module(body=[mnode], type_ignores=[]), path,
                                 "exec", flags=flags), ns)
    if seen != set(WANTED_METHODS):
        raise RuntimeError("missing live defs: %s" % (set(WANTED_METHODS) - seen,))
    seed_assigns()
    return ns


def lock_ctor():
    r = seam("Lock", {})
    return LockDouble(r if isinstance(r, str) else "lock")


def session_state_ctor():
    r = seam("SessionState", {})
    return mat_state(r)


def t_stub(key):
    return seam("t", {"key": key})


def write_status_stub(**kw):
    return seam("_write_runtime_status_quiet",
                {k: denorm(v) for k, v in kw.items()})


NS = extract_run(pre={
    "t": t_stub,
    "suppress": contextlib.suppress,
    "threading": types.SimpleNamespace(Lock=lock_ctor),
    "concurrent": types.SimpleNamespace(
        futures=types.SimpleNamespace(ThreadPoolExecutor=lambda **k: seam("ThreadPoolExecutor", k))),
    "SessionState": session_state_ctor,
    "_SESSION_DB_UNPINNED": UNPINNED,
    "_write_runtime_status_quiet": write_status_stub,
    "GatewayRunner": GatewayRunnerDouble,
})
# module-level Assign seeding may have overwritten the doubles; restore.
NS["t"] = t_stub
NS["suppress"] = contextlib.suppress
NS["threading"] = types.SimpleNamespace(Lock=lock_ctor)
NS["SessionState"] = session_state_ctor
NS["_SESSION_DB_UNPINNED"] = UNPINNED
NS["_write_runtime_status_quiet"] = write_status_stub
NS["GatewayRunner"] = GatewayRunnerDouble


def build_runner(inp, skip=()):
    rd = GatewayRunnerDouble(inp.get("runner") or {})
    for k, v in (inp.get("state") or {}).items():
        object.__setattr__(rd, k, mat_state_value(k, v))
    rn = inp.get("runner") or {}
    for nm in WANTED_METHODS:
        if nm in skip or nm in rn:
            continue
        object.__setattr__(rd, nm, types.MethodType(NS[nm], rd))
    return rd


def mat_source(v):
    if isinstance(v, dict):
        return types.SimpleNamespace(**v)
    return v


FUNC_NAMES = {
    "_status_action_label": "status_action_label",
    "_status_action_gerund": "status_action_gerund",
    "_sessions_map": "sessions_map",
    "_peek_session_state": "peek_session_state",
    "_session_state": "session_state",
    "_is_session_running": "is_session_running",
    "_running_agent_items": "running_agent_items",
    "_running_agent_count": "running_agent_count",
    "_resolve_session_key_or_none": "resolve_session_key_or_none",
    "_get_cached_session_source": "get_cached_session_source",
    "_stop_pool": "stop_pool",
    "_get_or_create_pool": "get_or_create_pool",
    "_update_runtime_status": "update_runtime_status",
    "_session_db": "session_db_get",
}
FIXTURE_TO_LIVE = {v: k for k, v in FUNC_NAMES.items()}


def drive(func, inp):
    live = FIXTURE_TO_LIVE.get(func)
    if live is None:
        raise ValueError("unknown func: %s" % func)
    fn = NS[live]
    sk = inp.get("session_key", "")

    if func in ("status_action_label", "status_action_gerund", "sessions_map",
                "running_agent_count", "session_db_get"):
        rd = build_runner(inp, skip=("_open_session_db_for_active_scope",)
                          if func == "session_db_get" else ())
        return run_flow(lambda: fn(rd))

    if func == "peek_session_state":
        rd = build_runner(inp)
        return run_flow(lambda: fn(rd, sk))

    if func == "session_state":
        rd = build_runner(inp, skip=("_sessions_map",))
        return run_flow(lambda: fn(rd, sk))

    if func == "is_session_running":
        rd = build_runner(inp, skip=("_peek_session_state",))
        return run_flow(lambda: fn(rd, sk))

    if func == "running_agent_items":
        rd = build_runner(inp, skip=("_sessions_map",))
        return run_flow(lambda: fn(rd))

    if func == "resolve_session_key_or_none":
        rd = build_runner(inp, skip=("_session_key_for_source",))
        src = mat_source(inp.get("source"))
        return run_flow(lambda: fn(rd, src, inp.get("session_key")))

    if func == "get_cached_session_source":
        rd = build_runner(inp)
        return run_flow(lambda: fn(rd, sk))

    if func == "stop_pool":
        ex = inp.get("executor")
        exd = None if ex is None else ExecutorDouble(ex)
        return run_flow(lambda: fn(exd))

    if func == "get_or_create_pool":
        rd = build_runner(inp)
        attr = inp.get("attr", "_executor")
        mp = lambda: seam("make_pool", {"attr": attr})
        return run_flow(lambda: fn(rd, attr, mp))

    if func == "update_runtime_status":
        rd = build_runner(inp, skip=("_describe_active_work", "_active_work_count"))
        return run_flow(lambda: fn(rd, gateway_state=inp.get("gateway_state"),
                                   exit_reason=inp.get("exit_reason")))

    raise ValueError("unknown func: %s" % func)


def install_case():
    CUR["idx"] = {}
    # cross-check: fixture io keys must be declared seams (seam-name set equality).
    extra = set((CUR["in"].get("io") or {}).keys()) - SEAM_NAMES
    if extra:
        raise ValueError("fixture io keys without a seam: %s" % sorted(extra))


def main():
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
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        try:
            install_case()
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE gateway_run_flow4: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
