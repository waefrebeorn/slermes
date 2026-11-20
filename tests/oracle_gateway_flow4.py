#!/usr/bin/env python3
"""Oracle for gateway_flow4 -- the UNCLAIMED-remainder FLOW cores of
hermes_cli/gateway.py (LIVE authority, py3.14 quarry venv) beyond the flow3
PoP wave.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the
quarry (no heavy import), module constants seeded, every effect replaced by a
recording stub returning the SAME scripted results the C harness feeds its
universal seam. Compares the ordered DECISION SEQUENCE (trace of seam calls)
plus the return value. Scripted {"__raise__": cls} raises a REAL exception of
that class; drive() catches BaseException so KeyboardInterrupt/SystemExit/
GeneratorExit pass through live except-Exception clauses exactly as the C port
propagates them. A MISSING seam key fail-closes (RuntimeError) on both sides;
a scripted null is a valid None result.

Input: stdin JSON lines from t_port_gateway_flow4:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE gateway_flow4: N cases, M mismatches. Fail-closed (exit 2)
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
from pathlib import Path as _Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = _Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gf4_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway.py")

PY = {
    "cmd_run": "_cmd_run",
    "wait_for_api_server_port_free": "_wait_for_api_server_port_free",
    "pm_managed_node_dirs": "_pm_managed_node_dirs",
    "native_service_homes": "_native_service_homes",
    "windows_process_listing": "_windows_process_listing",
    "s6_gateway_snapshot": "_s6_gateway_snapshot",
    "reaper_exclusion_pids": "_reaper_exclusion_pids",
    "apply_startup_watchdog_config": "_apply_startup_watchdog_config",
    "exit_diag": "_make_exit_diag",
    "respawn_storm_backoff": "_respawn_storm_backoff",
    "install_systemd_from_cli": "_install_systemd_from_cli",
    "legacy_launchd_labels_for_install": "legacy_launchd_labels_for_install",
    "prepare_service_launcher": "_prepare_service_launcher",
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
        cls = type("BaseExceptionGroup", (BaseExceptionGroup,),
                   {"__str__": lambda self: getattr(self, "_gf4_msg", "")})
        e = cls(msg, [KeyboardInterrupt("x")])
        e._gf4_msg = msg
        return e
    if name == "ExceptionGroup":
        return cls(msg, [Exception("x")])
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def truth(v):
    if v is None: return False
    if isinstance(v, bool): return v
    if isinstance(v, (int, float)): return v != 0
    if isinstance(v, str): return v != ""
    if isinstance(v, (list, dict, tuple, set)): return len(v) > 0
    return False


# ---------------- extraction ----------------
def extract():
    src = _Path(SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    ns = {"__name__": "hermes_cli.gateway", "__builtins__": builtins,
          "math": math, "json": json, "re": re,
          "Path": _Path, "set": set, "list": list, "tuple": tuple,
          "UTC": "UTC"}
    class _RS: pass
    ns["GatewayRuntimeSnapshot"] = _RS
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in targets):
                continue  # never exec subscript/attribute targets (env mutation)
            try:
                exec(compile(ast.get_source_segment(src, node), SRC, "exec"), ns)
            except BaseException:
                pass
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
def _mod(name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    return m


class PathD:
    def __init__(self, s): self.s = str(s)
    def __truediv__(self, other): return PathD(self.s.rstrip("/") + "/" + str(other))
    def __str__(self): return self.s
    def __repr__(self): return "PathD(%r)" % self.s
    def resolve(self): return self
    def expanduser(self): return self
    def is_dir(self): return seam("is_dir", {"path": self.s})
    def mkdir(self, parents=False, exist_ok=False):
        return seam("mkdir", {"path": self.s, "parents": parents, "exist_ok": exist_ok})
    def read_bytes(self): return seam("read_plist", {"path": self.s})
    def glob(self, pattern):
        v = seam("glob", {"path": self.s, "pattern": pattern})
        paths = v["paths"] if isinstance(v, dict) else (v or [])
        return [PathD(x) for x in paths]
    def relative_to(self, root):
        v = seam("relative_to", {"pinned": self.s, "root": str(root)})
        parts = v["parts"] if isinstance(v, dict) else (v or [])
        return types.SimpleNamespace(parts=tuple(parts))
    @property
    def parent(self):
        i = self.s.rfind("/")
        return PathD(self.s[:i] if i >= 0 else "")


class ResolvedDouble:
    def __init__(self, s): self.s = s
    def resolve(self): return self.s


class StdinDouble:
    def __init__(self, v): self.v = v
    def isatty(self): return self.v  # driven by input, NOT a seam


class DirDouble:
    def __init__(self, s): self.s = s
    def __truediv__(self, other): return DirDouble(self.s.rstrip("/") + "/" + str(other))
    def is_dir(self): return seam("dir_isdir", {"path": self.s})

class MgrDouble:
    def __getattr__(self, name):
        if name == "scandir":
            v = seam("mgr_scandir", {})
            return None if v is None else DirDouble(v)
        raise AttributeError(name)
    def is_running(self, name): return seam("mgr_is_running", {"service": name})


class ProcDouble:
    def __init__(self, pid): self.pid = pid
    def parent(self):
        v = seam("psutil_parent", {"pid": self.pid})
        return None if v is None else ProcDouble(v)


class FileDouble:
    def __enter__(self): return self
    def __exit__(self, *a): return False
    def write(self, text): return seam("write", {"text": text})


class EnvDict(dict):
    """os.environ-like dict whose __setitem__ records a seam."""
    def __setitem__(self, k, v):
        seam("setenv", {"name": k, "value": v})
        super().__setitem__(k, v)


def _print_stub(*a, **k):
    seam("print", {"text": " ".join(str(x) for x in a)})


def install_stubs(ns):
    ns["print"] = _print_stub
    # --- hermes_cli package + submodules ---
    hp = _mod("hermes_cli") if "hermes_cli" not in sys.modules else sys.modules["hermes_cli"]
    # _subprocess_compat
    sc = _mod("hermes_cli._subprocess_compat")
    def _bpr(cmd, timeout=None, errors=None):
        kind = "powershell" if ("-NoProfile" in cmd) else "wmic"
        return _obj(seam("bounded_probe_run", {"kind": kind, "timeout": timeout, "errors": errors}))
    sc.bounded_probe_run = _bpr
    hp._subprocess_compat = sc
    # service_manager
    sm = _mod("hermes_cli.service_manager")
    sm.detect_service_manager = lambda: seam("detect_manager", {})
    sm.get_service_manager = lambda: (seam("get_manager", {}), MgrDouble())[1]
    hp.service_manager = sm
    # config
    hcfg = _mod("hermes_cli.config")
    hcfg.load_config = lambda: seam("load_config", {})
    hp.config = hcfg
    # _launchers
    hl = _mod("hermes_cli._launchers")
    hl.ENTRY_POINTS = ["a", "b"]
    hl.ensure_install_launchers = lambda root, local: seam(
        "ensure_launchers", {"root": str(root), "local": str(local)})["paths"]
    hl.publishes_service_launcher = lambda root: seam("publishes", {"root": str(root)})
    hp._launchers = hl
    # hermes_constants
    hc = _mod("hermes_constants") if "hermes_constants" not in sys.modules else sys.modules["hermes_constants"]
    hc.account_default_hermes_home = lambda: ResolvedDouble(seam("account_home", {}))
    hc.sudo_invoker_default_home = lambda: _maybe_resolved(seam("sudo_home", {}))
    hc.get_default_hermes_root = lambda: ResolvedDouble(seam("root", {}))
    hc.get_hermes_home = lambda: PathD(seam("get_hermes_home", {}))
    hc.set_hermes_home_override = lambda home: seam("set_override", {"home": str(home)})
    hc.reset_hermes_home_override = lambda token: seam("reset_override", {"token": token})
    # --- gateway package + submodules ---
    gp = _mod("gateway") if "gateway" not in sys.modules else sys.modules["gateway"]
    gcfg = _mod("gateway.config")
    gcfg.Platform = types.SimpleNamespace(API_SERVER="api_server")
    gp.config = gcfg
    gplat = _mod("gateway.platforms")
    gas = _mod("gateway.platforms.api_server")
    gas.listen_address = lambda extra: (lambda d: [d["host"], d["port"]])(seam("listen_address", {"extra": extra}))
    gplat.api_server = gas
    gp.platforms = gplat
    gstat = _mod("gateway.status")
    gstat._read_pid_record = lambda: seam("read_pid_record", {})
    gstat._read_gateway_lock_record = lambda: seam("read_lock_record", {})
    gstat._pid_from_record = lambda rec: seam("pid_from_record", {"rec": rec})
    gstat.get_running_pid = lambda cleanup_stale=False: seam("running_pid", {"cleanup_stale": cleanup_stale})
    gstat.record_start_and_check_storm = lambda max_starts, window_s: _obj(seam(
        "record_storm", {"max_starts": max_starts, "window_s": window_s}))
    gp.status = gstat
    # --- other modules ---
    _mod("pm") if "pm" not in sys.modules else None
    pm = _mod("pm.lock")
    class Facts:
        def __init__(self, path): seam("Facts_new", {"path": str(path)})
        def env_for(self, name, store): return seam("env_for", {"name": name, "store": str(store)})
    pm.Facts = Facts
    plist = _mod("plistlib")
    plist.loads = lambda data: data
    pwd = _mod("pwd") if "pwd" not in sys.modules else sys.modules["pwd"]
    pwd.getpwuid = lambda uid: types.SimpleNamespace(pw_dir=seam("pw_dir", {}))
    pwd.getpwnam = lambda name: types.SimpleNamespace(pw_gid=seam("pwnam_gid", {"username": name}))
    sw = _mod("hermes_startup_watchdog")
    sw.ENV_STARTUP_WATCHDOG = "HERMES_STARTUP_WATCHDOG"
    sw.ENV_STARTUP_WATCHDOG_TIMEOUT_S = "HERMES_STARTUP_WATCHDOG_TIMEOUT_S"
    sw.startup_watchdog_disabled = lambda: seam("sw_disabled", {})
    sw.disarm_startup_watchdog = lambda: seam("sw_disarm", {})
    sw.arm_startup_watchdog = lambda: seam("sw_arm", {})
    sw.kick_startup_watchdog = lambda extra_s=None: seam("kick_watchdog", {"extra_s": extra_s})
    psutil = _mod("psutil")
    psutil.Process = lambda pid: ProcDouble(pid)
    dtmod = _mod("datetime")
    class _DT:
        @staticmethod
        def now(tz): return types.SimpleNamespace(isoformat=lambda: seam("timestamp", {}))
    dtmod.datetime = _DT
    dtmod.timezone = types.SimpleNamespace(utc="UTC")
    dtmod.UTC = "UTC"
    timemod = _mod("time")
    timemod.sleep = lambda s: seam("sleep", {"seconds": s})
    # --- namespace globals ---
    ns["contextlib"] = __import__("contextlib")
    ns["Path"] = PathD
    ns["get_hermes_home"] = hc.get_hermes_home
    ns["PROJECT_ROOT"] = PathD("/proj")
    ns["_current_profile_name"] = lambda: seam("profile_name", {})
    ns["_get_service_pids"] = lambda all_profiles=False: seam("service_pids", {"all_profiles": all_profiles})
    ns["GatewayRuntimeSnapshot"] = lambda **kw: kw
    ns["EXTERNAL_GATEWAY_SUPERVISOR_ENV"] = "HERMES_EXTERNAL_GATEWAY_SUPERVISOR"
    ns["_maybe_redirect_run_to_s6_supervision"] = lambda args: seam("redirect_s6", {})
    ns["run_gateway"] = lambda verbose, quiet=False, replace=False, force=False, idle_exit=False: seam(
        "run_gateway", {"verbose": verbose, "quiet": quiet, "replace": replace,
                        "force": force, "idle_exit": idle_exit})

    class AttrDict(dict):
        def __getattr__(self, k):
            try: return self[k]
            except KeyError: raise AttributeError(k)
    def _deep_obj(v):
        if isinstance(v, dict): return AttrDict({k: _deep_obj(x) for k, x in v.items()})
        if isinstance(v, list): return [_deep_obj(x) for x in v]
        return v
    ns["load_gateway_config"] = lambda: _deep_obj(seam("load_config", {}))
    ns["_wait_for_tcp_port_free"] = lambda host, port, timeout=10.0: seam(
        "wait_port", {"host": host, "port": port, "timeout": timeout})
    ns["_system_service_identity"] = lambda run_as_user: tuple(seam("sys_identity", {"run_as_user": run_as_user})["values"])
    ns["_remap_path_for_user"] = lambda root, home_dir: seam("remap_path", {"root": root, "home_dir": home_dir})
    ns["_hermes_home_for_target_user"] = lambda home_dir: seam("target_home", {"home_dir": home_dir})
    ns["is_wsl"] = lambda: seam("is_wsl", {})
    ns["print_warning"] = lambda text: seam("print_warning", {"text": text})
    ns["_print_info_lines"] = lambda *lines: seam("info_lines", {"lines": list(lines)})
    ns["prompt_yes_no"] = lambda question, default=True: seam("prompt_yes_no", {"question": question, "default": default})
    ns["systemd_install"] = lambda **kw: seam("systemd_install", kw)
    ns["systemd_start"] = lambda **kw: seam("systemd_start", kw)
    ns["shutil"] = types.SimpleNamespace(which=lambda name: seam("which", {"name": name}))
    ns["logger"] = types.SimpleNamespace(
        warning=lambda fmt, *args: seam("logger_warning", {"fmt": fmt, "args": list(args)}),
        debug=lambda fmt, *args: seam("logger_debug", {"fmt": fmt, "args": list(args)}),
        info=lambda *a, **k: None, error=lambda *a, **k: None)
    ns["time"] = timemod
    ns["json"] = types.SimpleNamespace(dumps=lambda line, default=None: seam("json_dumps", {"line": line}))
    ns["sys"] = types.SimpleNamespace(stdin=None, version="3.14.9 (main)", platform="win32")


def _obj(spec):
    if isinstance(spec, dict): return types.SimpleNamespace(**spec)
    return spec


def _maybe_resolved(v):
    return ResolvedDouble(v) if v is not None else None


def _env(inp):
    return EnvDict(inp.get("env") or {})


def drive(ns, func, inp):
    f = ns[PY[func]]
    if func == "cmd_run":
        a = dict(inp.get("args") or {})
        for k, d in (("verbose",0),("quiet",False),("replace",False),("force",False),
                     ("idle_exit",False),("external_supervisor",False)):
            a.setdefault(k, d)
        ns["os"] = types.SimpleNamespace(environ=_env(inp))
        return f(types.SimpleNamespace(**a))
    if func == "wait_for_api_server_port_free":
        return f(timeout=inp.get("timeout", 10.0))
    if func == "pm_managed_node_dirs":
        return sorted(f(inp["home"]))
    if func == "native_service_homes":
        return sorted(f())
    if func == "windows_process_listing":
        return f()
    if func == "s6_gateway_snapshot":
        return f(list(inp.get("gateway_pids", [])))
    if func == "reaper_exclusion_pids":
        ns["os"] = types.SimpleNamespace(getpid=lambda: seam("getpid", {}))
        return sorted(f(set(inp.get("extra_exclude", []))))
    if func == "apply_startup_watchdog_config":
        ns["os"] = types.SimpleNamespace(environ=_env(inp))
        return f()
    if func == "exit_diag":
        ns["os"] = types.SimpleNamespace(
            environ=dict(inp.get("env") or {}), getpid=lambda: seam("getpid", {}))
        orig_open = builtins.open
        builtins.open = lambda *a, **k: FileDouble()
        try:
            diag = f()
            return diag(inp.get("tag", ""), **(inp.get("extra") or {}))
        finally:
            builtins.open = orig_open
    if func == "respawn_storm_backoff":
        ns["os"] = types.SimpleNamespace(environ=dict(inp.get("env") or {}))
        ns["sys"] = types.SimpleNamespace(stdin=None, version="3.14.9", platform="win32")
        return f()
    if func == "install_systemd_from_cli":
        kind = inp.get("stdin_kind", "none")
        ns["sys"].stdin = (StdinDouble(inp.get("stdin_isatty", True)) if kind == "tty"
                           else (object() if kind == "no_isatty" else None))
        return f(types.SimpleNamespace(**(inp.get("args") or {})),
                 force=inp.get("force", False), system=inp.get("system", False),
                 run_as_user=inp.get("run_as_user", None))
    if func == "legacy_launchd_labels_for_install":
        ns["os"] = types.SimpleNamespace(getuid=lambda: 0)
        return f(tuple(inp.get("exclude", ())))
    if func == "prepare_service_launcher":
        ns["os"] = types.SimpleNamespace(chown=lambda path, uid, gid: seam(
            "chown", {"path": str(path), "uid": uid, "gid": gid}))
        return f(system=inp.get("system", False), run_as_user=inp.get("run_as_user", None))
    raise ValueError("unhandled func: %s" % func)


# ---------------- shadow mirror of the C port ----------------
def shadow(func, inp):
    SH["scripts"] = inp.get("seams", {})
    SH["trace"] = []
    SH["idx"] = {}

    def call(name, args): return shadow_seam(name, args)

    if func == "cmd_run":
        r = call("redirect_s6", {})
        if truth(r): return None
        a = dict(inp.get("args") or {})
        a.setdefault("external_supervisor", False)
        if truth(a["external_supervisor"]):
            call("setenv", {"name": "HERMES_EXTERNAL_GATEWAY_SUPERVISOR", "value": "1"})
        call("run_gateway", {"verbose": a.get("verbose", 0), "quiet": a.get("quiet", False),
                             "replace": a.get("replace", False), "force": a.get("force", False),
                             "idle_exit": a.get("idle_exit", False)})
        return None

    if func == "wait_for_api_server_port_free":
        cfg = call("load_config", {})
        pc = (cfg or {}).get("platforms", {}).get("api_server")
        if pc is None or not truth(pc.get("enabled")): return True
        la = call("listen_address", {"extra": pc.get("extra") or {}})
        host, port = la["host"], la["port"]
        freed = call("wait_port", {"host": host, "port": port, "timeout": inp.get("timeout", 10.0)})
        if not truth(freed):
            ps = str(port) if not isinstance(port, float) or not port.is_integer() else str(int(port))
            call("print", {"text": "⚠ %s:%s still accepting connections — new api_server may fail to bind" % (host, ps)})
        return freed

    if func == "pm_managed_node_dirs":
        home = inp["home"]
        store = home.rstrip("/") + "/tools"
        facts = call("Facts_new", {"path": store + "/facts.json"})
        out = []
        for name in ("node", "npm", "npx"):
            env = call("env_for", {"name": name, "store": store})
            for v in (env or {}).get("PATH") or []:
                if not v: continue
                if truth(call("is_dir", {"path": v})): out.append(v)
        return sorted(out)

    if func == "native_service_homes":
        h = call("account_home", {})
        su = call("sudo_home", {})
        out = [h]
        if su is not None: out.append(su)
        return sorted(out)

    if func == "windows_process_listing":
        wmic = call("which", {"name": "wmic"})
        result = None
        if wmic is not None:
            result = call("bounded_probe_run", {"kind": "wmic", "timeout": 10, "errors": "ignore"})
        bad = (result is None or result.get("returncode", -1) != 0 or not truth(result.get("stdout")))
        if bad:
            ps = call("which", {"name": "powershell"})
            if not truth(ps):
                pw = call("which", {"name": "pwsh"})
                if not truth(pw): return None
            result = call("bounded_probe_run", {"kind": "powershell", "timeout": 15, "errors": "ignore"})
            if result is None: return None
        if result.get("returncode", -1) != 0 or result.get("stdout") is None: return None
        return result["stdout"]

    if func == "s6_gateway_snapshot":
        if call("detect_manager", {}) != "s6": return None
        svc = "gateway-%s" % call("profile_name", {})
        call("get_manager", {})
        installed = running = False
        try:
            sd = call("mgr_scandir", {})
            if sd is not None:
                installed = truth(call("dir_isdir", {"path": sd.rstrip("/") + "/" + svc}))
        except Exception: installed = False
        if installed:
            try: running = truth(call("mgr_is_running", {"service": svc}))
            except Exception: running = False
        return {"manager": "s6 (container supervisor)", "service_installed": installed,
                "service_running": running, "gateway_pids": inp.get("gateway_pids", []),
                "service_scope": "s6"}

    if func == "reaper_exclusion_pids":
        out = [call("getpid", {})] + list(inp.get("extra_exclude", []))
        try:
            sp = call("service_pids", {"all_profiles": True})
            out.extend(sp or [])
        except Exception: pass
        try:
            r1 = call("read_pid_record", {}); r2 = call("read_lock_record", {})
            pids = [call("pid_from_record", {"rec": r1}), call("pid_from_record", {"rec": r2}),
                    call("running_pid", {"cleanup_stale": False})]
            for rec in pids:
                if rec is None or rec == 0 or rec <= 0: continue
                out.append(rec); cur = rec
                for _ in range(64):
                    par = call("psutil_parent", {"pid": cur})
                    if par is None: break
                    out.append(par); cur = par
        except Exception: pass
        return sorted(set(out))

    if func == "apply_startup_watchdog_config":
        bridged = False
        env = dict(inp.get("env") or {})
        try:
            cfg = call("load_config", {})
            gw = (cfg or {}).get("gateway") or {}
            if "HERMES_STARTUP_WATCHDOG" not in env and not truth(gw.get("startup_watchdog", True)):
                call("setenv", {"name": "HERMES_STARTUP_WATCHDOG", "value": "0"})
                env["HERMES_STARTUP_WATCHDOG"] = "0"
            tmo = gw.get("startup_watchdog_timeout_seconds")
            if "HERMES_STARTUP_WATCHDOG_TIMEOUT_S" not in env and tmo is not None:
                sv = str(int(tmo)) if isinstance(tmo, float) and tmo.is_integer() else str(tmo)
                call("setenv", {"name": "HERMES_STARTUP_WATCHDOG_TIMEOUT_S", "value": sv})
                env["HERMES_STARTUP_WATCHDOG_TIMEOUT_S"] = sv
                bridged = True
        except Exception: pass
        try:
            if truth(call("sw_disabled", {})): call("sw_disarm", {})
            else:
                if bridged: call("sw_disarm", {})
                call("sw_arm", {})
        except Exception: pass
        return None

    if func == "exit_diag":
        env = inp.get("env") or {}
        if env.get("HERMES_GATEWAY_EXIT_DIAG", "1") != "1": return None
        try:
            h = call("get_hermes_home", {})
            call("mkdir", {"path": h.rstrip("/") + "/logs", "parents": True, "exist_ok": True})
            line = {"ts": call("timestamp", {}), "tag": inp.get("tag", ""),
                    "pid": call("getpid", {}), "python": inp.get("python"),
                    "platform": inp.get("platform")}
            line.update(inp.get("extra") or {})
            txt = call("json_dumps", {"line": line})
            call("write", {"text": txt + "\n"})
        except Exception: pass
        return None

    if func == "respawn_storm_backoff":
        max_starts, win = 5, 120.0
        try:
            cfg = call("load_config", {})
            gw = cfg.get("gateway") if isinstance(cfg, dict) else None
            rs = gw.get("respawn_storm") if isinstance(gw, dict) else None
            if isinstance(rs, dict):
                if isinstance(rs.get("max_starts"), int): max_starts = rs["max_starts"]
                if isinstance(rs.get("window_seconds"), (int, float)): win = float(rs["window_seconds"])
        except Exception: pass
        env = inp.get("env") or {}
        try: max_starts = int(env["HERMES_GATEWAY_MAX_STARTS"])
        except (KeyError, ValueError): pass
        try: win = float(env["HERMES_GATEWAY_START_WINDOW_S"])
        except (KeyError, ValueError): pass
        try:
            storm = call("record_storm", {"max_starts": max_starts, "window_s": win}) if max_starts > 0 else None
            if storm is not None:
                call("logger_warning", {
                    "fmt": "Gateway (re)started %d times in %.0fs — backing off %.0fs to break a respawn storm.",
                    "args": [storm["count"], storm["window_s"], storm["backoff_s"]]})
                try: call("kick_watchdog", {"extra_s": storm["backoff_s"]})
                except Exception: pass
                call("sleep", {"seconds": storm["backoff_s"]})
        except Exception as be:
            call("logger_debug", {"fmt": "respawn-storm breaker check failed (non-fatal): %s", "args": [str(be)]})
        return None

    if func == "install_systemd_from_cli":
        if truth(call("is_wsl", {})):
            call("print_warning", {"text": "WSL detected — systemd services may not survive WSL restarts."})
            call("info_lines", {"lines": [
                "  Consider running in foreground instead: hermes gateway run",
                "  Or use tmux/screen for persistence: tmux new -s hermes 'hermes gateway run'"]})
            call("print", {"text": ""})
        kind = inp.get("stdin_kind", "none")
        non_interactive = True
        if kind == "tty": non_interactive = not truth(inp.get("stdin_isatty", True))
        args = inp.get("args") or {}
        def flag(name, question):
            v = args.get(name)
            if v is not None: return v
            if non_interactive: return True
            return call("prompt_yes_no", {"question": question, "default": True})
        start_now = flag("start_now", "Start the gateway now after installing the service?")
        start_on_login = flag("start_on_login", "Start the gateway automatically on login/boot with systemd?")
        call("systemd_install", {"force": inp.get("force", False), "system": inp.get("system", False),
                                 "run_as_user": inp.get("run_as_user"),
                                 "enable_on_startup": start_on_login,
                                 "non_interactive": non_interactive,
                                 "force_unit_path": args.get("force_unit_path", False)})
        if truth(start_now): call("systemd_start", {"system": inp.get("system", False)})
        return None

    if func == "legacy_launchd_labels_for_install":
        try:
            home = call("pw_dir", {})
            root = call("root", {})
        except Exception: return []
        agents = home.rstrip("/") + "/Library/LaunchAgents"
        if not truth(call("is_dir", {"path": agents})): return []
        gl = call("glob", {"path": agents, "pattern": "ai.hermes.gateway*.plist"})
        paths = sorted((gl["paths"] if isinstance(gl, dict) else (gl or [])))
        excl = inp.get("exclude", ())
        labels = set()
        for path in paths:
            try:
                data = call("read_plist", {"path": path})
                label = data["Label"]
                pinned = str(data["EnvironmentVariables"]["HERMES_HOME"])
                parts = call("relative_to", {"pinned": pinned, "root": root})
                parts = parts["parts"] if isinstance(parts, dict) else (parts or [])
                rel = tuple(parts)
            except Exception: continue
            if not isinstance(label, str) or label in excl or not label.startswith("ai.hermes.gateway"): continue
            if not rel or (len(rel) == 2 and rel[0] == "profiles"): labels.add(label)
        return sorted(labels)

    if func == "prepare_service_launcher":
        root, home = "/proj", call("get_hermes_home", {})
        owner = None
        if truth(inp.get("system", False)):
            ident = call("sys_identity", {"run_as_user": inp.get("run_as_user")})
            ident = ident["values"] if isinstance(ident, dict) else ident
            owner = (ident[3], ident[0])
            root = call("remap_path", {"root": root, "home_dir": ident[2]})
            home = call("target_home", {"home_dir": ident[2]})
        token = call("set_override", {"home": home})
        try:
            if not truth(call("publishes", {"root": root})): return None
            local = root.rstrip("/") + "/.hermes/bin"
            el = call("ensure_launchers", {"root": root, "local": local})
            paths = el["paths"] if isinstance(el, dict) else []
            if len(paths) != 2:
                raise make_exc({"__raise__": "RuntimeError", "msg": "Could not publish the gateway installation launcher"})
            if owner is not None:
                uid, username = owner
                gid = call("pwnam_gid", {"username": username})
                seq = [local.rsplit("/", 1)[0], local] + list(paths)
                for path in seq:
                    call("chown", {"path": path, "uid": uid, "gid": gid})
        finally:
            call("reset_override", {"token": token})
        return None

    raise ValueError("unhandled shadow func: %s" % func)


# ---------------- normalization / comparison ----------------
def _numnorm(o):
    if type(o) is float and o.is_integer(): return int(o)
    if isinstance(o, dict): return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)): return [_numnorm(v) for v in o]
    return o

def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))

def _fixtures():
    p = _REPO / "tests" / "fixtures" / "gateway_flow4.jsonl"
    return [json.loads(l) for l in p.read_text(encoding="utf-8").split("\n") if l.strip()]

def _setup_live(ns, inp):
    CUR["in"] = inp; CUR["trace"] = []; CUR["scripts"] = inp.get("seams", {}); CUR["idx"] = {}

def self_test(ns):
    cases = _fixtures()
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s" % (sorted(ff-set(PY)), sorted(set(PY)-ff)))
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    for c in cases:
        _setup_live(ns, c["in"])
        try: drive(ns, c["func"], c["in"])
        except BaseException as ex:
            if type(ex).__name__ in ("AttributeError", "NameError"):
                print("SELFTEST BINDING %s: %s: %s" % (c["func"], type(ex).__name__, ex)); bad += 1
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)

def shadow_test(ns):
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
            live = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)}, "trace": CUR["trace"]}
        try:
            sr = shadow(c["func"], inp)
            shr = {"ret": _numnorm(sr), "trace": SH["trace"]}
        except BaseException as ex:
            shr = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)}, "trace": SH["trace"]}
        if canon(live) != canon(shr):
            bad += 1
            print("SHADOW-DIFF %s" % c["func"])
            print("  LIVE: %s" % json.dumps(live, ensure_ascii=False, default=str))
            print("  SH  : %s" % json.dumps(shr, ensure_ascii=False, default=str))
    print("SHADOW gateway_flow4: %d cases, %d mismatches" % (len(cases), bad))
    sys.exit(1 if bad else 0)

def main():
    if "--self-test" in sys.argv: self_test(extract()); return
    if "--shadow-test" in sys.argv: shadow_test(extract()); return
    ns = extract(); install_stubs(ns)
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
        try: record = json.loads(line)
        except json.JSONDecodeError: continue
        func = record.get("func"); inp = record.get("in") or {}; got = record.get("out") or {}
        cases += 1
        _setup_live(ns, inp)
        try:
            ret = drive(ns, func, inp)
            want = {"ret": _numnorm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)}, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE gateway_flow4: 0 cases (fail-closed)"); sys.exit(2)
    print("ORACLE gateway_flow4: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)

if __name__ == "__main__":
    main()
