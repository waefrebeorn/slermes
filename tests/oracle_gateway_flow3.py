#!/usr/bin/env python3
"""Oracle for gateway_flow3 -- the UNCLAIMED-remainder FLOW cores of
hermes_cli/gateway.py (LIVE authority, py3.14 quarry venv) beyond the flow2
PoP wave.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the
quarry (no heavy import), module constants seeded (Assign AND AnnAssign),
every effect replaced by a recording stub returning the SAME scripted results
the C harness feeds its universal seam. Compares the ordered DECISION SEQUENCE
(trace of seam calls) plus the return value. Scripted {"__raise__": cls} raises
a REAL exception of that class (multi-arg constructors bypassed via
cls.__new__ + Exception.__init__); drive() catches BaseException so
KeyboardInterrupt/SystemExit/GeneratorExit pass through live except-Exception
clauses exactly as the C port propagates them. A MISSING seam key fail-closes
(RuntimeError) on both sides; a scripted null is a valid None result.

Input: stdin JSON lines from t_port_gateway_flow3:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE gateway_flow3: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.

Modes:
  --self-test    drive every fixture (no C), flag binding/script gaps
  --shadow-test  diff a literal Python shadow of the C port vs live drive
                 over every fixture (expect 0 mismatches) -- the shadow diff
"""
import ast
import builtins
import json
import math
import os
import re
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gf3_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway.py")

# harness func -> live python name
PY = {
    "parse_ps_line": "_parse_ps_line",
    "iter_windows_list_processes": "_iter_windows_list_processes",
    "scm_service_field": "_scm_service_field",
    "gateway_detached_env": "_gateway_detached_env",
    "stdin_is_tty": "_stdin_is_tty",
    "home_owns_bare_service_name": "_home_owns_bare_service_name",
    "cmd_setup": "_cmd_setup",
    "cmd_list": "_cmd_list",
    "cmd_migrate_legacy": "_cmd_migrate_legacy",
    "print_unserved_shared_ingress": "_print_unserved_shared_ingress",
    "print_duplicate_credential_warnings": "_print_duplicate_credential_warnings",
    "absorb_windows_console_controls": "_absorb_windows_console_controls",
    "ping_loop_tick_witness": "_ping_loop_tick_witness",
    "iter_proc_cmdlines": "_iter_proc_cmdlines",
    "wait_for_tcp_port_free": "_wait_for_tcp_port_free",
    "restart_argv_is_host_gateway": "_restart_argv_is_host_gateway",
    "gw_windows": "_gw_windows",
}
WANTED = set(PY.values())

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}
SH = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


def _drop_none(args):
    return {k: v for k, v in args.items() if v is not None}


