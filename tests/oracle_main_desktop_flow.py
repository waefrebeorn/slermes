#!/usr/bin/env python3
"""Oracle for the main_desktop.py desktop-launch FLOW-core port
(src/cli/port_main_desktop_flow.c).

Drives the LIVE hermes_cli/main_desktop.py launch/lifecycle functions with
SCRIPTED I/O: env, file reads/renames, sleeps, prints, sibling calls,
dependency imports and the ctypes/kernel32 doubles are recording stubs fed
the same scripts the C harness feeds its seams. Compares the DECISION
SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
("ret", with {"__raise__": ...} for escaping exceptions incl. SystemExit),
captured real stdout/stderr, and — for stateful ports — the mutated state
("state").

The live defs are AST-extracted from the quarry (module constants seeded from
Assign AND AnnAssign, plain-Name targets only; the extraction namespace is a
registered module so py3.14 dataclasses/annotation resolution works). The
heavy quarry import chain is NEVER imported: in-function imports resolve
against stub sys.modules entries whose attributes are recording seams;
`import ctypes` resolves against a scripted ctypes double. Fail-closed:
missing defs or 0 cases => exit 2.
Summary: 'ORACLE main_desktop_flow: N cases, M mismatches'.

Representation contract (shared with the C port header):
- Paths are POSIX strings; the live defs run with Path == FakePath (posix
  join/parts/name semantics) and os.path == posixpath.
- args is a SimpleNamespace built from in.args (vars(args) projection).
- in.platform models sys.platform; sys.exit is the real one (SystemExit keeps
  its code's type in {"__raise__":"SystemExit","code":...}).
- in.env is the child-env initial state (with_hermes_node_path effect seam);
  in.os_environ is the process env for `key in os.environ` (env.contains,
  state-resolved, recorded but not scripted).
- Scripted OSError str(): with errno -> "[Errno N] message"; without ->
  "message" (both sides construct the exception identically).
- file.read scripts carry hex-encoded bytes (bytes.fromhex live).
- Sets normalize to SORTED ascending arrays on both sides before comparison.
- Integral floats normalize to ints on both sides before comparison.
- print is an echo/effect seam: a script entry is required as a marker; the
  effect (capture-buffer append) is the result, not the script value.
"""
import ast
import builtins
import contextlib as _real_contextlib
import io as _real_io
import json
import logging
import math
import os
import posixpath
import re
import sys
import types
from pathlib import PurePosixPath
from typing import Any, Callable, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "hermes_cli", "main_desktop.py")

TARGETS = [
    "_site_packages_install_kind", "_explicit_profile_args",
    "_diagnose_esbuild_ignore_scripts", "_macos_signing_downgrade_error",
    "_sandbox_helper_is_setuid_root", "_windows_native_machine",
    "_windows_user_runnable_pe_machines", "_expected_windows_pe_machines",
    "_parse_pe_machine", "_packaged_node_pty_missing",
    "_packaged_desktop_current_for_head", "_prefer_wsl_d3d12",
    "_desktop_launch_env", "_check_desktop_skip_build",
    "_packaged_desktop_launch_command", "_run_desktop_one_shot",
    "_launch_bundled_desktop", "_rename_riding_out_file_lock",
]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


# Extraction namespace IS a registered module (py3.14 annotation resolution
# needs a real module).
_MOD = types.ModuleType("hermes_cli_main_desktop_flow_extracted")
_MOD.__file__ = SRC
sys.modules[_MOD.__name__] = _MOD
NS = _MOD.__dict__

CURRENT_CTX = None


def _ctx():
    if CURRENT_CTX is None:
        raise RuntimeError("seam called outside a case")
    return CURRENT_CTX


