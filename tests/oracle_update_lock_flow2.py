#!/usr/bin/env python3
"""Oracle for the update-lock UNCLAIMED-remainder FLOW port
(src/cli/update_lock_flow2.c).

Drives the LIVE hermes_cli/update_lock.py functions with SCRIPTED I/O: every
collaborator (file reads/stats/iterdir/unlink, the clock, os.getpid, the process
probes, and every sibling — claimed or not — plus the marker methods and the
runtime-host imports) is a recording stub fed the same scripts the C harness
feeds its universal seam. Compares the ordered seam call sequence ("trace"),
the final return ("ret"), and — for the state-mutating functions — the mutated
self/held model ("state").

The live defs are AST-extracted (module constants seeded, Assign AND AnnAssign;
real dataclasses _World/UpdateHolder/_Marker and the real MarkerBusy exception;
the UpdateLock methods extracted as plain functions driven with a fake self).
The heavy quarry import chain is never imported. Fail-closed: missing defs or
0 cases => exit 2. Summary: 'ORACLE update_lock_flow2: N cases, M mismatches'.

Representation contract (shared with the C port header):
- Paths are POSIX strings; name/parent/with_name/join follow PurePosixPath and
  normpath follows posixpath for the clean-path fixture domain.
- marker spec / holder spec / world / bytes-as-UTF-8-string / OSError str parity
  are identical to update_lock_flow.
- sys.platform is a case input (in.platform), not a seam.
- logger calls are silent on both sides (not traced).
"""
import ast
import builtins
import contextlib
import json
import math
import os
import posixpath
import re
import sys
import textwrap
import types
import functools
import logging
import subprocess
import calendar
import errno
import secrets
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath
from stat import S_ISREG
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env safety: derive the quarry from this file (tests/ -> repo -> home).
_REPO = Path(__file__).resolve().parent.parent
_HOME = _REPO.parent
os.environ.setdefault("HOME", str(_HOME))
os.environ.setdefault("USERPROFILE", str(_HOME))
os.environ.setdefault("LOCALAPPDATA", str(_HOME / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
Q = os.path.join(str(_HOME), "hermes-agent")
SRC = os.path.join(Q, "hermes_cli", "update_lock.py")

TARGETS = [
    "_git_common_dir", "checkout_lock_path", "incarnation_live", "_real_world",
    "_identity_line", "_is_runtime_host", "_read_bytes", "_read_marker",
    "_tmp_sibling", "_tmp_writer_gone", "marker_mutex_path",
    "_sweep_dead_tmp_siblings", "_compare_and_swap", "legacy_profile_claims",
    "_lock_holder", "_lock_with_contention_wait", "_held_by_our_windows_ancestor",
    "_release_checkout", "checkout_lock_fds", "custody_spawn_kwargs",
    "holds_checkout_lock", "update_in_progress", "checkout_lock_held",
    "_marker_unwritable", "_release_marker",
    "acquire_checkout", "_checkout_held_elsewhere", "_is_partner",
    "_adopt_or_refuse", "_drop_checkout",
]
UL_METHODS = ["acquire_checkout", "_checkout_held_elsewhere", "_is_partner",
              "_adopt_or_refuse", "_drop_checkout"]
CLASSES_NEEDED = ["_World", "UpdateHolder", "_Marker", "MarkerBusy"]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def method_seg(src, node):
    # methods: EXCLUDE decorators so @staticmethod _is_partner extracts plain.
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_text(text, ns, path):
    exec(compile(textwrap.dedent(text), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "re": re, "json": json, "os": os, "sys": sys, "types": types,
        "contextlib": contextlib, "math": math, "functools": functools,
        "logging": logging, "subprocess": subprocess, "calendar": calendar,
        "errno": errno, "secrets": secrets, "posixpath": posixpath,
        "dataclass": dataclass, "field": field,
        "Path": Path, "PurePosixPath": PurePosixPath, "S_ISREG": S_ISREG,
        "Any": Any, "Optional": Optional,
        "SimpleNamespace": types.SimpleNamespace,
        "cache": functools.cache,
        "contextmanager": contextlib.contextmanager,
        "suppress": contextlib.suppress,
    }
    tree = ast.parse(src)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                with contextlib.suppress(Exception):
                    exec_text(seg(src, node), ns, SRC)

    seed_consts()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES_NEEDED:
            exec_text(seg(src, node), ns, SRC)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            with contextlib.suppress(Exception):
                exec_text(seg(src, node), ns, SRC)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "UpdateLock":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in UL_METHODS:
                    exec_text(method_seg(src, sub), ns, SRC)
    seed_consts()
    missing = [t for t in TARGETS if t not in ns]
    missing += [c for c in CLASSES_NEEDED if c not in ns]
    for cname in ("UPDATE_MARKER_MAX_AGE_SECONDS", "CREATE_TIME_TOLERANCE_SECONDS",
                  "_OWN_CREATE_TIME_EPSILON", "_WHOLE_SECOND_CT",
                  "EMPTY_MARKER_GRACE_SECONDS", "_U32_MAX", "_U64_MAX",
                  "_INT_LINE", "_CT_VALUE", "_CT_LINE", "_DELEGATE_LINE", "_RUN_LINE",
                  "MARKER_NAME", "CHECKOUT_LOCK_NAME", "GIT_CHECKOUT_LOCK_NAME",
                  "MUTEX_SUFFIX", "OWN_PID_TMP_STALE_SECONDS",
                  "CHECKOUT_CONTENTION_WAIT_SECONDS", "_WINDOWS_LOCK_OFFSET",
                  "_LEASE_SLOTS", "_MAX_ANCESTRY_DEPTH"):
        if cname not in ns:
            missing.append(cname)
    if missing:
        print(f"FATAL oracle_update_lock_flow2: missing defs: {missing}")
        sys.exit(2)
    return ns


NS = extract()
# Pristine snapshot: run_case resets NS to this each case so a recording stub
# installed for one function never leaks into the next (shared __globals__).
PRISTINE = dict(NS)


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


# ── exception materialization ─────────────────────────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "ValueError": ValueError,
    "TypeError": TypeError, "RuntimeError": RuntimeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "Exception": Exception,
    "IndexError": IndexError, "StopIteration": StopIteration,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "MarkerBusy": NS["MarkerBusy"],
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    # Project to the C universal-seam raise-spec shape: an OSError carrying an
    # errno keeps {message: strerror, errno} (the C returns the spec verbatim);
    # everything else uses str(exc).
    d = {"__raise__": type(exc).__name__}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["message"] = exc.strerror if exc.strerror is not None else str(exc)
        d["errno"] = exc.errno
    else:
        d["message"] = str(exc)
    return d


# ── per-case script backend ───────────────────────────────────────────────
class Ctx:
    def __init__(self, inp):
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


# ── data model doubles ────────────────────────────────────────────────────
def bigint(v):
    if isinstance(v, int) and v > 2**53:
        return {"__bigint__": str(v)}
    return v


def unbigint(v):
    if isinstance(v, dict) and "__bigint__" in v:
        return int(v["__bigint__"])
    return v


def raw_bytes(inp):
    if inp.get("raw_hex") is not None:
        return bytes.fromhex(inp["raw_hex"])
    return (inp.get("raw") or "").encode("utf-8")


def make_holder(spec):
    if spec is None:
        return None
    return NS["UpdateHolder"](
        pid=spec.get("pid", 0),
        age_seconds=spec.get("age_seconds", 0.0),
        reason=spec.get("reason"),
        held=spec.get("held", False),
    )


def proj_holder(h):
    if h is None:
        return None
    return {"pid": h.pid, "age_seconds": h.age_seconds, "reason": h.reason, "held": h.held}


def py_posix_join(a, b):
    # mirrors the C posix_join (clean-path domain).
    if b.startswith("/"):
        return b
    if not a:
        return b
    return a + ("" if a.endswith("/") else "/") + b


class MarkerDouble:
    """_Marker stand-in: fields from the spec, age()/live_pid() are seams."""
    def __init__(self, spec, ctx):
        self.spec = spec
        self.ctx = ctx
        self.raw = raw_bytes(spec)
        self.pid = spec.get("pid", -1)
        self.started_at = unbigint(spec.get("started_at"))
        self.create_time = spec.get("create_time")
        self.delegate_pid = spec.get("delegate_pid")
        self.delegate_create_time = spec.get("delegate_create_time")
        self.in_flight = spec.get("in_flight", False)
        self.ct_text = spec.get("ct_text")
        self.delegate_ct_text = spec.get("delegate_ct_text")
        self.runs = tuple(spec.get("runs") or [])

    @property
    def base(self):
        return b"".join(self.raw.splitlines(keepends=True)[:3])

    @property
    def run(self):
        return self.runs[0] if self.runs else None

    def age(self, world=None):
        return self.ctx.call("marker.age", {"marker": self.spec})

    def live_pid(self, world=None):
        return self.ctx.call("marker.live_pid", {"marker": self.spec})

    def proj(self):
        return self.spec


class FakePath:
    """Path double: posix str, recording fs methods, PurePosixPath name/parent."""
    def __init__(self, s, ctx):
        self.s = s
        self.ctx = ctx

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __truediv__(self, other):
        return FakePath(py_posix_join(self.s, str(other)), self.ctx)

    @property
    def parent(self):
        return FakePath(str(PurePosixPath(self.s).parent), self.ctx)

    @property
    def name(self):
        return PurePosixPath(self.s).name

    def with_name(self, n):
        return FakePath(str(PurePosixPath(self.s).with_name(n)), self.ctx)

    def is_dir(self):
        return self.ctx.call("path.is_dir", {"path": str(self)})

    def is_file(self):
        return self.ctx.call("path.is_file", {"path": str(self)})

    def read_text(self, **kw):
        return self.ctx.call("path.read_text", {"path": str(self)})

    def read_bytes(self):
        r = self.ctx.call("path.read_bytes", {"path": str(self)})
        return None if r is None else r.encode("utf-8")

    def stat(self):
        r = self.ctx.call("path.stat", {"path": str(self)})
        return types.SimpleNamespace(**r)

    def iterdir(self):
        r = self.ctx.call("path.iterdir", {"path": str(self)})
        return [FakePath(e, self.ctx) for e in r]

    def unlink(self, **kw):
        return self.ctx.call("path.unlink", {"path": str(self)})

    def mkdir(self, **kw):
        return self.ctx.call("path.parent.mkdir", {"path": str(self),
                                                  "parents": kw.get("parents", False),
                                                  "exist_ok": kw.get("exist_ok", False)})


class FakeSelf:
    def __init__(self, spec, ctx):
        self.path = FakePath(spec["path"], ctx)
        self.install_root = spec.get("install_root")
        self._checkout = spec.get("_checkout", False)
        self.acquired = spec.get("acquired", False)
        self.holder = make_holder(spec.get("holder"))
        self.ctx = ctx

    def _is_partner(self, pid):
        return self.ctx.call("self._is_partner", {"pid": pid})


def proj_self(s):
    return {"path": str(s.path), "install_root": s.install_root,
            "_checkout": s._checkout, "acquired": s.acquired,
            "holder": proj_holder(s.holder)}


class FileDouble:
    """open(tmp,'wb') stand-in: write fires the single tmp.write seam."""
    def __init__(self, tmp, ctx):
        self.tmp = tmp
        self.ctx = ctx

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def write(self, body):
        return self.ctx.call("tmp.write", {"path": str(self.tmp), "body": body.decode("utf-8")})

    def flush(self):
        return None

    def fileno(self):
        return 0


def install_mutex(ctx):
    @contextlib.contextmanager
    def mutex_stub(path, *, wait=10.0):
        ctx.call("mutex.enter", {"path": str(path)})
        try:
            yield
        finally:
            ctx.call("mutex.exit", {"path": str(path)})
    NS["marker_mutex"] = mutex_stub


def install_runtime_host_imports(ctx):
    """Stub the in-function `from gateway.status import ...` / update_cmd_windows."""
    gw = types.ModuleType("gateway"); gw.__path__ = []
    gws = types.ModuleType("gateway.status")
    gws.looks_like_gateway_command_line = lambda line: ctx.call(
        "looks_like_gateway_command_line", {"line": line})
    gw.status = gws
    hc = types.ModuleType("hermes_cli"); hc.__path__ = []
    ucw = types.ModuleType("hermes_cli.update_cmd_windows")
    ucw._hermes_holder_subcommand = lambda line: ctx.call(
        "_hermes_holder_subcommand", {"line": line})
    hc.update_cmd_windows = ucw
    sys.modules["gateway"] = gw
    sys.modules["gateway.status"] = gws
    sys.modules["hermes_cli"] = hc
    sys.modules["hermes_cli.update_cmd_windows"] = ucw


def canon(v):
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def _os_stub(ctx, **extra):
    base = dict(getpid=lambda: ctx.call("os.getpid", {}),
                close=lambda fd: ctx.call("os.close", {"fd": fd}),
                O_RDONLY=0, O_BINARY=32768)
    base.update(extra)
    return types.SimpleNamespace(**base)


def _time_stub(ctx):
    return types.SimpleNamespace(
        time=lambda: ctx.call("time.time", {}),
        monotonic=lambda: ctx.call("time.monotonic", {}),
        sleep=lambda s: ctx.call("time.sleep", {"seconds": s}),
    )


def run_case(func, inp):
    ctx = Ctx(inp)
    ns = NS
    ns.clear()
    ns.update(PRISTINE)
    ns["logger"] = _SilentLogger()
    platform = inp.get("platform", "linux")
    ns["sys"] = types.SimpleNamespace(platform=platform)
    ns["time"] = _time_stub(ctx)
    ns["os"] = _os_stub(ctx)
    ns["secrets"] = types.SimpleNamespace(
        token_hex=lambda n: ctx.call("secrets.token_hex", {"n": n}))
    ns["Path"] = PurePosixPath
    out = {}
    ret = None
    self = None
    stateful = func in ("_release_checkout", "acquire_checkout",
                        "_adopt_or_refuse", "_drop_checkout")
    try:
        if func == "_git_common_dir":
            ns["os"] = types.SimpleNamespace(path=types.SimpleNamespace(normpath=posixpath.normpath))
            ret = ns["_git_common_dir"](FakePath(inp["root"], ctx))
            ret = None if ret is None else str(ret)
        elif func == "checkout_lock_path":
            ns["_default_install_root"] = lambda: ctx.call("_default_install_root", {})
            ns["_git_common_dir"] = lambda root: (
                lambda r: None if r is None else PurePosixPath(r))(
                    ctx.call("_git_common_dir", {"root": str(root)}))
            r = ns["checkout_lock_path"](inp.get("install_root"))
            ret = str(r)
        elif func == "incarnation_live":
            ns["_as_ct"] = lambda recorded: ctx.call("_as_ct", {"recorded": recorded})
            ns["_real_world"] = lambda: ctx.call("_real_world", {})
            ns["_incarnation"] = lambda pid, recorded, w: ctx.call(
                "_incarnation", {"pid": pid, "recorded": recorded, "world": w})
            ret = ns["incarnation_live"](inp.get("pid"), inp.get("recorded_ct"))
        elif func == "_real_world":
            ns["_own_create_time"] = lambda: ctx.call("_own_create_time", {})
            ns["_pid_alive"] = lambda pid: ctx.call("_pid_alive", {"pid": pid})
            ns["process_create_time"] = lambda pid=None: ctx.call("process_create_time", {"pid": pid})
            w = ns["_real_world"]()
            ret = {"pid": w.pid, "ct": w.ct, "now": w.now}
        elif func == "_identity_line":
            ns["process_create_time"] = lambda pid=None: ctx.call("process_create_time", {"pid": pid})
            ret = ns["_identity_line"](inp.get("pid"))
        elif func == "_is_runtime_host":
            install_runtime_host_imports(ctx)
            ret = ns["_is_runtime_host"](inp.get("cmdline") or [])
        elif func == "_read_bytes":
            r = ns["_read_bytes"](FakePath(inp["path"], ctx))
            ret = None if r is None else r.decode("utf-8")
        elif func == "_read_marker":
            ns["_read_bytes"] = lambda path: (
                lambda r: None if r is None else r.encode("utf-8"))(
                    ctx.call("_read_bytes", {"path": str(path)}))
            ns["_parse_marker"] = lambda raw, *, mtime=None: MarkerDouble(
                ctx.call("_parse_marker", {"raw": raw.decode("utf-8"), "mtime": mtime}), ctx)
            m = ns["_read_marker"](FakePath(inp["path"], ctx))
            ret = None if m is None else m.proj()
        elif func == "_tmp_sibling":
            r = ns["_tmp_sibling"](FakePath(inp["path"], ctx))
            ret = str(r)
        elif func == "_tmp_writer_gone":
            ns["_pid_alive"] = lambda pid: ctx.call("_pid_alive", {"pid": pid})
            ret = ns["_tmp_writer_gone"](inp.get("owner"), FakePath(inp["entry"], ctx))
        elif func == "marker_mutex_path":
            ret = str(ns["marker_mutex_path"](FakePath(inp["path"], ctx)))
        elif func == "_sweep_dead_tmp_siblings":
            ns["_bounded_int"] = lambda text, limit: ctx.call("_bounded_int", {"text": text, "limit": limit})
            ns["_tmp_writer_gone"] = lambda owner, entry: ctx.call(
                "_tmp_writer_gone", {"owner": owner, "entry": str(entry)})
            ret = ns["_sweep_dead_tmp_siblings"](FakePath(inp["path"], ctx))
        elif func == "_compare_and_swap":
            install_mutex(ctx)
            ns["_read_bytes"] = lambda path: (
                lambda r: None if r is None else r.encode("utf-8"))(
                    ctx.call("_read_bytes", {"path": str(path)}))
            ns["_tmp_sibling"] = lambda path: FakePath(ctx.call("_tmp_sibling", {"path": str(path)}), ctx)
            ns["open"] = lambda tmp, mode: FileDouble(tmp, ctx)
            ns["os"] = _os_stub(ctx, fsync=lambda fd: None,
                                replace=lambda tmp, path: ctx.call("os.replace", {"tmp": str(tmp), "path": str(path)}))
            ret = ns["_compare_and_swap"](FakePath(inp["path"], ctx),
                                          (inp.get("expected") or "").encode("utf-8"),
                                          (inp.get("new") or "").encode("utf-8"))
        elif func == "legacy_profile_claims":
            ns["update_marker_path"] = lambda: FakePath(ctx.call("update_marker_path", {}), ctx)
            ns["_read_marker"] = lambda path: (
                lambda r: None if r is None else MarkerDouble(r, ctx))(
                    ctx.call("_read_marker", {"path": str(path)}))
            claims = ns["legacy_profile_claims"](
                None if inp.get("root") is None else FakePath(inp["root"], ctx))
            ret = [proj_holder(h) for h in claims]
        elif func == "_lock_holder":
            ns["_read_bytes"] = lambda path: (
                lambda r: None if r is None else r.encode("utf-8"))(
                    ctx.call("_read_bytes", {"path": str(path)}))
            ns["_parse_marker"] = lambda raw, *, mtime=None: MarkerDouble(
                ctx.call("_parse_marker", {"raw": raw.decode("utf-8"), "mtime": mtime}), ctx)
            ns["incarnation_live"] = lambda pid, recorded_ct=None: ctx.call(
                "incarnation_live", {"pid": pid, "recorded_ct": recorded_ct})
            ret = proj_holder(ns["_lock_holder"](FakePath(inp["path"], ctx)))
        elif func == "_lock_with_contention_wait":
            ns["_try_lock"] = lambda fd: ctx.call("_try_lock", {"fd": fd})
            ns["_held_by_our_windows_ancestor"] = lambda path: ctx.call(
                "_held_by_our_windows_ancestor", {"path": str(path)})
            ret = ns["_lock_with_contention_wait"](inp["fd"], FakePath(inp["path"], ctx))
        elif func == "_held_by_our_windows_ancestor":
            ns["_read_bytes"] = lambda path: (
                lambda r: None if r is None else r.encode("utf-8"))(
                    ctx.call("_read_bytes", {"path": str(path)}))
            ns["_parse_marker"] = lambda raw, *, mtime=None: MarkerDouble(
                ctx.call("_parse_marker", {"raw": raw.decode("utf-8"), "mtime": mtime}), ctx)
            ns["incarnation_live"] = lambda pid, recorded_ct=None: ctx.call(
                "incarnation_live", {"pid": pid, "recorded_ct": recorded_ct})
            ns["_is_ancestor_pid"] = lambda pid: ctx.call("_is_ancestor_pid", {"pid": pid})
            ret = ns["_held_by_our_windows_ancestor"](FakePath(inp["path"], ctx))
        elif func == "_release_checkout":
            ns["_HELD"] = inp.get("held")
            ns["_unlock"] = lambda fd, offset=NS["_WINDOWS_LOCK_OFFSET"], count=1: ctx.call(
                "_unlock", {"fd": fd, "offset": offset, "count": count})
            ret = ns["_release_checkout"]()
            out["state"] = {"self": None, "held": ns["_HELD"]}
        elif func == "checkout_lock_fds":
            ns["_HELD"] = inp.get("held")
            ns["checkout_lock_path"] = lambda install_root=None: FakePath(
                ctx.call("checkout_lock_path", {"install_root": install_root}), ctx)
            ns["_inherited_lock_fd"] = lambda path: ctx.call("_inherited_lock_fd", {"path": str(path)})
            ret = list(ns["checkout_lock_fds"](inp.get("install_root")))
        elif func == "custody_spawn_kwargs":
            ns["_HELD"] = inp.get("held")
            r = ns["custody_spawn_kwargs"]()
            ret = {k: list(v) if isinstance(v, tuple) else v for k, v in r.items()}
        elif func == "holds_checkout_lock":
            ns["_HELD"] = inp.get("held")
            ns["os"] = _os_stub(ctx, path=types.SimpleNamespace(
                realpath=lambda p: ctx.call("os.path.realpath", {"path": str(p)})))
            ns["checkout_lock_path"] = lambda install_root=None: FakePath(
                ctx.call("checkout_lock_path", {"install_root": install_root}), ctx)
            ret = ns["holds_checkout_lock"](inp.get("install_root"))
        elif func == "update_in_progress":
            ns["read_live_update"] = lambda *, path=None, install_root=None: make_holder(
                ctx.call("read_live_update", {"path": path, "install_root": install_root}))
            ns["checkout_lock_held"] = lambda install_root=None: ctx.call(
                "checkout_lock_held", {"install_root": install_root})
            ret = ns["update_in_progress"](inp.get("install_root"))
        elif func == "checkout_lock_held":
            ns["holds_checkout_lock"] = lambda install_root=None: ctx.call(
                "holds_checkout_lock", {"install_root": install_root})
            ns["checkout_lock_path"] = lambda install_root=None: FakePath(
                ctx.call("checkout_lock_path", {"install_root": install_root}), ctx)
            ns["os"] = _os_stub(ctx, open=lambda path, flags: ctx.call(
                "os.open", {"path": str(path), "flags": flags}))
            ns["_try_shared_lock"] = lambda fd: ctx.call("_try_shared_lock", {"fd": fd})
            ret = ns["checkout_lock_held"](inp.get("install_root"))
        elif func == "_marker_unwritable":
            ret = ns["_marker_unwritable"](FakePath(inp["path"], ctx), make_exc(inp["exc"]))
        elif func == "_release_marker":
            install_mutex(ctx)
            ns["_read_bytes"] = lambda path: (
                lambda r: None if r is None else r.encode("utf-8"))(
                    ctx.call("_read_bytes", {"path": str(path)}))
            def release_decision(raw):
                r = ctx.call("_release_decision", {"raw": raw.decode("utf-8")})
                return (r["action"], None if r.get("new") is None else r["new"].encode("utf-8"))
            ns["_release_decision"] = release_decision
            ns["_compare_and_swap"] = lambda path, expected, new: ctx.call(
                "_compare_and_swap", {"path": str(path), "expected": expected.decode("utf-8"),
                                      "new": new.decode("utf-8")})
            ret = ns["_release_marker"](FakePath(inp["path"], ctx))
        elif func == "acquire_checkout":
            self = FakeSelf(inp["self"], ctx)
            ns["_acquire_checkout"] = lambda install_root: make_holder(
                ctx.call("_acquire_checkout", {"install_root": str(install_root)}))
            ret = ns["acquire_checkout"](self, inp.get("install_root"))
            out["state"] = {"self": proj_self(self), "held": inp.get("held")}
        elif func == "_checkout_held_elsewhere":
            self = FakeSelf(inp["self"], ctx)
            ns["_HELD"] = inp.get("held")
            ns["checkout_lock_path"] = lambda install_root=None: FakePath(
                ctx.call("checkout_lock_path", {"install_root": install_root}), ctx)
            ns["checkout_lock_held"] = lambda install_root=None: ctx.call(
                "checkout_lock_held", {"install_root": install_root})
            ns["_inherited_lock_fd"] = lambda path: ctx.call("_inherited_lock_fd", {"path": str(path)})
            ret = ns["_checkout_held_elsewhere"](self)
        elif func == "_is_partner":
            ns["_handoff_pid"] = lambda: ctx.call("_handoff_pid", {})
            ns["_is_ancestor_pid"] = lambda pid: ctx.call("_is_ancestor_pid", {"pid": pid})
            ret = ns["_is_partner"](inp["pid"])
        elif func == "_adopt_or_refuse":
            self = FakeSelf(inp["self"], ctx)
            existing = MarkerDouble(inp["marker"], ctx)
            ns["_live_partners"] = lambda marker: ctx.call("_live_partners", {"marker": marker.proj()})
            ns["_runtime_host_below"] = lambda pid: ctx.call("_runtime_host_below", {"pid": pid})
            ns["_identity_line"] = lambda pid=None: ctx.call("_identity_line", {})
            ns["_compare_and_swap"] = lambda path, expected, new: ctx.call(
                "_compare_and_swap", {"path": str(path), "expected": expected.decode("utf-8"),
                                      "new": new.decode("utf-8")})
            ret = ns["_adopt_or_refuse"](self, existing)
            out["state"] = {"self": proj_self(self), "held": inp.get("held")}
        elif func == "_drop_checkout":
            self = FakeSelf(inp["self"], ctx)
            ns["_release_checkout"] = lambda: ctx.call("_release_checkout", {})
            ret = ns["_drop_checkout"](self)
            out["state"] = {"self": proj_self(self), "held": inp.get("held")}
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity (incl. BaseException trio)
        ret = proj_exc(exc)
        if func == "_release_checkout":
            out["state"] = {"self": None, "held": ns.get("_HELD")}
        elif func in ("acquire_checkout", "_adopt_or_refuse", "_drop_checkout") and self is not None:
            out["state"] = {"self": proj_self(self), "held": inp.get("held")}
    out["ret"] = ret
    out["trace"] = ctx.trace
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
        try:
            py_out = run_case(func, inp)
        except Exception as exc:  # noqa: BLE001
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
        print("FATAL oracle_update_lock_flow2: 0 cases")
        print("ORACLE update_lock_flow2: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE update_lock_flow2: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