def _fetch(state, name, args):
    args = _drop_none(args)
    scripts = state["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = state["idx"].get(name, 0)
            state["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    state["trace"].append({"ev": name, "args": args, "ret": r})
    return r


def seam(name, args):
    return maybe_raise(_fetch(CUR, name, args))


def shadow_seam(name, args):
    return maybe_raise(_fetch(SH, name, args))


# ---- full builtin exception hierarchy (py3.14 quarry builtins) ----
EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "FileExistsError": FileExistsError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "TimeoutError": TimeoutError, "InterruptedError": InterruptedError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "ProcessLookupError": ProcessLookupError,
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "JSONDecodeError": json.JSONDecodeError,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "AssertionError": AssertionError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "ArithmeticError": ArithmeticError,
    "FloatingPointError": FloatingPointError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError,
    "SystemError": SystemError, "BufferError": BufferError, "EOFError": EOFError,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "ResourceWarning": ResourceWarning, "ImportWarning": ImportWarning,
    "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "EncodingWarning": EncodingWarning, "FutureWarning": FutureWarning,
    "Exception": Exception, "BaseException": BaseException,
    "KeyboardInterrupt": KeyboardInterrupt, "SystemExit": SystemExit,
    "GeneratorExit": GeneratorExit, "BaseExceptionGroup": BaseExceptionGroup,
    "ExceptionGroup": ExceptionGroup,
}
MULTI_ARG = {"UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError",
             "JSONDecodeError"}


def make_exc(spec):
    name = spec["__raise__"]
    msg = spec.get("msg", "")
    cls = EXC_MAP.get(name)
    if cls is None:
        cls = type(name, (Exception,), {})
    if name in MULTI_ARG:
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    if name == "BaseExceptionGroup":
        # BaseExceptionGroup.__str__ appends " (N sub-exception(s))"; keep the
        # scripted msg observable verbatim while staying a BaseException-only
        # class (a dynamic subclass with the SAME __name__ so the oracle's
        # ret envelope still reports "BaseExceptionGroup").
        cls = type("BaseExceptionGroup", (BaseExceptionGroup,),
                   {"__str__": lambda self: getattr(self, "_gf3_msg", "")})
        e = cls(msg, [KeyboardInterrupt("x")])
        e._gf3_msg = msg
        return e
    if name == "ExceptionGroup":
        return cls(msg, [Exception("x")])
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


# ---------------- extraction ----------------

def extract():
    src = Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    ns = {"__name__": "hermes_cli.gateway", "__builtins__": builtins,
          "math": math, "json": json, "os": types.SimpleNamespace(getpid=lambda: 1),
          "sys": types.SimpleNamespace(stdin=None),
          "contextlib": __import__("contextlib"),
          "socket": types.SimpleNamespace(SOCK_STREAM=1),
          "signal": types.SimpleNamespace(), "re": re}
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.get_source_segment(src, node), SRC, "exec"), ns)
            except BaseException:
                pass
    ns["logger"] = types.SimpleNamespace(  # module logger Assign must not swallow seam logs
        debug=lambda *a, **k: None, warning=lambda *a, **k: None,
        info=lambda *a, **k: None, error=lambda *a, **k: None)
    got = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.get_source_segment(src, node), SRC, "exec"), ns)
            got.add(node.name)
    missing = WANTED - got
    if missing:
        print("EXTRACT FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    return ns


# ---- doubles ----

class ServiceDouble:
    def __init__(self, tag):
        self.tag = tag

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        spec = seam("scm_getattr", {"service": self.tag, "field": name})
        if isinstance(spec, dict) and spec.get("callable") is True:
            return CallableDouble(name)
        return None  # non-callable -> service.as_dict().get(field)

    def as_dict(self):
        return seam("scm_as_dict", {})


class CallableDouble:
    def __init__(self, field):
        self.field = field

    def __call__(self):
        return seam("scm_call", {"field": self.field})


class StdinDouble:
    def isatty(self):
        return seam("stdin_isatty", {})


class SockDouble:
    def settimeout(self, t):
        seam("settimeout", {"timeout": t})

    def connect(self, address):
        seam("connect", {"address": address})

    def recv(self, n):
        v = seam("recv", {"n": n})
        return bytes(v) if isinstance(v, list) else v

    def close(self):
        seam("close", {})


class ResolvedDouble:
    def __init__(self, s):
        self.s = s

    def resolve(self):
        return self.s


class FileDouble:
    def __init__(self, data):
        self.data = data

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self):
        return self.data.encode("utf-8") if isinstance(self.data, str) else bytes(self.data)


# ---------------- stub installation / drive ----------------