# ── data model doubles ────────────────────────────────────────────────────
class FakePath:
    """Path double: posix str, recording predicates/reads, posix joining."""
    def __init__(self, s):
        self.s = s

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __repr__(self):
        return f"FakePath({self.s!r})"

    def __truediv__(self, other):
        return FakePath(posixpath.join(self.s, str(other)))

    @property
    def parts(self):
        s = self.s
        if not s:
            return ()
        comps = tuple(p for p in s.split("/") if p)
        return ("/",) + comps if s.startswith("/") else comps

    @property
    def name(self):
        s = self.s.rstrip("/")
        return s.rsplit("/", 1)[-1] if s else ""

    def exists(self):
        return _ctx().call("path.exists", {"path": self.s})

    def is_file(self):
        return _ctx().call("path.is_file", {"path": self.s})

    def is_dir(self):
        return _ctx().call("path.is_dir", {"path": self.s})

    def glob(self, pattern):
        r = _ctx().call("path.glob", {"path": self.s, "pattern": pattern})
        return [FakePath(x) for x in r]

    def rglob(self, pattern):
        r = _ctx().call("path.rglob", {"path": self.s, "pattern": pattern})
        return iter([FakePath(x) for x in r])   # next(..., None) live

    def stat(self):
        r = _ctx().call("path.stat", {"path": self.s})
        return types.SimpleNamespace(**r)

    def open(self, mode="r", *a, **kw):
        _ctx().call("path.open", {"path": self.s, "mode": mode})
        return FakeFile(self.s)

    def expanduser(self):
        return FakePath(_ctx().call("path.expanduser", {"path": self.s}))

    def resolve(self, strict=False):
        return FakePath(_ctx().call("path.resolve", {"path": self.s}))


class FakeFile:
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self, n=-1):
        r = _ctx().call("file.read", {"path": self.path, "n": n})
        return bytes.fromhex(r)

    def seek(self, pos, whence=0):
        _ctx().call("file.seek", {"path": self.path, "pos": pos})


class EnvDouble:
    """os.environ double: membership is state-resolved (in.os_environ)."""
    def __init__(self, ctx, env):
        self.ctx = ctx
        self.env = dict(env)

    def __contains__(self, name):
        return self.ctx.state_call("env.contains", {"name": name},
                                   lambda: name in self.env)


class NotBundledApp(Exception):
    pass


# ── exception materialization ─────────────────────────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "InterruptedError": InterruptedError,
    "TimeoutError": TimeoutError, "ProcessLookupError": ProcessLookupError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "Exception": Exception,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "NotBundledApp": NotBundledApp,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "SystemExit":
        return SystemExit(spec.get("code"))
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    if isinstance(exc, SystemExit):
        return {"__raise__": "SystemExit", "code": exc.code}
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend ───────────────────────────────────────────────
class Ctx:
    def __init__(self, inp):
        self.inp = inp
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []
        self.child_env = None   # with_hermes_node_path effect state

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry

    def state_call(self, name, args, fn):
        self.trace.append({"name": name, "args": args})
        return fn()


# ── ctypes double (scripted GetMachineTypeAttributes) ─────────────────────
class _FakeCInt:
    def __init__(self, value=0):
        self.value = value


class _FakeProc:
    """kernel32.GetMachineTypeAttributes: argtypes/restype assignable."""
    def __call__(self, machine, attrref):
        entry = _ctx().call("GetMachineTypeAttributes", {"machine": machine})
        if isinstance(entry, dict):
            attrref.value = entry.get("attributes", 0)
            return entry.get("hresult", 0)
        return entry


class _FakeKernel32:
    def __init__(self):
        self.GetMachineTypeAttributes = _FakeProc()


def _fake_kernel32():
    _ctx().call("_kernel32", {})
    return _FakeKernel32()


_ctypes_double = types.ModuleType("ctypes")
_ctypes_double.c_int = _FakeCInt
_ctypes_double.c_long = object()
_ctypes_double.byref = lambda x: x          # the c_int itself carries .value
_ctypes_double.POINTER = lambda x: object()
_ctypes_double.WinDLL = lambda *a, **k: None
_wintypes_double = types.ModuleType("ctypes.wintypes")
_wintypes_double.USHORT = object()
_ctypes_double.wintypes = _wintypes_double
sys.modules["ctypes"] = _ctypes_double
sys.modules["ctypes.wintypes"] = _wintypes_double


