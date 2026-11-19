#!/usr/bin/env python3
"""Oracle for the gateway/scale_to_zero.py UNCLAIMED-remainder FLOW cores
(src/gateway/scale_to_zero_flow.c).

AST-extracts the LIVE suspend-layer helpers (no heavy gateway import), seeds the
module constants / logger / os / urllib / socket doubles, and replaces every live
I/O dependency with recording stubs driven by the SAME per-name scripts the C
harness feeds its universal seams (in.io). Compares the ordered seam-call trace
plus the return value and the malformed-sleep-url latch state.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion, empty -> None);
{"__raise__": kind, "msg": text, ...} raises a real exception of that kind; a
scripted null -> None; ABSENT key -> FAIL-CLOSED RuntimeError "no script for
<name>" (recorded as ret, then raised). BaseException kinds pass through every
`except OSError` / `except (URLError,OSError,ValueError)` site. The 6 ported
functions run inline (real) on both sides; the 8 claimed decision-layer siblings
are not called by them.

Input: stdin JSON lines from t_port_scale_to_zero_flow.
Summary: ORACLE scale_to_zero_flow: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import json
import os
import sys
import types
import __future__
from pathlib import Path

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


def rec(ev):
    CUR["trace"].append(ev)


def script(name, args):
    io = CUR["in"].get("io") or {}
    if name not in io:
        spec = {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
        rec({"ev": name, "args": args, "ret": spec})
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
    rec({"ev": name, "args": args, "ret": val})
    return val


# urllib stub exception classes (identity targets of the live except clauses).
# Named exactly URLError/HTTPError so type(e).__name__ matches the scripted kind.
URLErrorStub = type("URLError", (OSError,), {})
HTTPErrorStub = type("HTTPError", (URLErrorStub,), {})


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
        import asyncio
        return asyncio.CancelledError(msg)
    if kind == "HTTPError":
        e = HTTPErrorStub(msg)
        e.code = v.get("code")
        e.reason = v.get("reason")
        return e
    if kind == "URLError":
        return URLErrorStub(msg)
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
        "KeyError": KeyError, "OSError": OSError, "FileNotFoundError": FileNotFoundError,
        "RuntimeError": RuntimeError, "TimeoutError": TimeoutError, "ImportError": ImportError,
        "OverflowError": OverflowError, "UnboundLocalError": UnboundLocalError,
        "NameError": NameError, "ConnectionError": ConnectionError,
        "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
        "PermissionError": PermissionError, "MemoryError": MemoryError,
    }
    cls = builtins.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


def materialize(v):
    if isinstance(v, dict) and "__raise__" in v:
        raise make_exc(v)
    return v


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


def proj_bytes(b):
    if isinstance(b, bytes):
        return b.decode("utf-8", "replace")
    return b


class RecLogger:
    def _norm_arg(self, a):
        if isinstance(a, BaseException):
            return {"__raise__": type(a).__name__, "msg": str(a)}
        return a

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

    def log(self, level, fmt, *a, **k):
        return self._rec("logger.log", level, fmt, a, k)


# ── context-manager doubles ──
class ResponseCM:
    def __init__(self, spec):
        object.__setattr__(self, "_spec", dict(spec))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_spec")
        if name in d:
            return d[name]
        raise AttributeError(name)

    def __enter__(self):
        materialize(script("response.__enter__", {}))
        return self

    def __exit__(self, et, ev, tb):
        exc = raise_spec(ev) if ev is not None else None
        r = materialize(script("response.__exit__", {"exc": exc}))
        return bool(r)


class SockCM:
    def __init__(self, spec):
        object.__setattr__(self, "_spec", dict(spec))

    def __enter__(self):
        materialize(script("sock.__enter__", {}))
        return self

    def __exit__(self, et, ev, tb):
        exc = raise_spec(ev) if ev is not None else None
        r = materialize(script("sock.__exit__", {"exc": exc}))
        return bool(r)

    def settimeout(self, t):
        return materialize(script("sock.settimeout", {"timeout": t}))

    def connect(self, p):
        return materialize(script("sock.connect", {"path": p}))

    def sendall(self, d):
        return materialize(script("sock.sendall", {"data": proj_bytes(d)}))

    def recv(self, n):
        v = materialize(script("sock.recv", {"size": n}))
        if isinstance(v, str):
            return v.encode("utf-8")
        return v


def wrap_cm(v, cls):
    return cls(v) if isinstance(v, dict) else v


# ── os double ──
class _OsPath:
    def exists(self, p):
        return materialize(script("os.path.exists", {"path": p}))


class _Os:
    def __init__(self):
        self.path = _OsPath()

    @property
    def environ(self):
        return materialize(script("os.environ", {}))


# ── live-def extraction ──
TOPS = (
    "_env_str", "self_suspend_available", "brokered_sleep_url",
    "suspend_available", "request_brokered_suspend", "suspend_self",
)
PORTED = set(TOPS)


def extract():
    path = os.path.join(QUARRY, "gateway", "scale_to_zero.py")
    src = open(path, encoding="utf-8").read()
    ns = {"__name__": "stz_extracted", "__file__": path, "logger": RecLogger(),
          "BROKERED_SUSPEND_TIMEOUT_S": 40.0, "FLY_API_SOCKET": "/.fly/api",
          "DEFAULT_IDLE_TIMEOUT_MINUTES": 2}
    sys.modules.setdefault("stz_extracted", types.ModuleType("stz_extracted"))
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


def stubmod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


def rec_urlsplit(url):
    v = materialize(script("urlsplit", {"url": url}))
    return types.SimpleNamespace(**v) if isinstance(v, dict) else v


def rec_request(url, data=None, method=None):
    return materialize(script("urllib.request.Request",
                              {"url": url, "data": proj_bytes(data), "method": method}))


def rec_urlopen(request, timeout=None):
    return wrap_cm(materialize(script("urlopen", {"request": request, "timeout": timeout})), ResponseCM)


def rec_opener(request, timeout=None):
    return wrap_cm(materialize(script("opener", {"request": request, "timeout": timeout})), ResponseCM)


def rec_socket(family, type):
    return wrap_cm(materialize(script("socket.socket", {"family": family, "type": type})), SockCM)


def install_globals():
    NS["logger"] = RecLogger()
    NS["json"] = json
    NS["os"] = _Os()
    NS["socket"] = types.SimpleNamespace(socket=rec_socket, AF_UNIX=1, SOCK_STREAM=1)
    up = pkg("urllib")
    up.parse = stubmod("urllib.parse", urlsplit=rec_urlsplit)
    up.error = stubmod("urllib.error", HTTPError=HTTPErrorStub, URLError=URLErrorStub)
    up.request = stubmod("urllib.request", Request=rec_request, urlopen=rec_urlopen)
    NS["urllib"] = up
    # module constants referenced by the ported defs
    NS["SCALE_TO_ZERO_ENV"] = "HERMES_SCALE_TO_ZERO"
    NS["FLY_APP_NAME_ENV"] = "FLY_APP_NAME"
    NS["FLY_MACHINE_ID_ENV"] = "FLY_MACHINE_ID"
    NS["FLY_API_SOCKET"] = "/.fly/api"
    NS["SLEEP_URL_ENV"] = "GATEWAY_RELAY_SLEEP_URL"
    NS["BROKERED_SUSPEND_TIMEOUT_S"] = 40.0
    NS["_malformed_sleep_url_logged"] = False


install_globals()


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": ret, "state": state_fn()}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": state_fn()}


def drive(func, inp):
    st = inp.get("state") or {}
    NS["_malformed_sleep_url_logged"] = bool(st.get("malformed_logged", False))
    NS["logger"] = RecLogger()
    state_echo = lambda: {"malformed_logged": NS["_malformed_sleep_url_logged"]}
    env = inp.get("environ")

    if func == "env_str":
        return run_flow(lambda: NS["_env_str"](inp.get("env"), inp.get("key", "")), state_echo)
    if func == "self_suspend_available":
        return run_flow(lambda: NS["self_suspend_available"](env), state_echo)
    if func == "brokered_sleep_url":
        return run_flow(lambda: NS["brokered_sleep_url"](env), state_echo)
    if func == "suspend_available":
        return run_flow(lambda: NS["suspend_available"](env), state_echo)
    if func == "request_brokered_suspend":
        opener = rec_opener if inp.get("opener") else None
        return run_flow(lambda: NS["request_brokered_suspend"](
            inp.get("url", ""), timeout=inp.get("timeout", 40.0), opener=opener), state_echo)
    if func == "suspend_self":
        return run_flow(lambda: NS["suspend_self"](
            env, socket_path=inp.get("socket_path", "/.fly/api"),
            timeout=inp.get("timeout", 10.0)), state_echo)
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
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE scale_to_zero_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
