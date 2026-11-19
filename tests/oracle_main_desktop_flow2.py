#!/usr/bin/env python3
"""Oracle for the main_desktop.py UNCLAIMED-remainder FLOW port
(src/cli/main_desktop_flow2.c).

Drives the LIVE hermes_cli/main_desktop.py stage-and-swap / macOS-signing /
installed-app flows with SCRIPTED I/O: path predicates/reads/renames, glob,
lstat, subprocess, shutil, psutil, plistlib, ctypes, os.walk/getpid, logger,
print, claimed siblings and foreign-module imports are recording stubs fed
the same scripts the C harness feeds its seams. Compares the DECISION
SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
("ret", with {"__raise__": ...} for escaping exceptions incl. SystemExit),
captured real stdout/stderr, and — for stateful ports — the mutated state
("state"). Ported functions call each other REAL (internal composition);
only CLAIMED siblings (port_main_desktop_flow.c) and foreign imports are
seamed.

The live defs are AST-extracted from the quarry (module constants seeded
from Assign AND AnnAssign, plain-Name targets only; the extraction namespace
is a registered module so py3.14 annotation resolution works). The heavy
quarry import chain is NEVER imported: in-function imports resolve against
stub sys.modules entries whose attributes are recording seams. Fail-closed:
missing defs or 0 cases => exit 2.
Summary: 'ORACLE main_desktop_flow2: N cases, M mismatches'.

Representation contract (shared with the C port header):
- Paths are normalized POSIX strings; the live defs run with Path == FakePath
  (posix join/parent/parents/parts/name/suffix/relative_to semantics).
- in.platform models sys.platform (default "linux").
- Sets normalize to SORTED ascending arrays; tuples to arrays; integral
  floats to ints; Path/proc/WinDLL doubles project to their carrier values.
- Scripted OSError str(): with errno -> "[Errno N] message"; without ->
  "message". Base OSError(errno, msg) promotes to the errno subclass.
- subprocess.run seam args {"argv", "capture_output": true, **kwargs};
  check=True + nonzero rc raises the real CalledProcessError (str parity).
- shutil.rmtree(ignore_errors=True) swallows OSError-family raises.
- path.read_text(encoding="utf-8-sig") strips a leading BOM after the seam.
- file.read / path.read_bytes / plistlib.loads scripts carry hex bytes.
- json.loads runs REAL (strict); the C port pre-validates the same grammar.
- _swap_staged_desktop_app seam ret {"exe", "held_by"} models the sibling's
  held_by list mutation; release_after_refused_rename ret {"holders",
  "closed"} projects to the live tuple.
- _force_adhoc_macos_signing seam True applies the sibling's single env
  mutation (CSC_IDENTITY_AUTO_DISCOVERY="false").
- print is an echo/effect seam (script marker required; args.text carries
  the end newline; the harness appends to the real capture buffers).
- BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) passes
  through every except Exception site; catch matching walks the full
  builtin hierarchy.
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
import subprocess as _real_subprocess
import sys
import types
from typing import Any, Callable, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "hermes_cli", "main_desktop.py")

TARGETS = [
    "_packaged_resources_dir", "_recover_interrupted_swap",
    "_move_live_app_aside", "_discard_desktop_staging", "_kernel32",
    "_runs_from", "_desktop_ancestor_in", "_sandbox_helper_lstat",
    "_stage_macos_bundle_copy", "_launch_installed_macos_desktop_app",
    "_desktop_macos_bundle_id", "_desktop_macos_local_signing_identity",
    "_codesign_verify", "_macos_signature_summary",
    "_desktop_macos_has_valid_real_signature", "_macos_legacy_adhoc_resign",
    "_app_asar_hash", "_swap_in_new_macos_bundle",
    "_running_macos_app_bundles", "_update_owned_macos_bundles",
    "_owns_installed_desktop_apps", "_installed_desktop_apps",
    "_install_rebuilt_macos_bundles", "_install_rebuilt_desktop_app",
    "_refresh_installed_desktop_apps", "_installed_desktop_launch_target",
    "_promote_staged_desktop_app", "build_prepared_desktop",
    "_desktop_macos_local_codesign",
]

# Claimed siblings (port_main_desktop_flow.c) — SEAMED, never re-ported.
CLAIMED = [
    "_desktop_packaged_executable", "_desktop_packaged_executable_in",
    "_rename_riding_out_file_lock", "_desktop_macos_relaunchable_fixup",
    "_desktop_exe_integrity_error", "_swap_staged_desktop_app",
    "_desktop_staging_dir", "_stop_desktop_processes_locking_build",
    "_force_adhoc_macos_signing", "_diagnose_esbuild_ignore_scripts",
    "_macos_signing_downgrade_error",
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


_MOD = types.ModuleType("hermes_cli_main_desktop_flow2_extracted")
_MOD.__file__ = SRC
sys.modules[_MOD.__name__] = _MOD
NS = _MOD.__dict__

CURRENT_CTX = None


def _ctx():
    if CURRENT_CTX is None:
        raise RuntimeError("seam called outside a case")
    return CURRENT_CTX


# ── data model doubles ────────────────────────────────────────────────────

def _posix_parent(s):
    if not s or s == ".":
        return "."
    n = len(s)
    while n > 1 and s[n - 1] == "/":
        n -= 1
    if n == 1 and s[0] == "/":
        return "/"
    cut = s.rfind("/", 0, n)
    if cut < 0:
        return "."
    if cut == 0:
        return "/"
    return s[:cut]


def _posix_parents(s):
    out = []
    cur = s or ""
    while True:
        p = _posix_parent(cur)
        if p == cur:
            break
        out.append(p)
        cur = p
    return out


def _posix_parts(s):
    if not s or s == ".":
        return ()
    comps = tuple(p for p in s.split("/") if p)
    return ("/",) + comps if s.startswith("/") else comps


def _posix_name(s):
    if not s:
        return ""
    t = s.rstrip("/")
    if not t:
        return ""
    return t.rsplit("/", 1)[-1]


def _posix_suffix(s):
    name = _posix_name(s)
    i = name.rfind(".")
    return name[i:] if i > 0 else ""


class ParentsDouble:
    """PurePosixPath.parents: _PathParents raises IndexError(index) OOR."""
    def __init__(self, s):
        self._list = _posix_parents(s)

    def __len__(self):
        return len(self._list)

    def __getitem__(self, i):
        if isinstance(i, slice):
            return [FakePath(x) for x in self._list[i]]
        if i < 0 or i >= len(self._list):
            raise IndexError(i)
        return FakePath(self._list[i])

    def __iter__(self):
        return iter(FakePath(x) for x in self._list)

    def __contains__(self, other):
        return any(FakePath(x) == other for x in self._list)


class FakePath:
    """Path double: posix str, recording predicates/reads, posix joining."""
    def __init__(self, s):
        if hasattr(s, "__fspath__"):
            s = s.__fspath__()
        if not isinstance(s, str):
            tn = type(s).__name__
            raise TypeError(
                "argument should be a str or an os.PathLike object where "
                f"__fspath__ returns a str, not '{tn}'")
        self.s = s

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __repr__(self):
        return f"FakePath({self.s!r})"

    def __eq__(self, other):
        return isinstance(other, FakePath) and self.s == other.s

    def __hash__(self):
        return hash(self.s)

    def __truediv__(self, other):
        return FakePath(posixpath.join(self.s, str(other)))

    @property
    def parts(self):
        return _posix_parts(self.s)

    @property
    def name(self):
        return _posix_name(self.s)

    @property
    def suffix(self):
        return _posix_suffix(self.s)

    @property
    def parent(self):
        return FakePath(_posix_parent(self.s))

    @property
    def parents(self):
        return ParentsDouble(self.s)

    def relative_to(self, base):
        sp, bp = _posix_parts(self.s), _posix_parts(str(base))
        if len(bp) > len(sp) or sp[:len(bp)] != bp:
            raise ValueError(f"{self.s!r} is not in the subpath of {str(base)!r}")
        return FakePath("/".join(sp[len(bp):]) if sp[len(bp):] else ".")

    def exists(self):
        return _ctx().call("path.exists", {"path": self.s})

    def is_file(self):
        return _ctx().call("path.is_file", {"path": self.s})

    def is_dir(self):
        return _ctx().call("path.is_dir", {"path": self.s})

    def lstat(self):
        r = _ctx().call("path.lstat", {"path": self.s})
        return types.SimpleNamespace(**r)

    def stat(self):
        r = _ctx().call("path.stat", {"path": self.s})
        return types.SimpleNamespace(**r)

    def glob(self, pattern):
        r = _ctx().call("path.glob", {"path": self.s, "pattern": pattern})
        if isinstance(r, str):
            r = [r]
        return [FakePath(x) for x in r]

    def rename(self, target):
        return _ctx().call("path.rename", {"path": self.s, "target": str(target)})

    def resolve(self, strict=False):
        return FakePath(_ctx().echo("path.resolve", {"path": self.s}, self.s))

    def read_bytes(self):
        r = _ctx().call("path.read_bytes", {"path": self.s})
        return bytes.fromhex(r)

    def read_text(self, encoding=None, errors=None):
        r = _ctx().call("path.read_text", {"path": self.s, "encoding": encoding})
        if encoding == "utf-8-sig" and r.startswith("\ufeff"):
            r = r[1:]
        return r


class ProcDouble:
    def __init__(self, id=None, pid=None, info=None):
        self.id = id
        self.pid = pid
        self.info = info if info is not None else {}

    def exe(self):
        return _ctx().call("proc.exe", {"proc": self.id})

    def parents(self):
        r = _ctx().call("proc.parents", {"proc": self.id})
        return [ProcDouble(id=e["id"], pid=e["pid"]) for e in r]


class Kernel32Double:
    def __init__(self, payload):
        self.payload = payload


class FakeFile:
    """builtin open(asar, 'rb') double: read(n) fires the file.read seam."""
    def __init__(self, path, ctx):
        self.path = path
        self.ctx = ctx

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self, n=-1):
        r = self.ctx.call("file.read", {"path": self.path, "n": n})
        return bytes.fromhex(r)


def _fp_or_none(r):
    return None if r is None else FakePath(r)


# ── exception materialization ─────────────────────────────────────────────
EXC_CLASSES = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "Exception": Exception, "TypeError": TypeError,
    "StopAsyncIteration": StopAsyncIteration, "StopIteration": StopIteration,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "EOFError": EOFError, "RuntimeError": RuntimeError,
    "RecursionError": RecursionError, "NotImplementedError": NotImplementedError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "AttributeError": AttributeError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "AssertionError": AssertionError, "ArithmeticError": ArithmeticError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "SystemError": SystemError,
    "ReferenceError": ReferenceError, "MemoryError": MemoryError,
    "BufferError": BufferError, "Warning": Warning,
    "SubprocessError": _real_subprocess.SubprocessError,
    "CalledProcessError": _real_subprocess.CalledProcessError,
    "TimeoutExpired": _real_subprocess.TimeoutExpired,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "SystemExit":
        return SystemExit(spec.get("code"))
    if kind == "CalledProcessError":
        return _real_subprocess.CalledProcessError(
            spec.get("returncode", 1), spec.get("cmd", []),
            output=spec.get("output"), stderr=spec.get("stderr"))
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

    def echo(self, name, args, value):
        """Identity echo seam (path.resolve): record the event, require a
        script marker, honor a scripted raise, return the echoed input."""
        self.trace.append({"name": name, "args": args})
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, dict) and entry.get("__script__") \
                and isinstance(entry.get("raise"), dict):
            raise make_exc(entry["raise"])
        return value


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


def _which(name):
    return _ctx().call("shutil.which", {"name": name})


def _rmtree(path, ignore_errors=False, **kw):
    try:
        return _ctx().call("shutil.rmtree",
                           {"path": str(path), "ignore_errors": ignore_errors})
    except OSError:
        if ignore_errors:
            return None
        raise


def _subprocess_run(argv, **kwargs):
    ctx = _ctx()
    args = {"argv": list(argv), "capture_output": True}
    args.update(kwargs)
    r = ctx.call("subprocess.run", args)
    if kwargs.get("check") and r["returncode"] != 0:
        raise _real_subprocess.CalledProcessError(
            r["returncode"], list(argv),
            output=r.get("stdout"), stderr=r.get("stderr"))
    return types.SimpleNamespace(
        returncode=r["returncode"], stdout=r.get("stdout"), stderr=r.get("stderr"))


def _os_walk(path):
    r = _ctx().call("os.walk", {"path": str(path)})
    return [(e["root"], e["dirs"], e["files"]) for e in r]


def _psutil_process(pid):
    r = _ctx().call("psutil.Process", {"pid": pid})
    return ProcDouble(id=r["id"], pid=pid)


def _psutil_process_iter(attrs):
    r = _ctx().call("psutil.process_iter", {"attrs": list(attrs)})
    return [ProcDouble(info={"exe": e.get("exe")}) for e in r]


def _plistlib_loads(data):
    return _ctx().call("plistlib.loads", {"data": data.hex()})


def _windll(name, use_last_error=False):
    r = _ctx().call("ctypes.WinDLL",
                    {"name": name, "use_last_error": use_last_error})
    return Kernel32Double(r)


def _load_config():
    return _ctx().call("load_config", {})


def _find_release_cwd_holders(live_root):
    return _ctx().call("find_release_cwd_holders", {"live_root": str(live_root)})


def _release_after_refused_rename(live_root, consent=None):
    r = _ctx().call("release_after_refused_rename",
                    {"live_root": str(live_root), "consent": consent})
    return (r["holders"], r["closed"])


def _describe_block(held_by, release_dir):
    return _ctx().call("describe_block",
                       {"held_by": list(held_by), "release_dir": str(release_dir)})


def _holder_names(held_by):
    return _ctx().call("holder_names", {"held_by": list(held_by)})


def _desktop_install_record():
    return FakePath(_ctx().call("desktop_install_record", {}))


def _packaged_gui_app_paths():
    return [FakePath(s) for s in _ctx().call("packaged_gui_app_paths", {})]


def _read_json_or_empty(path):
    return _ctx().call("read_json_or_empty", {"path": str(path)})


def _atomic_json_write(path, data):
    return _ctx().call("atomic_json_write", {"path": str(path), "data": data})


def _get_default_hermes_root():
    return FakePath(_ctx().call("get_default_hermes_root", {}))


def _run_in_custody(root, cmd, label, cwd=None, env=None):
    return _ctx().call("run_in_custody", {
        "root": str(root), "cmd": list(cmd), "label": label,
        "cwd": str(cwd) if cwd is not None else None,
        "env": dict(env) if env is not None else None})


def _publish_stage(label):
    return _ctx().call("publish_stage", {"label": label})


def _pm_ensure(name, base_env=None):
    r = _ctx().call("pm.ensure",
                    {"name": name, "base_env": dict(base_env) if base_env is not None else None})
    return types.SimpleNamespace(env=dict(r["env"]))


def _launch_detached(cmd, cwd=None, env=None):
    return _ctx().call("launch_detached", {
        "launch_command": [str(c) for c in cmd],
        "cwd": str(cwd) if cwd is not None else None})


_stub_module("hermes_cli.config", load_config=_load_config)
_stub_module("hermes_cli.desktop_release_holders",
             find_release_cwd_holders=_find_release_cwd_holders,
             release_after_refused_rename=_release_after_refused_rename,
             describe_block=_describe_block, holder_names=_holder_names)
_stub_module("hermes_cli.gui_uninstall",
             desktop_install_record=_desktop_install_record,
             packaged_gui_app_paths=_packaged_gui_app_paths)
_stub_module("utils", atomic_json_write=_atomic_json_write,
             read_json_or_empty=_read_json_or_empty)
_stub_module("hermes_constants", get_default_hermes_root=_get_default_hermes_root)
_stub_module("hermes_cli.main", PROJECT_ROOT=FakePath("/repo"))
_stub_module("hermes_cli.source_build", run_in_custody=_run_in_custody)
_stub_module("hermes_cli.update_stage", publish_stage=_publish_stage)
_stub_module("pm", ensure=_pm_ensure)
_stub_module("hermes_cli.bundled_app", launch_detached=_launch_detached)
_stub_module("plistlib", loads=_plistlib_loads)
_stub_module("psutil", Process=_psutil_process, process_iter=_psutil_process_iter)
_stub_module("ctypes", WinDLL=_windll)


# ── extraction ────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    NS.update({
        "os": os, "sys": sys, "re": re, "json": json, "math": math,
        "types": types, "logging": logging, "posixpath": posixpath,
        "contextlib": _real_contextlib, "stat": __import__("stat"),
        "shlex": __import__("shlex"), "hashlib": __import__("hashlib"),
        "subprocess": _real_subprocess, "argparse": __import__("argparse"),
        "shutil": __import__("shutil"), "platform": __import__("platform"),
        "Path": FakePath,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "SimpleNamespace": types.SimpleNamespace,
        "facts": types.SimpleNamespace(native_arch=lambda: "amd64"),
        "_time_mod": types.SimpleNamespace(sleep=lambda d: None, time=lambda: 0),
        "desktop_launch_notice": lambda message, **kw: None,
        "desktop_console_output": lambda **kw: None,
        "__file__": SRC,
    })
    tree = ast.parse(src)

    def seed_constants():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
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
    seed_constants()

    missing = [t for t in TARGETS if t not in NS]
    if missing:
        print(f"FATAL oracle_main_desktop_flow2: missing defs: {missing}")
        sys.exit(2)


extract()

# logger double (the seeded Assign built a real logging.Logger).
NS["logger"] = types.SimpleNamespace(
    warning=lambda fmt, *a: _ctx().call("logger.warning", {"message": fmt % a}),
    info=lambda fmt, *a: _ctx().call("logger.info", {"message": fmt % a}),
)

# Claimed siblings: recording overrides (the AST pass exec'd the real defs).
NS["_desktop_packaged_executable"] = lambda desktop_dir: _fp_or_none(_ctx().call(
    "_desktop_packaged_executable", {"desktop_dir": str(desktop_dir)}))
NS["_desktop_packaged_executable_in"] = lambda release_dir: _fp_or_none(_ctx().call(
    "_desktop_packaged_executable_in", {"release_dir": str(release_dir)}))
NS["_rename_riding_out_file_lock"] = lambda src, dst: _ctx().call(
    "_rename_riding_out_file_lock", {"src": str(src), "dst": str(dst)})


def _relaunchable_fixup(desktop_dir, *, publisher_signing_configured=None,
                        release_dir=None):
    return _ctx().call("_desktop_macos_relaunchable_fixup", {
        "desktop_dir": str(desktop_dir),
        "release_dir": str(release_dir) if release_dir is not None else None})


NS["_desktop_macos_relaunchable_fixup"] = _relaunchable_fixup
NS["_desktop_exe_integrity_error"] = lambda path: _ctx().call(
    "_desktop_exe_integrity_error", {"path": str(path)})


def _swap_staged(desktop_dir, staging_dir, *, held_by=None):
    r = _ctx().call("_swap_staged_desktop_app", {
        "desktop_dir": str(desktop_dir), "staging_dir": str(staging_dir),
        "held_by": list(held_by) if held_by is not None else None})
    if isinstance(r, dict) and held_by is not None:
        held_by.extend(r.get("held_by") or [])
    exe = r.get("exe") if isinstance(r, dict) else r
    return _fp_or_none(exe)


NS["_swap_staged_desktop_app"] = _swap_staged
NS["_desktop_staging_dir"] = lambda desktop_dir: FakePath(_ctx().call(
    "_desktop_staging_dir", {"desktop_dir": str(desktop_dir)}))
NS["_stop_desktop_processes_locking_build"] = lambda desktop_dir, *, also_posix=False: \
    _ctx().call("_stop_desktop_processes_locking_build",
                {"desktop_dir": str(desktop_dir)})


def _force_adhoc(env, *, source_mode):
    r = _ctx().call("_force_adhoc_macos_signing",
                    {"env": dict(env), "source_mode": source_mode})
    if r is True:
        env["CSC_IDENTITY_AUTO_DISCOVERY"] = "false"
    return r


NS["_force_adhoc_macos_signing"] = _force_adhoc
NS["_diagnose_esbuild_ignore_scripts"] = lambda output: _ctx().call(
    "_diagnose_esbuild_ignore_scripts", {"output": output})
NS["_macos_signing_downgrade_error"] = lambda installed, rebuilt: _ctx().call(
    "_macos_signing_downgrade_error",
    {"installed": dict(installed),
     "rebuilt": dict(rebuilt) if rebuilt is not None else None})

_REAL_SYS_EXIT = sys.exit


# ── per-case installation ─────────────────────────────────────────────────
def install_case(func, inp, ctx):
    saved = {"print": builtins.print, "open": builtins.open,
             "os": NS.get("os"), "sys": NS.get("sys"),
             "shutil": NS.get("shutil"), "subprocess": NS.get("subprocess"),
             "project_root": sys.modules["hermes_cli.main"].PROJECT_ROOT}

    def patched_print(*a, sep=" ", end="\n", file=None, flush=False):
        text = sep.join(str(x) for x in a) + end
        fname = "stderr" if file is sys.stderr else "stdout"
        ctx.call("print", {"text": text, "end": end, "file": fname})
        (sys.stderr if fname == "stderr" else sys.stdout).write(text)

    def patched_open(file, mode="r", *a, **kw):
        p = str(file)
        ctx.call("open", {"path": p, "mode": mode})
        return FakeFile(p, ctx)

    builtins.print = patched_print
    builtins.open = patched_open
    NS["os"] = types.SimpleNamespace(
        path=posixpath, name="posix", walk=_os_walk,
        getpid=lambda: ctx.call("os.getpid", {}))
    NS["sys"] = types.SimpleNamespace(platform=inp.get("platform", "linux"),
                                      exit=_REAL_SYS_EXIT, stdin=None)
    NS["shutil"] = types.SimpleNamespace(which=_which, rmtree=_rmtree)
    NS["subprocess"] = types.SimpleNamespace(
        run=_subprocess_run,
        CalledProcessError=_real_subprocess.CalledProcessError,
        SubprocessError=_real_subprocess.SubprocessError,
        TimeoutExpired=_real_subprocess.TimeoutExpired,
        DEVNULL=_real_subprocess.DEVNULL)
    sys.modules["hermes_cli.main"].PROJECT_ROOT = FakePath(inp.get("project_root", "/repo"))
    return saved


def restore_case(saved):
    builtins.print = saved["print"]
    builtins.open = saved["open"]
    NS["os"] = saved["os"]
    NS["sys"] = saved["sys"]
    NS["shutil"] = saved["shutil"]
    NS["subprocess"] = saved["subprocess"]
    sys.modules["hermes_cli.main"].PROJECT_ROOT = saved["project_root"]


def drive(func, inp, ctx, held_by_ref=None):
    f = NS[func]
    state = None
    if func == "_packaged_resources_dir":
        ret = f(FakePath(inp["desktop_dir"]))
    elif func == "_recover_interrupted_swap":
        ret = f(FakePath(inp["live"]), FakePath(inp["aside"]))
    elif func == "_move_live_app_aside":
        held_by = held_by_ref["hb"]
        ret = f(FakePath(inp["live_root"]), FakePath(inp["previous"]),
                held_by, inp.get("consent"))
        state = {"held_by": held_by}
    elif func == "_discard_desktop_staging":
        ret = f(FakePath(inp["staging_dir"]))
    elif func == "_kernel32":
        ret = f()
    elif func == "_runs_from":
        ret = f(ProcDouble(id=inp["proc"]["id"]), FakePath(inp["release_dir"]))
    elif func == "_desktop_ancestor_in":
        ret = f(FakePath(inp["desktop_dir"]))
    elif func == "_sandbox_helper_lstat":
        ret = f(FakePath(inp["packaged_executable"]))
    elif func == "_stage_macos_bundle_copy":
        ret = f(FakePath(inp["src"]), FakePath(inp["dst"]))
    elif func == "_launch_installed_macos_desktop_app":
        ret = f()
    elif func == "_desktop_macos_bundle_id":
        ret = f(FakePath(inp["bundle"]))
    elif func == "_desktop_macos_local_signing_identity":
        ret = f()
    elif func == "_codesign_verify":
        ret = f(inp["codesign"], FakePath(inp["app"]), **(inp.get("kwargs") or {}))
    elif func == "_macos_signature_summary":
        ret = f(inp["codesign"], FakePath(inp["app"]))
    elif func == "_desktop_macos_has_valid_real_signature":
        ret = f(FakePath(inp["app"]))
    elif func == "_macos_legacy_adhoc_resign":
        ret = f(inp["codesign"], FakePath(inp["app"]))
    elif func == "_app_asar_hash":
        ret = f(FakePath(inp["app_path"]))
    elif func == "_swap_in_new_macos_bundle":
        ret = f(FakePath(inp["tmp"]), FakePath(inp["target"]), FakePath(inp["old"]))
    elif func == "_running_macos_app_bundles":
        ret = f()
    elif func == "_update_owned_macos_bundles":
        ret = f([FakePath(c) for c in inp["candidates"]])
    elif func == "_owns_installed_desktop_apps":
        ret = f()
    elif func == "_installed_desktop_apps":
        ret = f()
    elif func == "_install_rebuilt_macos_bundles":
        ret = f(FakePath(inp["rebuilt_app"]),
                [FakePath(c) for c in inp["candidates"]],
                running={FakePath(s) for s in inp["running"]})
    elif func == "_install_rebuilt_desktop_app":
        ret = f(FakePath(inp["desktop_dir"]),
                [FakePath(c) for c in inp["candidates"]])
    elif func == "_refresh_installed_desktop_apps":
        ret = f(FakePath(inp["desktop_dir"]))
    elif func == "_installed_desktop_launch_target":
        ret = f(FakePath(inp["desktop_dir"]), FakePath(inp["packaged_executable"]))
    elif func == "_promote_staged_desktop_app":
        ic = None
        if inp.get("integrity_check") == "custom":
            ic = lambda p: ctx.call("integrity_check", {"path": str(p)})
        ret = f(FakePath(inp["desktop_dir"]), FakePath(inp["staging_dir"]),
                integrity_check=ic)
    elif func == "build_prepared_desktop":
        icons = inp.get("icons")
        ret = f(FakePath(inp["desktop_dir"]),
                source_mode=bool(inp.get("source_mode", False)),
                pnpm=inp["pnpm"], env=dict(inp.get("env") or {}),
                icons=FakePath(icons) if icons else None)
    elif func == "_desktop_macos_local_codesign":
        ret = f(FakePath(inp["app"]), desktop_dir=FakePath(inp["desktop_dir"]),
                identity=inp.get("identity", "-"))
    else:
        raise RuntimeError(f"unknown func {func}")
    return ret, state


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
    if isinstance(v, Kernel32Double):
        return canon(v.payload)
    if isinstance(v, types.SimpleNamespace):
        return {k: canon(x) for k, x in vars(v).items()}
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
    saved = install_case(func, inp, ctx)
    ret = None
    state = None
    held_by_ref = {}
    if func == "_move_live_app_aside":
        hb = inp.get("held_by")
        held_by_ref["hb"] = list(hb) if hb is not None else None
    try:
        with _real_contextlib.redirect_stdout(cap_out), _real_contextlib.redirect_stderr(cap_err):
            ret, state = drive(func, inp, ctx, held_by_ref)
    except BaseException as exc:   # noqa: BLE001 - SystemExit included
        ret = proj_exc(exc)
        if func == "_move_live_app_aside":
            state = {"held_by": held_by_ref["hb"]}
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
        print("FATAL oracle_main_desktop_flow2: 0 cases")
        print("ORACLE main_desktop_flow2: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE main_desktop_flow2: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())