# ── stub modules for in-function imports (recording seams) ────────────────
def _stub_module(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parent, _, leaf = name.rpartition(".")
    if parent:
        pmod = sys.modules.get(parent)
        if pmod is None:
            pmod = _stub_module(parent)
        setattr(pmod, leaf, m)
    return m


def _fp_or_none(r):
    return None if r is None else FakePath(r)


def _with_hermes_node_path():
    ctx = _ctx()

    def do():
        ctx.child_env = dict(ctx.inp.get("env") or {})
        return ctx.child_env
    return ctx.state_call("with_hermes_node_path", {}, do)


def _resolve_bundle_layout(root):
    r = _ctx().call("resolve_bundle_layout", {"project_root": str(root)})
    launcher = r.get("launcher")
    return types.SimpleNamespace(
        launcher=FakePath(launcher) if launcher is not None else None,
        app_root=FakePath(r.get("app_root", "")))


def _launch_detached(cmd, env=None, cwd=None):
    return _ctx().call("launch_detached", {
        "launch_command": [str(c) for c in cmd],
        "env": dict(env) if env is not None else None,
        "cwd": str(cwd) if cwd is not None else None,
    })


_stub_module("hermes_constants",
    is_wsl=lambda: _ctx().call("is_wsl", {}),
    socket_safe_tmpdir=lambda: _ctx().call("socket_safe_tmpdir", {}),
    with_hermes_node_path=_with_hermes_node_path,
)
_stub_module("hermes_cli.main",
    explicit_cli_profile=lambda: _ctx().call("explicit_cli_profile", {}),
    PROJECT_ROOT=FakePath("/repo"),
)
_stub_module("hermes_cli.steward",
    read_install_stamp=lambda resources: _ctx().call(
        "read_install_stamp", {"resources": str(resources)}),
)
_stub_module("hermes_cli.version_info",
    _run_git=lambda root, *args: _ctx().call(
        "_run_git", {"project_root": str(root), "args": list(args)}),
)
_stub_module("hermes_cli.main_desktop_apparmor",
    print_no_sandbox_fallback=lambda exe: _ctx().call(
        "print_no_sandbox_fallback", {"packaged_executable": str(exe)}),
    install_profile=lambda exe: _ctx().call("install_profile", {"exe": str(exe)}),
)
_stub_module("hermes_cli.main_desktop_tcc",
    _desktop_macos_setup_tcc_identity=lambda identity: _ctx().call(
        "_desktop_macos_setup_tcc_identity", {"identity": identity}),
)
_stub_module("hermes_cli.bundled_app",
    NotBundledApp=NotBundledApp,
    resolve_bundle_layout=_resolve_bundle_layout,
    launch_detached=_launch_detached,
)


# ── extraction ────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    NS.update({
        "os": os, "sys": sys, "re": re, "json": json, "math": math,
        "types": types, "logging": logging, "posixpath": posixpath,
        "contextlib": _real_contextlib, "stat": __import__("stat"),
        "shlex": __import__("shlex"), "hashlib": __import__("hashlib"),
        "subprocess": __import__("subprocess"), "argparse": __import__("argparse"),
        "Path": FakePath, "PurePosixPath": PurePosixPath,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "SimpleNamespace": types.SimpleNamespace,
        "platform": types.SimpleNamespace(
            machine=lambda: _ctx().call("platform.machine", {})),
        "facts": types.SimpleNamespace(
            native_arch=lambda: _ctx().call("facts.native_arch", {})),
        "_time_mod": types.SimpleNamespace(
            sleep=lambda d: _ctx().call("time.sleep", {"delay": d}),
            time=lambda: 0),
        "desktop_launch_notice": lambda message, **kw: _ctx().call(
            "desktop_launch_notice", {"message": message}),
        "desktop_console_output": lambda **kw: None,
        "__file__": SRC,
    })
    tree = ast.parse(src)

    def seed_constants():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue   # never exec Subscript targets (real env mutation)
                try:
                    exec_node(src, node, NS, SRC)
                except Exception:
                    pass

    seed_constants()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, NS, SRC)
            except Exception:
                pass
    seed_constants()   # re-seed after defs (forwarder aliases etc.)

    missing = [t for t in TARGETS if t not in NS]
    if missing:
        print(f"FATAL oracle_main_desktop_flow: missing defs: {missing}")
        sys.exit(2)


extract()

# logger double (the seeded Assign built a real logging.Logger).
NS["logger"] = types.SimpleNamespace(
    warning=lambda fmt, *a: _ctx().call("logger.warning", {"message": fmt % a}),
    info=lambda fmt, *a: _ctx().call("logger.info", {"message": fmt % a}),
)

# Module-global siblings of the targets: recording overrides (the AST pass
# exec'd the real defs; the flow comparison seams them like the C port does).
NS["_desktop_dist_exists"] = lambda desktop_dir: _ctx().call(
    "_desktop_dist_exists", {"desktop_dir": str(desktop_dir)})