def _mod(name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    return m


def _print_stub(*a, **k):
    seam("print", {"text": " ".join(str(x) for x in a)})


def install_stubs(ns):
    ns["print"] = _print_stub
    # gateway_detached_env / io
    ns["os"] = types.SimpleNamespace(
        getenv=lambda name: seam("getenv", {"name": name}),
        getpid=lambda: seam("getpid", {}),
        listdir=lambda path: seam("listdir", {"path": path}),
        environ=types.SimpleNamespace(__setitem__=lambda s, k, v: None),
    )
    ns["_truthy_env"] = lambda value: seam("truthy_env", {"value": value})
    ns["sys"] = types.SimpleNamespace(stdin=None, version="3.14.9 x", platform="win32")
    ns["_native_service_homes"] = lambda: seam("native_service_homes", {})
    ns["_bare_unit_pinned_home"] = lambda: seam("bare_unit_pinned_home", {})
    ns["gateway_setup"] = lambda: seam("gateway_setup", {})
    ns["_gateway_list"] = lambda: seam("gateway_list", {})
    ns["supports_systemd_services"] = lambda: seam("supports_systemd_services", {})
    ns["is_macos"] = lambda: seam("is_macos", {})
    ns["remove_legacy_hermes_units"] = lambda **kw: seam("remove_legacy_hermes_units", kw)
    # import-bearing functions: stub modules in sys.modules
    gms = _mod("hermes_cli.gateway_multiplex_served")
    gms.served_profile_unserved_platforms = lambda p: seam("served_unserved_platforms", {"profile": p})
    gmig = _mod("hermes_cli.gateway_migrate")
    gmig.duplicate_credential_findings = lambda: seam("duplicate_credential_findings", {})
    # package attrs for `from pkg import sub`/`from pkg.sub import name`
    hp = sys.modules.get("hermes_cli") or _mod("hermes_cli")
    hp.gateway_multiplex_served, hp.gateway_migrate = gms, gmig
    sig = _mod("signal")
    sig.SIGINT, sig.SIG_IGN = "SIGINT", "SIG_IGN"
    sig.signal = lambda a, b: seam("signal", {"sig": a, "handler": b})
    ns["signal"] = sig
    ns["ctypes"] = types.ModuleType("ctypes")
    ns["ctypes"].windll = types.SimpleNamespace(kernel32=types.SimpleNamespace(
        SetConsoleCtrlHandler=lambda a, b: seam("set_console_ctrl_handler", {})))
    sys.modules["ctypes"] = ns["ctypes"]
    sock = _mod("socket")
    sock.SOCK_STREAM = 1
    sock.socket = lambda fam, typ: seam("socket_new", {"family": fam, "sock_stream": typ})
    sock.create_connection = lambda addr, timeout=None: seam("connect", {"address": addr})
    ns["socket"] = sock
    ns["poll_until"] = lambda fn, timeout, interval: seam(
        "poll_until", {"marker": "_port_free", "host": CUR["in"].get("host", ""),
                       "port": CUR["in"].get("port", 0), "timeout": timeout,
                       "interval": interval})
    # restart_argv imports (package attrs so `from agent.secret_scope import ...`
    # and `from gateway import host_rendezvous` resolve without a heavy quarry import)
    sec = _mod("agent.secret_scope")
    sec.is_multiplex_active = lambda: seam("is_multiplex_active", {})
    ap = sys.modules.get("agent") or _mod("agent")
    ap.secret_scope = sec
    hr = _mod("gateway.host_rendezvous")
    hr.ROLE_GATEWAY = "gateway"
    def _hr_read(role):
        r = seam("hr_read_record", {"role": role})
        if isinstance(r, dict):
            return types.SimpleNamespace(**r)
        return r
    hr.read_record = _hr_read
    hr.record_names_live_owner = lambda rec: bool(getattr(rec, "live_owner", False))
    gp = sys.modules.get("gateway") or _mod("gateway")
    gp.host_rendezvous = hr
    hc = _mod("hermes_constants")
    hc.get_hermes_home = lambda: ResolvedDouble(seam("get_hermes_home", {}))
    hc.get_default_hermes_root = lambda: ResolvedDouble(seam("get_default_hermes_root", {}))
    hp.get_hermes_home = hc.get_hermes_home
    hp.get_default_hermes_root = hc.get_default_hermes_root
    ns["_orig_open"] = builtins.open


def drive(ns, func, inp):
    f = ns[PY[func]]
    if func == "parse_ps_line":
        return f(inp["line"])
    if func == "iter_windows_list_processes":
        return list(f(inp["listing"]))
    if func == "scm_service_field":
        return f(ServiceDouble(inp.get("service", "")), inp["field"])
    if func == "gateway_detached_env":
        return f()
    if func == "stdin_is_tty":
        ns["sys"].stdin = StdinDouble() if inp.get("stdin_present", True) else None
        return f()
    if func == "home_owns_bare_service_name":
        return f(inp["home"])
    if func == "cmd_setup":
        return f(types.SimpleNamespace(**(inp.get("args") or {})))
    if func == "cmd_list":
        return f(types.SimpleNamespace(**(inp.get("args") or {})))
    if func == "cmd_migrate_legacy":
        return f(types.SimpleNamespace(**(inp.get("args") or {})))
    if func == "print_unserved_shared_ingress":
        return f(inp.get("profile") or "")
    if func == "print_duplicate_credential_warnings":
        return f()
    if func == "absorb_windows_console_controls":
        sig = ns["signal"]
        if hasattr(sig, "SIGBREAK"):
            del sig.SIGBREAK
        orig_hasattr = builtins.hasattr
        def probed_hasattr(obj, name):
            if obj is sig and name == "SIGBREAK":
                v = bool(_fetch(CUR, "hasattr_sigbreak", {}))
                if v:
                    sig.SIGBREAK = "SIGBREAK"
                elif orig_hasattr(sig, "SIGBREAK"):
                    del sig.SIGBREAK
                return v
            return orig_hasattr(obj, name)
        builtins.hasattr = probed_hasattr
        try:
            return f()
        finally:
            builtins.hasattr = orig_hasattr
    if func == "ping_loop_tick_witness":
        def _sock(fam, typ):
            seam("socket_new", {"family": fam, "sock_stream": typ})
            return SockDouble()
        ns["socket"].socket = _sock
        return f(inp["family"], inp["address"], inp["timeout"])
    if func == "iter_proc_cmdlines":
        orig = builtins.open
        def fake_open(path, mode="r", *a, **k):
            m = re.search(r"/proc/(\d+)/cmdline", str(path))
            pid = int(m.group(1)) if m else 0
            data = seam("read_cmdline", {"pid": pid})
            return FileDouble(data)
        builtins.open = fake_open
        try:
            return list(f(set(inp.get("exclude_pids", []))))
        finally:
            builtins.open = orig
    if func == "wait_for_tcp_port_free":
        return f(inp["host"], inp["port"], timeout=inp.get("timeout", 10.0))
    if func == "restart_argv_is_host_gateway":
        return f(list(inp["argv"]))
    if func == "gw_windows":
        sys.modules["hermes_cli"].gateway_windows = _fetch(CUR, "import_gateway_windows", {})
        return f()
    raise ValueError("unhandled func: %s" % func)


# ---------------- shadow mirror of the C port ----------------
# Literal transcription of gf3_* in Python. Uses the SAME scripted seam channel
# (shadow_seam) so a 0-diff proves the C transcription, not just the fixtures.

_WS = set(" \t\n\r\v\f\x1c\x1d\x1e\x1f")


def s_strip(s):
    i, j = 0, len(s)
    while i < j and s[i] in _WS: i += 1
    while j > i and s[j - 1] in _WS: j -= 1
    return s[i:j]


def s_splitws(s):
    return re.split(r"[ \t\n\r\v\f\x1c-\x1f]+", s.strip("".join(_WS)))


def s_int(s):
    s = s.strip("".join(_WS))
    m = re.fullmatch(r"([+-]?)(\d(?:_?\d)*)", s)
    if not m:
        raise ValueError("bad int")
    return int(m.group(1) + m.group(2).replace("_", ""))


def s_isdigit(s):
    return bool(s) and all("0" <= c <= "9" for c in s)


def shadow(func, inp):
    SH["scripts"] = inp.get("seams", {})
    SH["trace"] = []
    SH["idx"] = {}

    def call(name, args):
        return shadow_seam(name, args)

    def truth(v):
        if v is None: return False
        if isinstance(v, bool): return v
        if isinstance(v, (int, float)): return v != 0
        if isinstance(v, str): return v != ""
        if isinstance(v, (list, dict)): return len(v) > 0
        return False

    if func == "parse_ps_line":
        s = s_strip(inp["line"])
        if not s or "grep" in s: return None
        parts = s.split(None, 1)
        if len(parts) == 2:
            try:
                return [s_int(parts[0]), parts[1]]
            except ValueError:
                pass
        aux = s.split()
        if len(aux) > 10 and s_isdigit(aux[1]):
            return [s_int(aux[1]), " ".join(aux[10:])]
        return None

    if func == "iter_windows_list_processes":
        out, cur = [], ""
        for raw in inp["listing"].split("\n"):
            line = s_strip(raw)
            if line.startswith("CommandLine="):
                cur = line[len("CommandLine="):]
            elif line.startswith("ProcessId="):
                try:
                    out.append([s_int(line[len("ProcessId="):]), cur])
                except ValueError:
                    pass
                cur = ""
        return out

    if func == "scm_service_field":
        spec = call("scm_getattr", {"service": inp.get("service", ""), "field": inp["field"]})
        if isinstance(spec, dict) and spec.get("callable") is True:
            return call("scm_call", {"field": inp["field"]})
        d = call("scm_as_dict", {})
        return d.get(inp["field"]) if isinstance(d, dict) else None

    if func == "gateway_detached_env":
        v = call("getenv", {"name": "HERMES_GATEWAY_DETACHED"})
        return call("truthy_env", {"value": v})

    if func == "stdin_is_tty":
        if inp.get("stdin_present", True) is False:
            return False
        try:
            r = call("stdin_isatty", {})
        except BaseException as ex:
            if isinstance(ex, (ValueError, OSError)):
                return None
            raise
        return truth(r)

    if func == "home_owns_bare_service_name":
        homes = call("native_service_homes", {})
        home = inp["home"]
        if homes is None:
            raise TypeError("argument of type 'NoneType' is not a container or iterable")
        if any(h == home for h in homes): return True
        pin = call("bare_unit_pinned_home", {})
        return pin is not None and pin == home

    if func == "cmd_setup":
        call("gateway_setup", {}); return None
    if func == "cmd_list":
        call("gateway_list", {}); return None
    if func == "cmd_migrate_legacy":
        args = inp.get("args") or {}
        dry, yes = truth(args.get("dry_run")), truth(args.get("yes"))
        if not truth(call("supports_systemd_services", {})):
            if not truth(call("is_macos", {})):
                call("print", {"text": "Legacy unit migration only applies to systemd-based Linux hosts."})
                return None
        call("remove_legacy_hermes_units", {"interactive": not yes, "dry_run": dry})
        return None

    if func == "print_unserved_shared_ingress":
        try:
            un = call("served_unserved_platforms", {"profile": inp.get("profile") or ""})
        except BaseException as ex:
            if isinstance(ex, Exception):
                return None
            raise
        if not un: return None
        call("print", {"text": ""})
        for k in sorted(un.keys()):
            call("print", {"text": "  ⚠ %s: %s" % (k, un[k])})
        call("print", {"text": "  Enable it on the default profile (shared ingress serves every profile), or disable it here."})
        return None

    if func == "print_duplicate_credential_warnings":
        try:
            lines = call("duplicate_credential_findings", {})
            if lines:
                call("print", {"text": ""})
                for line in lines:
                    call("print", {"text": "⚠ %s" % line})
        except BaseException as ex:
            if isinstance(ex, Exception):
                return None
            raise
        return None

    if func == "absorb_windows_console_controls":
        def do_signal(sig):
            try:
                call("signal", {"sig": sig, "handler": "SIG_IGN"})
                return True
            except BaseException as ex:
                if isinstance(ex, (OSError, ValueError)):
                    return False
                raise
        if not do_signal("SIGINT"):
            pass  # SIGBREAK block skipped when the first signal() raised
        elif truth(call("hasattr_sigbreak", {})):
            do_signal("SIGBREAK")
        try:
            call("set_console_ctrl_handler", {})
        except BaseException as ex:
            if not isinstance(ex, (OSError, AttributeError)):
                raise
        return None

    if func == "ping_loop_tick_witness":
        tmo = max(float(inp["timeout"]), 0.0)
        result = False
        pending = None
        try:
            call("socket_new", {"family": inp["family"], "sock_stream": 1})
        except BaseException as ex:
            if not isinstance(ex, Exception):
                raise
            return False  # except Exception: return False; sock is None so no close
        try:
            call("settimeout", {"timeout": tmo})
            call("connect", {"address": inp["address"]})
            rv = call("recv", {"n": 1})
            result = isinstance(rv, list) and rv == [49]
        except BaseException as ex:
            if isinstance(ex, Exception):
                result = False
            else:
                pending = ex
        try:
            call("close", {})
        except BaseException as ex:
            if not isinstance(ex, Exception):
                raise
        if pending is not None:
            raise pending
        return result

    if func == "iter_proc_cmdlines":
        mypid = call("getpid", {})
        ents = call("listdir", {"path": "/proc"})
        out, ex = [], set(inp.get("exclude_pids", []))
        for entry in (ents or []):
            if not s_isdigit(entry): continue
            pid = s_int(entry)
            if pid == mypid or pid in ex: continue
            try:
                cl = call("read_cmdline", {"pid": pid})
            except BaseException as err:
                if isinstance(err, (OSError, PermissionError)):
                    continue
                raise
            out.append([pid, cl.replace("\x00", " ")])
        return out

    if func == "wait_for_tcp_port_free":
        return call("poll_until", {"marker": "_port_free", "host": inp["host"],
                                   "port": inp["port"], "timeout": inp.get("timeout", 10.0),
                                   "interval": 0.1})

    if func == "restart_argv_is_host_gateway":
        argv = inp["argv"]
        if not argv or "gateway" not in argv: return False
        for flag in ("--profile", "-p"):
            if flag in argv:
                idx = argv.index(flag)
                name = argv[idx + 1] if idx + 1 < len(argv) else ""
                return name == "default"
        if any(p == "--profile=default" for p in argv): return True
        if any(p.startswith("--profile=") and not p.endswith("=default") for p in argv): return False
        try:
            if truth(call("is_multiplex_active", {})): return True
        except BaseException as ex:
            if not isinstance(ex, Exception): raise
        try:
            rec = call("hr_read_record", {"role": "gateway"})
            if rec is not None and truth(rec.get("live_owner")) and len(rec.get("profiles") or []) > 1:
                return True
        except BaseException as ex:
            if not isinstance(ex, Exception): raise
        try:
            h = call("get_hermes_home", {})
            d = call("get_default_hermes_root", {})
            return h == d
        except BaseException as ex:
            if not isinstance(ex, Exception): raise
            return False

    if func == "gw_windows":
        return call("import_gateway_windows", {})

    raise ValueError("unhandled shadow func: %s" % func)


# ---------------- normalization / comparison ----------------

def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    if isinstance(o, tuple):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def _fixtures():
    p = _REPO / "tests" / "fixtures" / "gateway_flow3.jsonl"
    return [json.loads(l) for l in p.read_text(encoding="utf-8").split("\n") if l.strip()]


def _setup_live(ns, inp):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}


