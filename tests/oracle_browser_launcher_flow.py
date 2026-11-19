#!/usr/bin/env python3
"""Oracle for src/tools/browser_launcher_flow.c — drives the LIVE
tools/browser_launcher.py launch-decision defs under IDENTICAL scripted seams.

The 18 targets (+ the pure _ephemeral_root helper and the LaunchedBrowser
dataclass) are AST-extracted in source order; module Assign/AnnAssign
constants are seeded (plain-Name targets only); every node is compiled with
the PEP 563 future flag so annotations stay strings, and the exec namespace
is a registered module (py3.14 dataclasses needs sys.modules[__name__]).

All live I/O is replaced by recording stubs consuming the SAME "io" scripts
the C harness feeds its universal seam: absent/null -> None on every call;
{"seq": [...]} -> per-call sequence (None when exhausted); any other value ->
reused. Each call records {ev, args, ret} (ret = RAW scripted value) in call
order. Comparison: ordered trace + ret + raise-spec + final state
(served_homes set, sorted). Path-bearing args/results are posix-normalized
(Path.as_posix / posixpath.join) to match the C port's posix path math.

Input: stdin JSON lines from t_port_browser_launcher_flow:
    {"func": ..., "in": {...}, "out": {"ret","raise","trace","state"}}
Summary: ORACLE browser_launcher_flow: N cases, M mismatches.
Fail-closed: exit 1 on any mismatch, on a PYERR, or when zero cases ran.
"""
import ast
import base64
import contextlib
import dataclasses
import hashlib
import json
import os
import posixpath
import re
import socket
import stat
import sys
import threading
import types
import __future__
from dataclasses import dataclass, field
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tools", "browser_launcher.py")

WANTED = {
    "running_in_docker",
    "apparmor_restricts_unprivileged_userns",
    "needs_chromium_sandbox_bypass",
    "_explicit_executable_configured",
    "_resolve_executable",
    "chromium_ready",
    "_launch_timeout",
    "_resolve_headed",
    "_spawn_kwargs",
    "_read_devtools_file",
    "_log_tail",
    "_wait_for_devtools",
    "_singleton_owner",
    "_singleton_pid",
    "running_instance",
    "_default_user_data_dir",
    "_prepare_dir",
    "launch_chromium",
}
REAL_HELPERS = {"_ephemeral_root"}
# Seamed siblings that are not port targets here (_has_display and
# build_chromium_argv are claimed by port_browser_launcher.c,
# _maybe_autoinstall_chromium by browser_tool_install.c). Every module
# sibling a target calls must be a recorder, or the live def leaks past the
# scripted seam (NameError when it was never extracted).
SEAMED_EXTRA = {
    "_has_display", "get_browser", "_key_lock",
    "_maybe_autoinstall_chromium", "_chromium_missing_hint",
    "_pid_naming_dir", "_pid_alive", "_devtools_endpoint_live", "_start_time",
    "_launch_owned", "_start_shared",
}

CUR = {"in": {}, "trace": [], "counts": {}}


def rec(ev):
    CUR["trace"].append(ev)


def script_next(name):
    io = CUR["in"].get("io") or {}
    s = io.get(name)
    if s is None:
        return None
    if isinstance(s, dict) and "seq" in s:
        seq = s["seq"]
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        return seq[i] if i < len(seq) else None
    return s


BUILTIN_EXC = {
    "OSError": OSError, "IOError": OSError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "PermissionError": PermissionError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "InterruptedError": InterruptedError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ConnectionError": ConnectionError, "ValueError": ValueError,
    "TypeError": TypeError, "RuntimeError": RuntimeError,
    "AttributeError": AttributeError, "KeyError": KeyError,
}