NS["_electron_dir"] = lambda project_root: FakePath(_ctx().call(
    "_electron_dir", {"project_root": str(project_root)}))
NS["_desktop_packaged_executable"] = lambda desktop_dir: _fp_or_none(_ctx().call(
    "_desktop_packaged_executable", {"desktop_dir": str(desktop_dir)}))
NS["_packaged_resources_dir"] = lambda desktop_dir: _fp_or_none(_ctx().call(
    "_packaged_resources_dir", {"desktop_dir": str(desktop_dir)}))
NS["_desktop_build_needed"] = lambda desktop_dir, project_root, *, source_mode: _ctx().call(
    "_desktop_build_needed", {"desktop_dir": str(desktop_dir),
                              "project_root": str(project_root), "source_mode": source_mode})
NS["_desktop_launch_options"] = lambda: _ctx().call("_desktop_launch_options", {})
NS["_detect_linux_password_store"] = lambda: _ctx().call("_detect_linux_password_store", {})
NS["_desktop_linux_sandbox_fixup"] = lambda exe: _ctx().call(
    "_desktop_linux_sandbox_fixup", {"packaged_executable": str(exe)})
NS["_desktop_linux_needs_no_sandbox"] = lambda: _ctx().call("_desktop_linux_needs_no_sandbox", {})
NS["_desktop_linux_sandbox_helper_is_regular_file"] = lambda exe: _ctx().call(
    "_desktop_linux_sandbox_helper_is_regular_file", {"packaged_executable": str(exe)})
NS["_desktop_linux_needs_disable_setuid_sandbox"] = lambda exe: _ctx().call(
    "_desktop_linux_needs_disable_setuid_sandbox", {"packaged_executable": str(exe)})
NS["_kernel32"] = _fake_kernel32

# Live defs kept for the nested-call flows (_desktop_launch_env ->
# _prefer_wsl_d3d12; _launch_bundled_desktop -> _packaged_desktop_launch_command
# + _explicit_profile_args). Saved for the per-case sibling seaming of
# _expected_windows_pe_machines.
_LIVE_RUNNABLE = NS["_windows_user_runnable_pe_machines"]
_LIVE_NATIVE = NS["_windows_native_machine"]
_REAL_SYS_EXIT = sys.exit


# ── per-case installation ─────────────────────────────────────────────────
def install_case(func, inp, ctx):
    saved = {"print": builtins.print, "os": NS.get("os"), "sys": NS.get("sys"),
             "runnable": NS.get("_windows_user_runnable_pe_machines"),
             "native": NS.get("_windows_native_machine"),
             "project_root": sys.modules["hermes_cli.main"].PROJECT_ROOT}

    def patched_print(*a, sep=" ", end="\n", file=None, flush=False):
        text = sep.join(str(x) for x in a) + end
        fname = "stderr" if file is sys.stderr else "stdout"
        ctx.call("print", {"text": text, "end": end, "file": fname})
        (sys.stderr if fname == "stderr" else sys.stdout).write(text)

    builtins.print = patched_print
    NS["os"] = types.SimpleNamespace(
        path=posixpath, name="posix",
        environ=EnvDouble(ctx, inp.get("os_environ") or {}),
        fsencode=os.fsencode,
        getcwd=lambda: ctx.call("os.getcwd", {}),
        rename=lambda s, d: ctx.call("os.rename", {"src": str(s), "dst": str(d)}),
    )
    NS["sys"] = types.SimpleNamespace(platform=inp.get("platform", "linux"),
                                      exit=_REAL_SYS_EXIT, stdin=None)
    sys.modules["hermes_cli.main"].PROJECT_ROOT = FakePath(inp.get("project_root", "/repo"))
    if func == "_expected_windows_pe_machines":
        NS["_windows_user_runnable_pe_machines"] = lambda: ctx.call(
            "_windows_user_runnable_pe_machines", {})
        NS["_windows_native_machine"] = lambda: ctx.call("_windows_native_machine", {})
    return saved


def restore_case(saved):
    builtins.print = saved["print"]
    NS["os"] = saved["os"]
    NS["sys"] = saved["sys"]
    NS["_windows_user_runnable_pe_machines"] = saved["runnable"]
    NS["_windows_native_machine"] = saved["native"]
    sys.modules["hermes_cli.main"].PROJECT_ROOT = saved["project_root"]


