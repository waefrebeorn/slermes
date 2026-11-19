#!/usr/bin/env python3
"""Oracle for the update-lock FLOW-core port (src/cli/port_update_lock_flow.c).

Drives the LIVE hermes_cli/update_lock.py state machine with SCRIPTED I/O:
the world (pid/clock/process table), the marker mutex, marker reads, publish,
unlink, legacy claims and the UpdateLock self-methods are recording stubs fed
the same scripts the C harness feeds its seams. Compares the DECISION
SEQUENCE (ordered seam calls {"name","args"} in "trace") plus the final
return and — for the claim state machine — the mutated self/claims state.

The live defs are AST-extracted (module constants seeded, Assign AND
AnnAssign, plain-Name targets only; real dataclasses _World/UpdateHolder/
_Marker and the real MarkerBusy exception; the UpdateLock._claim_marker
method is extracted as a plain function driven with a fake self). The heavy
quarry import chain is never imported. Fail-closed: missing defs or 0 cases
=> exit 2. Summary: 'ORACLE update_lock_flow: N cases, M mismatches'.

Representation contract (shared with the C port header):
- u64 values past 2^53 -> {"__bigint__": decimal} on both sides.
- Paths are POSIX strings; every Path reaches a trace event via str() of a
  FakePath whose __str__ is the posix string (as_posix normalization —
  str(Path) flips slashes on Windows).
- Scripted OSError str(): with errno -> "[Errno N] message"; without ->
  "message" (both sides construct the exception identically; no filename).
- logger calls are silent on both sides (not traced).
- Marker bytes: fixtures avoid truncated multi-byte UTF-8 (Python groups
  replacements there; the port replaces per byte).
"""
import ast
import contextlib
import json
import math
import os
import re
import sys
import textwrap
import threading
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

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "hermes_cli", "update_lock.py")

TARGETS = [
    "_bounded_int", "_as_ct", "_incarnation", "_identity_live",
    "_parse_marker", "judge_marker", "_release_decision", "_live_partners",
    "_reclaim_dead", "_claim_marker",
]
CLASSES_NEEDED = ["_World", "UpdateHolder", "_Marker", "MarkerBusy"]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    # Class-method segments (e.g. UpdateLock._claim_marker) are extracted with
    # their class-body indentation intact; compile() rejects a leading indent.
    # Module-level defs start at col 0, so dedent is a no-op for them.
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "re": re, "json": json, "os": os, "sys": sys, "types": types,
        "contextlib": contextlib, "math": math, "threading": threading,
        "functools": functools, "logging": logging, "subprocess": subprocess,
        "calendar": calendar, "errno": errno, "secrets": secrets,
        "dataclass": dataclass, "field": field,
        "Path": Path, "PurePosixPath": PurePosixPath, "S_ISREG": S_ISREG,
        "Any": Any, "Optional": Optional,
        "SimpleNamespace": types.SimpleNamespace,
        "cache": functools.cache,
        "contextmanager": contextlib.contextmanager,
        "suppress": contextlib.suppress,
    }
    tree = ast.parse(src)
    # module constants (Assign/AnnAssign, plain Name targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # real classes (dataclasses + the MarkerBusy exception)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES_NEEDED:
            exec_node(src, node, ns, SRC)
    # all module-level defs (targets + real siblings)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # UpdateLock._claim_marker: extract the method as a plain function
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "UpdateLock":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name == "_claim_marker":
                    exec_node(src, sub, ns, SRC)
    # re-seed constants after defs
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    missing = [t for t in TARGETS if t not in ns]
    missing += [c for c in CLASSES_NEEDED if c not in ns]
    for cname in ("UPDATE_MARKER_MAX_AGE_SECONDS", "CREATE_TIME_TOLERANCE_SECONDS",
                  "_OWN_CREATE_TIME_EPSILON", "_WHOLE_SECOND_CT",
                  "EMPTY_MARKER_GRACE_SECONDS", "_U32_MAX", "_U64_MAX",
                  "_INT_LINE", "_CT_VALUE", "_CT_LINE", "_DELEGATE_LINE", "_RUN_LINE",
                  "_marker_unwritable"):
        if cname not in ns:
            missing.append(cname)
    if missing:
        print(f"FATAL oracle_update_lock_flow: missing defs: {missing}")
        sys.exit(2)
    return ns


