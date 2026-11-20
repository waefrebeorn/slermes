#!/usr/bin/env python3
"""Oracle for the gateway/scale_to_zero.py FLOW-remainder cores
(src/gateway/scale_to_zero_flow2.c).

AST-extracts the LIVE decision-layer defs (no heavy gateway import), seeds the
module constants / logger / os / time / Path doubles, and replaces every live
I/O dependency with recording stubs driven by the SAME per-name scripts the C
harness feeds its universal seams (in.io). Compares the ordered seam-call trace
plus the return value. dashboard_client_heartbeat_path runs INLINE (real) inside
touch/last_seen on both sides; its hermes_constants from-import fires the
"import_get_process_hermes_home" seam (module __getattr__) then the
"get_process_hermes_home" call seam — in that order.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion, empty -> None);
{"__raise__": kind, "msg": text, ...} raises a real exception of that kind; a
scripted null -> None; ABSENT key -> FAIL-CLOSED RuntimeError "no script for
<name>" (recorded as ret, then raised). BaseException kinds (SystemExit /
KeyboardInterrupt / GeneratorExit / CancelledError / BaseExceptionGroup) pass
through the `except Exception` site in touch_dashboard_client_heartbeat.

Domain (mirrors the port header): ASCII scope; integral floats == ints (canon
folds); env/element str() positions exclude integral floats; float() tokens are
printable ASCII without quotes/backslashes and non-finite results are excluded;
path inputs are str|null; object platform elements are SimpleNamespace doubles
with scalar values; same-type container comparisons are excluded. Path returns
and Path seam args project via as_posix() (Windows pathlib live).

Input: stdin JSON lines from t_port_scale_to_zero_flow2.
Summary: ORACLE scale_to_zero_flow2: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import asyncio
import json
import math
import os
import sys
import types
import __future__
from pathlib import Path, PurePath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": []}
SEQPOS = {}


def proj(x):
    """Project Path seam args/returns to their as_posix() string (C model)."""
    if isinstance(x, PurePath):
        return x.as_posix()
    if isinstance(x, dict):
        return {k: proj(v) for k, v in x.items()}
    if isinstance(x, list):
        return [proj(v) for v in x]
    return x


def rec(ev):
    CUR["trace"].append(ev)


def script(name, args):
    io = CUR["in"].get("io") or {}
    if name not in io:
        spec = {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
        rec({"ev": name, "args": proj(args), "ret": spec})
        return spec
    sc = io[name]
    val = sc
    if isinstance(sc, dict) and "__seq__" in sc:
        seq = sc["__seq__"]
        idx = SEQPOS.get(name, 0)
        if not seq:
            val = None
        else:
            if idx >= len(seq):
                idx = len(seq) - 1
            val = seq[idx]
            SEQPOS[name] = idx + 1
    rec({"ev": name, "args": proj(args), "ret": val})
    return val


# BaseException-family kinds sit OUTSIDE Exception (pass through except Exception).
_BASE_KINDS = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
               "CancelledError", "BaseExceptionGroup"}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
        "KeyError": KeyError, "IndexError": IndexError, "OSError": OSError,
        "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
        "PermissionError": PermissionError, "IsADirectoryError": IsADirectoryError,
        "NotADirectoryError": NotADirectoryError, "InterruptedError": InterruptedError,
        "RuntimeError": RuntimeError, "TimeoutError": TimeoutError,
        "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
        "OverflowError": OverflowError, "MemoryError": MemoryError,
        "UnboundLocalError": UnboundLocalError, "NameError": NameError,
        "ConnectionError": ConnectionError, "ConnectionResetError": ConnectionResetError,
        "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
        "ConnectionRefusedError": ConnectionRefusedError, "BlockingIOError": BlockingIOError,
        "ChildProcessError": ChildProcessError, "ProcessLookupError": ProcessLookupError,
        "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
        "FloatingPointError": FloatingPointError, "LookupError": LookupError,
        "RecursionError": RecursionError, "NotImplementedError": NotImplementedError,
        "SystemError": SystemError, "BufferError": BufferError, "EOFError": EOFError,
        "AssertionError": AssertionError, "ReferenceError": ReferenceError,
        "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
        "SyntaxError": SyntaxError, "IndentationError": IndentationError,
        "TabError": TabError, "UnicodeError": UnicodeError,
        "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
        "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
        "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
        "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
        "PendingDeprecationWarning": PendingDeprecationWarning,
        "ResourceWarning": ResourceWarning, "SyntaxWarning": SyntaxWarning,
        "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
        "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
        "Exception": Exception, "BaseException": BaseException,
        "EnvironmentError": OSError, "IOError": OSError,
    }
    cls = builtins.get(kind)
    if cls is None:
        base = BaseException if kind in _BASE_KINDS else Exception
        cls = type(kind, (base,), {})
    return cls(msg)


def materialize(v):
    if isinstance(v, dict) and "__raise__" in v:
        raise make_exc(v)
    return v


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def norm_nums(x):
    if isinstance(x, float) and math.isfinite(x) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class RecLogger:
    def _norm_arg(self, a):
        if isinstance(a, BaseException):
            return {"__raise__": type(a).__name__, "msg": str(a)}
        return proj(a)

    def _rec(self, name, level, fmt, a, k):
        args = {"fmt": fmt}
        if level is not None:
            args["level"] = level
        if a:
            args["args"] = [self._norm_arg(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return materialize(script(name, args))

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", None, fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", None, fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", None, fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", None, fmt, a, k)


# ── os double (dirname lives on os.path; makedirs/utime/stat on os itself —
# the outer attribute resolves BEFORE the argument seam fires) ──
class _OsPath:
    def dirname(self, p):
        return materialize(script("os.path.dirname", {"path": p}))


class _Os:
    def __init__(self):
        self.path = _OsPath()

    @property
    def environ(self):
        return materialize(script("os.environ", {}))

    def makedirs(self, name, mode=0o777, exist_ok=False):
        return materialize(script("os.makedirs", {"path": name, "exist_ok": exist_ok}))

    def utime(self, path, times=None, **k):
        return materialize(script("os.utime", {"path": path, "times": times}))

    def stat(self, path, **k):
        v = materialize(script("os.stat", {"path": path}))
        return types.SimpleNamespace(**v) if isinstance(v, dict) else v


# ── open/close double: an OBJECT script result models a file handle ──
class FileHandle:
    def close(self):
        return materialize(script("close", {}))


def rec_open(file, mode="r", *a, **k):
    v = materialize(script("open", {"path": file, "mode": mode,
                                    "encoding": k.get("encoding")}))
    return FileHandle() if isinstance(v, dict) else v


def rec_time():
    return materialize(script("time.time", {}))


# ── hermes_constants from-import stub: __getattr__ fires the import seam once
# per fetch (AttributeError on __path__ keeps it a NON-package: no double fire) ──
def _hc_getattr(name):
    if name != "get_process_hermes_home":
        raise AttributeError(name)
    materialize(script("import_get_process_hermes_home", {}))

    def _call():
        return materialize(script("get_process_hermes_home", {}))
    return _call


# ── live-def extraction ──
TOPS = (
    "_env_str", "scale_to_zero_enabled", "parse_idle_timeout_seconds",
    "messaging_is_relay_only_or_absent", "should_arm", "is_idle",
    "dashboard_client_heartbeat_path", "touch_dashboard_client_heartbeat",
    "dashboard_client_last_seen",
)
PORTED = set(TOPS) - {"_env_str"}


def extract():
    path = os.path.join(QUARRY, "gateway", "scale_to_zero.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "stz2_extracted", "__file__": path, "logger": RecLogger(),
          "DEFAULT_IDLE_TIMEOUT_MINUTES": 2}
    sys.modules.setdefault("stz2_extracted", types.ModuleType("stz2_extracted"))
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TOPS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
    missing = PORTED - set(ns)
    if missing:
        raise RuntimeError("extraction missing: %s" % sorted(missing))
    return ns


NS = extract()


def install_globals():
    NS["logger"] = RecLogger()
    NS["os"] = _Os()
    NS["time"] = types.SimpleNamespace(time=rec_time)
    NS["open"] = rec_open
    NS["Path"] = Path
    NS["SCALE_TO_ZERO_ENV"] = "HERMES_SCALE_TO_ZERO"
    NS["_TRUTHY"] = {"1", "true", "yes", "on"}
    NS["DEFAULT_IDLE_TIMEOUT_MINUTES"] = 2
    # REL computed with the REAL os.path.join (Windows backslashes); pathlib
    # splits it into the same components the C POSIX-tail model joins.
    NS["DASHBOARD_CLIENT_HEARTBEAT_REL"] = os.path.join("state", "dashboard_clients.heartbeat")
    m = types.ModuleType("hermes_constants")
    m.__getattr__ = _hc_getattr
    sys.modules["hermes_constants"] = m


install_globals()


def run_flow(fn):
    try:
        ret = fn()
        if isinstance(ret, PurePath):
            ret = ret.as_posix()
        return {"ret": ret}
    except BaseException as e:
        return {"ret": raise_spec(e)}


def drive(func, inp):
    NS["logger"] = RecLogger()
    if func == "scale_to_zero_enabled":
        return run_flow(lambda: NS["scale_to_zero_enabled"](inp.get("environ")))
    if func == "parse_idle_timeout_seconds":
        dflt = inp["default_minutes"] if "default_minutes" in inp else 2
        return run_flow(lambda: NS["parse_idle_timeout_seconds"](inp.get("cfg_value"), dflt))
    if func == "messaging_is_relay_only_or_absent":
        plats = inp.get("platforms")
        if isinstance(plats, list):
            plats = [types.SimpleNamespace(**e) if isinstance(e, dict) else e for e in plats]
        return run_flow(lambda: NS["messaging_is_relay_only_or_absent"](plats))
    if func == "should_arm":
        return run_flow(lambda: NS["should_arm"](
            enabled=inp.get("enabled"), relay_only_or_absent=inp.get("relay_only_or_absent"),
            wake_url=inp.get("wake_url")))
    if func == "is_idle":
        return run_flow(lambda: NS["is_idle"](
            active_work_count=inp.get("active_work_count"),
            seconds_since_last_inbound=inp.get("seconds_since_last_inbound"),
            idle_timeout_seconds=inp.get("idle_timeout_seconds"),
            has_live_background_work=inp.get("has_live_background_work")))
    if func == "dashboard_client_heartbeat_path":
        return run_flow(lambda: NS["dashboard_client_heartbeat_path"](inp.get("hermes_home")))
    if func == "touch_dashboard_client_heartbeat":
        return run_flow(lambda: NS["touch_dashboard_client_heartbeat"](inp.get("path")))
    if func == "dashboard_client_last_seen":
        return run_flow(lambda: NS["dashboard_client_last_seen"](
            inp.get("path"), now=inp.get("now")))
    raise ValueError("unknown func: %s" % func)


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
        SEQPOS.clear()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e)}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE scale_to_zero_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())