def state_for(func, ctx, env_state):
    if func == "_prefer_wsl_d3d12":
        return {"env": dict(env_state)}
    if func == "_desktop_launch_env":
        return {"env": dict(ctx.child_env if ctx.child_env is not None else {})}
    return None


def drive(func, inp, ctx, args, env_state):
    if func == "_site_packages_install_kind":
        ret = NS[func](inp.get("project_root", ""))
    elif func == "_explicit_profile_args":
        ret = NS[func]()
    elif func == "_diagnose_esbuild_ignore_scripts":
        ret = NS[func](inp.get("output"))
    elif func == "_macos_signing_downgrade_error":
        ret = NS[func](inp.get("installed") or {}, inp.get("rebuilt"))
    elif func == "_sandbox_helper_is_setuid_root":
        ret = NS[func](types.SimpleNamespace(**(inp.get("st") or {})))
    elif func in ("_windows_native_machine", "_windows_user_runnable_pe_machines",
                  "_expected_windows_pe_machines"):
        ret = NS[func]()
    elif func == "_parse_pe_machine":
        ret = NS[func](FakePath(inp["path"]))
    elif func == "_packaged_node_pty_missing":
        ret = NS[func](FakePath(inp["dist_dir"]))
    elif func == "_packaged_desktop_current_for_head":
        ret = NS[func](FakePath(inp["desktop_dir"]), FakePath(inp["project_root"]))
    elif func == "_prefer_wsl_d3d12":
        ret = NS[func](env_state)
    elif func == "_desktop_launch_env":
        ret = NS[func](args)
    elif func == "_check_desktop_skip_build":
        pe = inp.get("packaged_executable")
        ret = NS[func](FakePath(inp["desktop_dir"]), FakePath(inp["project_root"]),
                       source_mode=bool(inp.get("source_mode", False)),
                       packaged_executable=FakePath(pe) if pe is not None else None)
    elif func == "_packaged_desktop_launch_command":
        ret = NS[func](FakePath(inp["packaged_executable"]))
    elif func == "_run_desktop_one_shot":
        ret = NS[func](args, FakePath(inp["desktop_dir"]),
                       bundled=bool(inp.get("bundled", False)))
    elif func == "_launch_bundled_desktop":
        ret = NS[func](args, dict(inp.get("env") or {}),
                       list(inp.get("electron_flags") or []))
    elif func == "_rename_riding_out_file_lock":
        ret = NS[func](FakePath(inp["src"]), FakePath(inp["dst"]))
    else:
        raise RuntimeError(f"unknown func {func}")
    return ret, state_for(func, ctx, env_state)


def canon(v):
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, FakePath):
        return v.s
    if isinstance(v, (set, frozenset)):
        return sorted(canon(x) for x in v)
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def run_case(func, inp):
    global CURRENT_CTX
    ctx = Ctx(inp)
    CURRENT_CTX = ctx
    cap_out = _real_io.StringIO()
    cap_err = _real_io.StringIO()
    args = types.SimpleNamespace(**(inp.get("args") or {}))
    env_state = dict(inp.get("env") or {})
    saved = install_case(func, inp, ctx)
    ret = None
    state = None
    try:
        with _real_contextlib.redirect_stdout(cap_out), _real_contextlib.redirect_stderr(cap_err):
            ret, state = drive(func, inp, ctx, args, env_state)
    except BaseException as exc:   # noqa: BLE001 - SystemExit included
        ret = proj_exc(exc)
        state = state_for(func, ctx, env_state)
    finally:
        restore_case(saved)
        CURRENT_CTX = None
    out = {"ret": ret, "trace": ctx.trace,
           "stdout": cap_out.getvalue(), "stderr": cap_err.getvalue()}
    if state is not None:
        out["state"] = state
    return out


def deep_eq(a, b):
    return canon(a) == canon(b)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in", {}) or {}
        c_out = rec.get("out")
        cases += 1
        if func not in TARGETS:
            mism += 1
            print(f"MISMATCH {func}: unknown func (fail-closed)")
            continue
        try:
            py_out = run_case(func, inp)
        except Exception as exc:   # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(c_out.get(key)), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(py_out.get(key)), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_main_desktop_flow: 0 cases")
        print("ORACLE main_desktop_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE main_desktop_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