NS = extract()


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
    return {"__raise__": type(exc).__name__, "message": str(exc)}


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
    if "raw_hex" in inp and inp["raw_hex"] is not None:
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


def marker_proj(m):
    return {
        "pid": m.pid,
        "started_at": bigint(m.started_at) if m.started_at is not None else None,
        "create_time": m.create_time,
        "delegate_pid": m.delegate_pid,
        "delegate_create_time": m.delegate_create_time,
        "in_flight": m.in_flight,
        "ct_text": m.ct_text,
        "delegate_ct_text": m.delegate_ct_text,
        "runs": list(m.runs),
        "base_hex": m.base.hex(),
        "run": m.run,
    }


def spec_to_marker(spec):
    return NS["_Marker"](
        raw=raw_bytes(spec),
        pid=spec.get("pid", -1),
        started_at=unbigint(spec.get("started_at")),
        create_time=spec.get("create_time"),
        delegate_pid=spec.get("delegate_pid"),
        delegate_create_time=spec.get("delegate_create_time"),
        in_flight=spec.get("in_flight", False),
        ct_text=spec.get("ct_text"),
        delegate_ct_text=spec.get("delegate_ct_text"),
        runs=tuple(spec.get("runs") or []),
    )


def make_world(ctx, w):
    return NS["_World"](
        pid=w["pid"],
        ct=None if w.get("ct") is None else float(w["ct"]),
        now=float(w["now"]),
        alive=lambda pid: ctx.call("world.alive", {"pid": pid}),
        ct_of=lambda pid: ctx.call("world.ct_of", {"pid": pid}),
    )


class FakePath:
    """Path double: posix str, recording mkdir/unlink, PurePosixPath parent."""
    def __init__(self, s, ctx):
        self.s = s
        self.ctx = ctx

    def __str__(self):
        return self.s

    def __fspath__(self):
        return self.s

    @property
    def parent(self):
        return FakePath(str(PurePosixPath(self.s).parent), self.ctx)

    @property
    def name(self):
        return PurePosixPath(self.s).name

    def mkdir(self, **kw):
        return self.ctx.call("path.parent.mkdir", {
            "path": str(self),
            "parents": kw.get("parents", False),
            "exist_ok": kw.get("exist_ok", False),
        })

    def unlink(self, **kw):
        return self.ctx.call("path.unlink", {"path": str(self)})


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

    def _checkout_held_elsewhere(self):
        return self.ctx.call("self._checkout_held_elsewhere", {})

    def _adopt_or_refuse(self, existing):
        # record + resolve with the scripted holder state effect (the live
        # method assigns self.holder on refusal; the seam script carries it)
        self.ctx.trace.append({"name": "self._adopt_or_refuse",
                               "args": {"marker": marker_proj(existing)}})
        entry = self.ctx.next_entry("self._adopt_or_refuse")
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            if isinstance(entry.get("holder"), dict):
                self.holder = make_holder(entry["holder"])
            return entry.get("ret")
        return entry


def install_mutex(ctx):
    @contextlib.contextmanager
    def mutex_stub(path, *, wait=10.0):
        ctx.call("mutex.enter", {"path": str(path)})
        try:
            yield
        finally:
            ctx.call("mutex.exit", {"path": str(path)})
    NS["marker_mutex"] = mutex_stub


def install_read_marker(ctx):
    def read_marker(path):
        r = ctx.call("_read_marker", {"path": str(path)})
        return None if r is None else spec_to_marker(r)
    NS["_read_marker"] = read_marker


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