def make_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls in BUILTIN_EXC:
        return BUILTIN_EXC[cls](msg)
    return type(cls, (Exception,), {})(msg)


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class FileDouble:
    """open() result: scripted TEXT with universal-newline translation and
    utf-8-sig BOM strip; readline keeps the trailing \\n, '' at EOF."""

    def __init__(self, content, encoding=None):
        if not isinstance(content, str):
            content = ""
        if encoding == "utf-8-sig" and content.startswith("\ufeff"):
            content = content[1:]
        content = content.replace("\r\n", "\n").replace("\r", "\n")
        self._content = content
        self._lines = content.split("\n")
        self._i = 0

    def read(self):
        return self._content

    def readline(self):
        if self._i >= len(self._lines):
            return ""
        if self._i == len(self._lines) - 1:
            s = self._lines[self._i]
            self._i += 1
            return s
        s = self._lines[self._i] + "\n"
        self._i += 1
        return s

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def materialize(name, r, k):
    if isinstance(r, dict) and "__raise__" in r:
        exc = make_exc(r)
        if name == "rmtree" and k.get("ignore_errors"):
            return None  # ignore_errors=True swallows AFTER recording
        raise exc
    if name == "open":
        return FileDouble(r, k.get("encoding"))
    if name == "read_bytes":
        return base64.b64decode(r["b64"]) if isinstance(r, dict) else b""
    if name == "_key_lock":
        return NoopLock()  # the lock value is scripted but always a lock
    if name == "locate_command":
        return types.SimpleNamespace(
            command=r.get("command") if isinstance(r, dict) else None)
    return r


# Per-seam argument builders: identical field sets to the C args objects
# (None-valued keys dropped on both sides).
ARGMAP = {
    "exists": lambda a, k: {"path": a[0]},
    "open": lambda a, k: {"path": a[0],
                          "mode": a[1] if len(a) > 1 else k.get("mode"),
                          "encoding": k.get("encoding")},
    "read_bytes": lambda a, k: {"path": a[0].as_posix()},
    "mkdir": lambda a, k: {"path": a[0].as_posix(),
                           "mode": k.get("mode", 0o777),
                           "parents": k.get("parents", False),
                           "exist_ok": k.get("exist_ok", False)},
    "makedirs": lambda a, k: {"path": a[0], "mode": k.get("mode", 0o777),
                              "exist_ok": k.get("exist_ok", False)},
    "unlink": lambda a, k: {"path": a[0]},
    "rmtree": lambda a, k: {"path": a[0],
                            "ignore_errors": k.get("ignore_errors", False)},
    "readlink": lambda a, k: {"path": a[0]},
    "isfile": lambda a, k: {"path": a[0]},
    "getpid": lambda a, k: {},
    "geteuid": lambda a, k: {},
    "monotonic": lambda a, k: {},
    "sleep": lambda a, k: {"seconds": a[0]},
    "proc.poll": lambda a, k: {},
    "chromium_executable": lambda a, k: {"allow_override": k.get("allow_override")},
    "locate_command": lambda a, k: {"name": a[0]},
    "command_timeout": lambda a, k: {},
    "is_headed_mode": lambda a, k: {},
    "chromium_args": lambda a, k: {},
    "windows_hide_flags": lambda a, k: {},
    "ns_tag": lambda a, k: {},
    "hermes_home_key": lambda a, k: {},
    "running_in_docker": lambda a, k: {},
    "apparmor_restricts_unprivileged_userns": lambda a, k: {},
    "_resolve_executable": lambda a, k: {},
    "_maybe_autoinstall_chromium": lambda a, k: {},
    "_chromium_missing_hint": lambda a, k: {},
    "_has_display": lambda a, k: {"env": a[0]},
    "_read_devtools_file": lambda a, k: {"user_data_dir": a[0]},
    "_log_tail": lambda a, k: {"user_data_dir": a[0]},
    "_singleton_owner": lambda a, k: {"user_data_dir": a[0]},
    "_singleton_pid": lambda a, k: {"user_data_dir": a[0]},
    "_pid_naming_dir": lambda a, k: {"user_data_dir": a[0]},
    "_pid_alive": lambda a, k: {"pid": a[0]},
    "_devtools_endpoint_live": lambda a, k: {"port": a[0], "path": a[1]},
    "_start_time": lambda a, k: {"pid": a[0]},
    "get_browser": lambda a, k: {"key": a[0]},
    "_key_lock": lambda a, k: {"key": a[0]},
    "chromium_ready": lambda a, k: {},
    "_default_user_data_dir": lambda a, k: {"key": a[0], "home_key": a[1]},
    "_launch_owned": lambda a, k: dict(
        {"key": a[0], "executable": a[1], "user_data_dir": a[2]}, **k),
    "_start_shared": lambda a, k: dict(
        {"key": a[0], "executable": a[1], "user_data_dir": a[2],
         "home_key": a[3],
         "prepare": "<callable>" if callable(a[4]) else None}, **k),
}


