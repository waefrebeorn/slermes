#!/usr/bin/env python3
"""Oracle for gateway_windows_flow3 — the UNCLAIMED remainder FLOW cores of
hermes_cli/gateway_windows.py (LIVE authority, py3.14 quarry venv), third
slice after gateway_windows_flow / gateway_windows_flow2.

Ports covered: console encodings + schtasks byte decode, service roots,
launcher settings/scripts, dead-launcher refresh + script drift, stdio
probes (_stdout_isatty / _stdin_console_mode_ok), and the deep status probe
cluster (_probe / _probe_missing / _probe_pid_file / _probe_lock_file /
_probe_running_pid / _probe_pid_exists / _probe_state_file / _probe_exit_diag).
Claimed siblings (_schtasks_encoding, _hermes_home, _preserve_hermes_home_path,
_stable_gateway_working_dir, _sanitize_filename, _render_launchers,
get_task_script_path, _assert_windows, _write_launcher_scripts) are SEAMED;
unclaimed ported siblings COMPOSE for real.

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

Input: stdin JSON lines from t_port_gateway_windows_flow3:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE gateway_windows_flow3: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import json
import os
import sys
import types
from pathlib import Path, PurePosixPath, PurePath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gw3_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_windows.py")

# C harness func -> live python name.
PY = {
    "windows_console_encodings": "_windows_console_encodings",
    "decode_schtasks_output": "_decode_schtasks_output",
    "hermes_service_roots": "hermes_service_roots",
    "launcher_settings": "_launcher_settings",
    "launcher_scripts": "_launcher_scripts",
    "refresh_dead_launcher": "refresh_dead_launcher",
    "launcher_script_drift": "launcher_script_drift",
    "stdout_isatty": "_stdout_isatty",
    "stdin_console_mode_ok": "_stdin_console_mode_ok",
    "probe": "_probe",
    "probe_missing": "_probe_missing",
    "probe_pid_file": "_probe_pid_file",
    "probe_lock_file": "_probe_lock_file",
    "probe_running_pid": "_probe_running_pid",
    "probe_pid_exists": "_probe_pid_exists",
    "probe_state_file": "_probe_state_file",
    "probe_exit_diag": "_probe_exit_diag",
}
WANTED = set(PY.values())

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


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
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "JSONDecodeError": json.JSONDecodeError,
    "StopIteration": StopIteration, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError,
    "SystemError": SystemError, "BufferError": BufferError, "EOFError": EOFError,
    "Exception": Exception, "BaseException": BaseException,
    "KeyboardInterrupt": KeyboardInterrupt, "SystemExit": SystemExit,
    "GeneratorExit": GeneratorExit,
}
# multi-arg constructors: cls(msg) mangles; bypass __init__ (py3.14-safe)
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
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def posix(p):
    if p is None:
        return None
    if isinstance(p, PathDouble):
        return p.posix
    if isinstance(p, PurePath):
        return p.as_posix()
    return str(PurePosixPath(p))


class PathDouble:
    """A Path stand-in: PurePosixPath normalization + pathlib-faithful
    name/parent/with_suffix/is_absolute/join, scripted exists/read/resolve.
    The constructor raises the py3.14 TypeError on non-str (probe-verified)."""

    def __init__(self, s):
        if not isinstance(s, str):
            raise TypeError(
                "argument should be a str or an os.PathLike object where "
                "__fspath__ returns a str, not '%s'" % type(s).__name__)
        self.posix = str(PurePosixPath(s))

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix

    def __repr__(self):
        return "PathDouble(%r)" % self.posix

    def __truediv__(self, other):
        return PathDouble(str(PurePosixPath(self.posix) / str(other)))

    @property
    def name(self):
        return PurePosixPath(self.posix).name

    @property
    def parent(self):
        return PathDouble(str(PurePosixPath(self.posix).parent))

    def is_absolute(self):
        return PurePosixPath(self.posix).is_absolute()

    def with_suffix(self, suffix):
        return PathDouble(str(PurePosixPath(self.posix).with_suffix(suffix)))

    def exists(self):
        return seam("path_exists", {"path": self.posix})

    def read_text(self, encoding=None):
        return seam("path_read_text", {"path": self.posix, "encoding": encoding})

    def read_bytes(self):
        return bytes(seam("path_read_bytes", {"path": self.posix}))

    def resolve(self):
        r = seam("path_resolve", {"path": self.posix})
        return PathDouble(r) if r is not None else None


class Kernel32Double:
    """getattr presence gate for the console-CP getters: a missing key raises
    AttributeError (caught by the live loop, no seam); a present-null value
    returns None so the live call raises the NoneType-not-callable TypeError."""

    SEAMS = {"GetConsoleOutputCP": "get_console_output_cp",
             "GetOEMCP": "get_oem_cp", "GetACP": "get_acp"}

    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name not in d:
            raise AttributeError(name)
        if d[name] is None:
            return None
        sn = self.SEAMS[name]
        return lambda: seam(sn, {})


class StdKernel32:
    def GetStdHandle(self, n):
        return seam("get_std_handle", {"std_handle": n})

    def GetConsoleMode(self, handle, ref):
        return seam("get_console_mode", {"handle": handle})


class StdoutDouble:
    def isatty(self):
        return seam("stdout_isatty", {})


class DTDouble:
    def __init__(self, epoch, aware):
        self.epoch = epoch
        self.aware = aware

    def __sub__(self, other):
        if not isinstance(other, DTDouble):
            raise TypeError("unsupported operand type(s) for -: 'DTDouble' and '%s'"
                            % type(other).__name__)
        if self.aware != other.aware:
            raise TypeError("can't subtract offset-naive and offset-aware datetimes")
        return TDDouble(self.epoch - other.epoch)


class TDDouble:
    def __init__(self, seconds):
        self.seconds = seconds

    def total_seconds(self):
        return self.seconds


def _mat_dt(r):
    if isinstance(r, dict) and "epoch" in r:
        return DTDouble(r["epoch"], bool(r.get("aware", False)))
    raise TypeError("datetime double shape")  # inner except-Exception swallow


class FileDouble:
    def __init__(self, data):
        self.data = bytes(data)
        self.pos = 0

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return None  # silent close (family convention)

    def seek(self, off, whence=0):
        if whence == 0:
            self.pos = off
        elif whence == 1:
            self.pos += off
        elif whence == 2:
            self.pos = len(self.data) + off
        return self.pos

    def tell(self):
        return self.pos

    def read(self):
        r = self.data[self.pos:]
        self.pos = len(self.data)
        return r


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return o.posix
    if isinstance(o, PurePath):
        return o.as_posix()
    if isinstance(o, bytes):
        return list(o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


# ── extraction ──

def extract():
    import codecs
    import ctypes
    import logging
    import re
    import time
    import uuid
    from datetime import datetime, timezone, UTC
    src = open(SRC, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.gateway_windows_flow3_extracted",
        "json": json, "os": os, "re": re, "sys": sys, "time": time, "uuid": uuid,
        "ctypes": ctypes, "codecs": codecs,
        "Path": PathDouble, "PurePosixPath": PurePosixPath,
        "datetime": datetime, "timezone": timezone, "UTC": UTC,
        "logging": logging, "logger": RecLogger(),
    }
    tree = ast.parse(src)
    const_nodes = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            const_nodes.append(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            const_nodes.append(node)
    for node in const_nodes:
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # the module's `logger = logging.getLogger(__name__)` Assign overwrites the
    # seeded recorder — re-set it AFTER the constant loop.
    ns["logger"] = RecLogger()
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=flags), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    return ns


# ── recording stubs ──

def rec_print(*args, **kw):
    text = kw.get("sep", " ").join(str(a) for a in args)
    seam("print", {"text": text})


def _path_result(r):
    return PathDouble(r) if r is not None else None


def make_import_module(modname, handlers):
    class M(types.ModuleType):
        def __getattr__(self, name):
            if name in handlers:
                return handlers[name]()
            raise AttributeError(name)
    m = M(modname)
    sys.modules[modname] = m
    return m


def install_stubs(ns):
    ns["print"] = rec_print

    # claimed siblings (SEAMED, one event per live call)
    ns["_schtasks_encoding"] = lambda: seam("schtasks_encoding", {})
    ns["_hermes_home"] = lambda: _path_result(seam("hermes_home", {}))
    ns["_preserve_hermes_home_path"] = lambda value: seam("preserve_hermes_home_path", {"value": value})

    def stable_wd(project_root, home=None):
        return seam("stable_gateway_working_dir",
                    {"project_root": project_root, "home": posix(home)})
    ns["_stable_gateway_working_dir"] = stable_wd
    ns["_sanitize_filename"] = lambda name: seam("sanitize_filename", {"name": name})
    ns["get_task_script_path"] = lambda: _path_result(seam("get_task_script_path", {}))

    def render_launchers(settings, script_path):
        r = seam("render_launchers", {"settings": list(settings),
                                      "script_path": posix(script_path)})
        return [(PathDouble(p), c, e) for p, c, e in r]
    ns["_render_launchers"] = render_launchers
    ns["_assert_windows"] = lambda: seam("assert_windows", {})

    def write_launcher_scripts(scripts):
        return seam("write_launcher_scripts",
                    {"scripts": [[posix(p), c, e] for p, c, e in scripts]})
    ns["_write_launcher_scripts"] = write_launcher_scripts

    # foreign collaborators (module-top bare globals)
    ns["project_root_str"] = lambda: seam("project_root_str", {})

    # json / datetime doubles (seamed leaves)
    ns["json"] = types.SimpleNamespace(loads=lambda s: seam("json_loads", {"s": s}))
    ns["datetime"] = types.SimpleNamespace(
        fromisoformat=lambda s: _mat_dt(seam("datetime_fromisoformat", {"s": s})),
        now=lambda tz=None: DTDouble(seam("datetime_now", {}), True))

    # os double: normcase is the only os effect in the ported defs
    ns["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(normcase=lambda p: seam("normcase", {"path": p})))

    # builtin open -> scripted file double
    def open_stub(path, mode="r", *a, **k):
        r = seam("open", {"path": posix(path), "mode": mode})
        return FileDouble(r["data"])
    ns["open"] = open_stub

    # in-function imports: module attribute access fires the import seam
    for pkg in ("hermes_cli", "gateway"):
        if pkg not in sys.modules:
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m

    def h_project_root():
        return seam("import_project_root", {})

    def h_profile_arg():
        seam("import_profile_arg", {})
        return lambda hh: seam("profile_arg", {"hermes_home": hh})

    def h_get_python_path():
        seam("import_get_python_path", {})
        return lambda: seam("get_python_path", {})

    make_import_module("hermes_cli.gateway", {
        "PROJECT_ROOT": h_project_root,
        "_profile_arg": h_profile_arg,
        "get_python_path": h_get_python_path,
    })

    def h_lock_active():
        seam("import_is_gateway_runtime_lock_active", {})
        return lambda lock_path: seam("is_gateway_runtime_lock_active",
                                      {"lock_path": posix(lock_path)})

    def h_get_running_pid():
        seam("import_get_running_pid", {})

        def get_running_pid(cleanup_stale=True):
            return seam("get_running_pid", {"cleanup_stale": cleanup_stale})
        return get_running_pid

    def h_pid_exists():
        seam("import_pid_exists", {})
        return lambda pid: seam("pid_exists", {"pid": pid})

    make_import_module("gateway.status", {
        "is_gateway_runtime_lock_active": h_lock_active,
        "get_running_pid": h_get_running_pid,
        "_pid_exists": h_pid_exists,
    })

    def h_vbs_identity():
        seam("import_vbs_identity", {})

        def vbs_identity(text):
            r = seam("vbs_identity", {"text": text})
            return tuple(r) if isinstance(r, list) else r
        return vbs_identity

    def h_launcher_text():
        seam("import_launcher_text", {})
        return lambda definition: seam("launcher_text", {"definition": definition})

    def h_read_definition():
        seam("import_read_definition", {})
        return lambda path: seam("read_definition", {"path": posix(path)})

    make_import_module("hermes_cli.gateway_runtime_service_identity", {
        "_vbs_identity": h_vbs_identity,
        "launcher_text": h_launcher_text,
        "read_definition": h_read_definition,
    })


# ── drive ──

def _ctypes_ns(kernel32):
    return types.SimpleNamespace(
        windll=types.SimpleNamespace(kernel32=kernel32),
        byref=lambda x: x, c_ulong=lambda: 0)


def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    f = ns[PY[func]]
    if func == "windows_console_encodings":
        k = inp.get("kernel32")
        ns["ctypes"] = _ctypes_ns(Kernel32Double(k) if isinstance(k, dict) else None)
        return f()
    if func == "decode_schtasks_output":
        k = inp.get("kernel32")
        ns["ctypes"] = _ctypes_ns(Kernel32Double(k) if isinstance(k, dict) else None)
        return f(bytes(inp.get("data", [])))
    if func == "hermes_service_roots":
        ns["sys"] = types.SimpleNamespace(executable=inp.get("executable"))
        return f()
    if func == "launcher_settings":
        home = PathDouble(inp["home"]) if inp.get("home") is not None else None
        return f(home)
    if func == "launcher_scripts":
        return f()
    if func == "refresh_dead_launcher":
        home = PathDouble(inp["home"]) if inp.get("home") is not None else None
        return f(home, inp.get("task_name", ""))
    if func == "launcher_script_drift":
        return f()
    if func == "stdout_isatty":
        so = inp.get("stdout")
        if isinstance(so, dict) and so.get("__stdout__"):
            so = StdoutDouble()
        ns["sys"] = types.SimpleNamespace(stdout=so)
        return f()
    if func == "stdin_console_mode_ok":
        ns["sys"] = types.SimpleNamespace(platform=inp.get("platform", "win32"))
        k = inp.get("kernel32")
        ns["ctypes"] = _ctypes_ns(StdKernel32() if k is not None else None)
        return f()
    if func == "probe":
        f(inp.get("index", 0), inp.get("ok"), inp.get("message", ""))
        return None
    if func == "probe_missing":
        return f(inp.get("index", 0), PathDouble(inp["path"]), inp.get("label", ""))
    if func == "probe_pid_file":
        return f(PathDouble(inp["pid_path"]))
    if func == "probe_lock_file":
        f(PathDouble(inp["lock_path"]))
        return None
    if func == "probe_running_pid":
        return f()
    if func == "probe_pid_exists":
        f(inp.get("candidate_pid"))
        return None
    if func == "probe_state_file":
        f(PathDouble(inp["state_path"]))
        return None
    if func == "probe_exit_diag":
        f(PathDouble(inp["diag_path"]))
        return None
    raise ValueError("unhandled func: %s" % func)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def self_test(ns):
    """Drive every fixture case live (no C): surface binding bugs / script gaps."""
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "gateway_windows_flow3.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").split("\n") if l.strip()]
    fixture_funcs = {c["func"] for c in cases}
    if fixture_funcs != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(fixture_funcs - set(PY)), sorted(set(PY) - fixture_funcs)))
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    names = set(PY.values()) | set(PY)
    for c in cases:
        inp = c["in"]
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        try:
            drive(ns, c["func"], inp)
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
            want = {"ret": norm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE gateway_windows_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_windows_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()