def run_case(func, inp):
    ctx = Ctx(inp)
    ns = NS  # extracted defs share NS as __globals__; install per-case stubs
    ns["logger"] = _SilentLogger()
    ns["time"] = types.SimpleNamespace(
        time=lambda: ctx.call("time.time", {}),
        monotonic=lambda: ctx.call("time.monotonic", {}),
    )
    ns["os"] = types.SimpleNamespace(getpid=lambda: ctx.call("os.getpid", {}))

    def real_world():
        w = ctx.call("real_world", {})
        return make_world(ctx, w)
    ns["_real_world"] = real_world

    out = {}
    ret = None
    try:
        if func == "_bounded_int":
            limit = int(inp["limit_str"]) if "limit_str" in inp else inp.get("limit", 4294967295)
            r = ns["_bounded_int"](inp.get("text", ""), limit)
            ret = bigint(r) if r is not None else None
        elif func == "_as_ct":
            ret = ns["_as_ct"](inp.get("recorded"))
        elif func == "_incarnation":
            w = make_world(ctx, inp["world"])
            rec = inp.get("recorded")
            ret = ns["_incarnation"](inp["pid"], None if rec is None else float(rec), w)
        elif func == "_identity_live":
            w = make_world(ctx, inp["world"])
            ct = inp.get("create_time")
            ret = ns["_identity_live"](inp["pid"], None if ct is None else float(ct),
                                       float(inp["age"]), w)
        elif func == "_parse_marker":
            m = ns["_parse_marker"](raw_bytes(inp), mtime=inp.get("mtime"))
            ret = marker_proj(m)
        elif func == "judge_marker":
            w = make_world(ctx, inp["world"])
            verdict, owner, run = ns["judge_marker"](raw_bytes(inp), w)
            ret = {"verdict": verdict, "owner": owner, "run": run}
        elif func == "_release_decision":
            w = make_world(ctx, inp["world"])
            action, new = ns["_release_decision"](raw_bytes(inp), w)
            ret = {"action": action, "new": None if new is None else new.decode("utf-8")}
        elif func == "_live_partners":
            m = spec_to_marker(inp["marker"])
            ret = ns["_live_partners"](m)
        elif func == "_reclaim_dead":
            install_mutex(ctx)
            install_read_marker(ctx)
            ns["checkout_lock_held"] = lambda install_root=None: ctx.call(
                "checkout_lock_held", {"install_root": install_root})
            path = FakePath(inp["path"], ctx)
            ret = ns["_reclaim_dead"](path, inp.get("install_root"))
        elif func == "_claim_marker":
            install_mutex(ctx)
            install_read_marker(ctx)
            ns["_sweep_dead_tmp_siblings"] = lambda path: ctx.call(
                "_sweep_dead_tmp_siblings", {"path": str(path)})
            ns["legacy_profile_claims"] = lambda root=None: [
                make_holder(h) for h in ctx.call("legacy_profile_claims", {"root": str(root)})
            ]
            ns["_identity_line"] = lambda pid=None: ctx.call("_identity_line", {})
            ns["_publish_exclusive"] = lambda path, body: ctx.call(
                "_publish_exclusive", {"path": str(path), "body": body.decode("utf-8")})
            ns["_live_partners"] = lambda marker: ctx.call(
                "_live_partners", {"marker": marker_proj(marker)})

            def read_live_update(*, path=None, install_root=None):
                r = ctx.call("read_live_update", {
                    "path": None if path is None else str(path),
                    "install_root": install_root,
                })
                return make_holder(r)
            ns["read_live_update"] = read_live_update
            ns["_CLAIMS"] = dict(inp.get("claims") or {})
            self = FakeSelf(inp["self"], ctx)
            ret = ns["_claim_marker"](self)
            out["state"] = {
                "self": {
                    "path": str(self.path),
                    "install_root": self.install_root,
                    "_checkout": self._checkout,
                    "acquired": self.acquired,
                    "holder": proj_holder(self.holder),
                },
                "claims": dict(ns["_CLAIMS"]),
            }
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
        if func == "_claim_marker":
            self = locals().get("self")
            out["state"] = {
                "self": None if self is None else {
                    "path": str(self.path),
                    "install_root": self.install_root,
                    "_checkout": self._checkout,
                    "acquired": self.acquired,
                    "holder": proj_holder(self.holder),
                },
                "claims": dict(ns.get("_CLAIMS") or {}),
            }
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
        print("FATAL oracle_update_lock_flow: 0 cases")
        print("ORACLE update_lock_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE update_lock_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