def stub(name):
    def f(*a, **k):
        args = {kk: vv for kk, vv in ARGMAP[name](a, k).items()
                if vv is not None}
        r = script_next(name)
        rec({"ev": name, "args": args, "ret": r})
        return materialize(name, r, k)
    return f


class SilentLogger:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass


def extract():
    src = open(SRC, encoding="utf-8").read()
    modname = "browser_launcher_flow_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod  # py3.14 dataclasses annotation resolution
    ns = mod.__dict__
    ns.update({
        "__name__": modname,
        "json": json, "hashlib": hashlib, "re": re, "stat": stat,
        "threading": threading, "contextlib": contextlib, "socket": socket,
        "Path": Path, "dataclass": dataclass, "field": field,
        "Any": object, "Callable": object, "Optional": object,
        "os": types.SimpleNamespace(),
        "sys": types.SimpleNamespace(platform="linux"),
        "time": types.SimpleNamespace(),
        "shutil": types.SimpleNamespace(),
        "subprocess": types.SimpleNamespace(),
        "logging": types.SimpleNamespace(getLogger=lambda name: SilentLogger()),
        "logger": SilentLogger(),
    })
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag

    def compile_node(node):
        return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                       flags=flags)

    # Module constants: Assign AND AnnAssign, plain-Name targets only.
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(
                isinstance(t, ast.Name) for t in node.targets):
            try:
                exec(compile_node(node), ns)
            except Exception:
                pass
        elif (isinstance(node, ast.AnnAssign) and node.value is not None
              and isinstance(node.target, ast.Name)):
            try:
                exec(compile_node(node), ns)
            except Exception:
                pass
    real = {}
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "LaunchedBrowser":
            exec(compile_node(node), ns)
        elif isinstance(node, ast.FunctionDef) and (
                node.name in WANTED or node.name in REAL_HELPERS):
            exec(compile_node(node), ns)
            real[node.name] = ns[node.name]
    missing = (WANTED | REAL_HELPERS) - set(real)
    if missing:
        raise RuntimeError("extraction missed: %s" % sorted(missing))
    for const in ("_DEVTOOLS_FILE", "_LAUNCH_LOG", "_MIN_FIRST_LAUNCH_TIMEOUT_S",
                  "_POLL_INTERVAL_S", "_STDERR_TAIL_CHARS", "_lock"):
        if const not in ns:
            raise RuntimeError("constant not seeded: %s" % const)
    return ns, real


def modstub(fullname, **attrs):
    parts = fullname.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        m = sys.modules.get(nm)
        if m is None:
            m = types.ModuleType(nm)
            m.__path__ = []
            sys.modules[nm] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], m)
    for kk, vv in attrs.items():
        setattr(sys.modules[fullname], kk, vv)


def install_module_stubs():
    modstub("hermes_cli.browser_runtime",
            chromium_executable=stub("chromium_executable"),
            chromium_missing_hint=stub("_chromium_missing_hint"))
    modstub("hermes_platform.resolver", locate_command=stub("locate_command"))
    modstub("tools.browser_session_cloud",
            command_timeout=stub("command_timeout"),
            is_headed_mode=stub("is_headed_mode"),
            chromium_args=stub("chromium_args"))
    modstub("tools.browser_launcher_holders", ns_tag=stub("ns_tag"))
    modstub("tools.process_registry")
    modstub("hermes_constants", hermes_home_key=stub("hermes_home_key"))
    modstub("hermes_cli._subprocess_compat",
            windows_hide_flags=stub("windows_hide_flags"))


def install_path_patches():
    def mkdir_stub(self, mode=0o777, parents=False, exist_ok=False):
        return stub("mkdir")(self, mode=mode, parents=parents,
                             exist_ok=exist_ok)

    def read_bytes_stub(self):
        return stub("read_bytes")(self)

    Path.mkdir = mkdir_stub
    Path.read_bytes = read_bytes_stub