def self_test(ns):
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    cases = _fixtures()
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(ff - set(PY)), sorted(set(PY) - ff)))
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    names = set(PY.values()) | set(PY)
    for c in cases:
        _setup_live(ns, c["in"])
        try:
            drive(ns, c["func"], c["in"])
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec)); bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec)); bad += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


def shadow_test(ns):
    """Diff a literal Python mirror of the C port against the LIVE drive."""
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    cases = _fixtures()
    install_stubs(ns)
    bad = 0
    for c in cases:
        inp = c["in"]
        _setup_live(ns, inp)
        try:
            ret = drive(ns, c["func"], inp)
            live = {"ret": _numnorm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            live = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        try:
            sh_ret = shadow(c["func"], inp)
            shr = {"ret": _numnorm(sh_ret), "trace": SH["trace"]}
        except BaseException as ex:
            shr = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                   "trace": SH["trace"]}
        if canon(live) != canon(shr):
            bad += 1
            print("SHADOW-DIFF %s" % c["func"])
            print("  LIVE: %s" % json.dumps(live, ensure_ascii=False))
            print("  SH  : %s" % json.dumps(shr, ensure_ascii=False))
    faulthandler.cancel_dump_traceback_later()
    print("SHADOW gateway_flow3: %d cases, %d mismatches" % (len(cases), bad))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(extract())
        return
    if "--shadow-test" in sys.argv:
        shadow_test(extract())
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
        _setup_live(ns, inp)
        try:
            ret = drive(ns, func, inp)
            want = {"ret": _numnorm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE gateway_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