def setup_case(ns, real, inp, target):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    # Every seamed sibling is a recorder EXCEPT the target under test.
    for name in (WANTED | SEAMED_EXTRA):
        ns[name] = real[name] if name == target else stub(name)
    for name in REAL_HELPERS:
        ns[name] = real[name]
    o = types.SimpleNamespace()
    o.name = inp.get("os_name", "posix")
    o.path = types.SimpleNamespace(join=posixpath.join,
                                   exists=stub("exists"),
                                   isfile=stub("isfile"))
    o.readlink = stub("readlink")
    o.makedirs = stub("makedirs")
    o.unlink = stub("unlink")
    o.getpid = stub("getpid")
    if inp.get("has_geteuid", True):
        o.geteuid = stub("geteuid")
    ns["os"] = o
    ns["sys"] = types.SimpleNamespace(platform=inp.get("platform", "linux"))
    ns["time"] = types.SimpleNamespace(monotonic=stub("monotonic"),
                                       sleep=stub("sleep"))
    ns["shutil"] = types.SimpleNamespace(rmtree=stub("rmtree"))
    ns["open"] = stub("open")  # builtin open, shadowed in the exec namespace
    sub = types.SimpleNamespace()
    cnp = inp.get("create_new_process_group")
    if cnp is not None:
        sub.CREATE_NEW_PROCESS_GROUP = cnp
    ns["subprocess"] = sub
    ns["_served_homes"] = set()
    ns["_browsers"] = {}
    ns["_procs"] = {}
    ns["_launching"] = {}


def drive(ns, func, inp):
    udd = inp.get("user_data_dir", "")
    if func in ("running_in_docker", "apparmor_restricts_unprivileged_userns",
                "needs_chromium_sandbox_bypass", "_resolve_executable",
                "chromium_ready", "_spawn_kwargs"):
        return ns[func]()
    if func == "_explicit_executable_configured":
        return ns[func](stub("chromium_executable"))
    if func == "_launch_timeout":
        return ns[func](inp.get("timeout"))
    if func == "_resolve_headed":
        return ns[func](inp.get("headed"), inp.get("env") or {})
    if func in ("_read_devtools_file", "_log_tail", "_singleton_owner",
                "_singleton_pid", "running_instance"):
        return ns[func](udd)
    if func == "_wait_for_devtools":
        proc = types.SimpleNamespace(poll=stub("proc.poll"))
        return ns[func](proc, udd, inp.get("deadline", 0))
    if func == "_default_user_data_dir":
        r = ns[func](inp.get("key", ""), inp.get("home_key", ""))
        return Path(r).as_posix()
    if func == "_prepare_dir":
        return ns[func](udd, inp.get("ephemeral", True))
    if func == "launch_chromium":
        prepare = (lambda: None) if inp.get("prepare") else None
        return ns[func](
            inp.get("key", ""),
            executable=inp.get("executable"),
            user_data_dir=inp.get("user_data_dir"),
            ephemeral=inp.get("ephemeral", True),
            headed=inp.get("headed"),
            extra_args=inp.get("extra_args", ()),
            env=inp.get("env"),
            timeout=inp.get("timeout"),
            sandbox_bypass=inp.get("sandbox_bypass", True),
            prepare=prepare,
        )
    raise ValueError("unknown func: %s" % func)


def norm_ret(x):
    if dataclasses.is_dataclass(x) and not isinstance(x, type):
        return norm_ret(dataclasses.asdict(x))
    if isinstance(x, tuple):
        return [norm_ret(v) for v in x]
    if isinstance(x, list):
        return [norm_ret(v) for v in x]
    if isinstance(x, dict):
        return {k: norm_ret(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()),
                              key=lambda kv: kv[0]))
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
    ns, real = extract()
    install_module_stubs()
    install_path_patches()
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
        setup_case(ns, real, inp, func)
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm_ret(ret), "raise": None}
        except BaseException as e:  # SystemExit included — fail closed
            want = {"ret": None,
                    "raise": {"__raise__": type(e).__name__, "msg": str(e)}}
        want["trace"] = CUR["trace"]
        want["state"] = {"served_homes": sorted(ns["_served_homes"])}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE browser_launcher_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
