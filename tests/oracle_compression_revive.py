#!/usr/bin/env python3
"""oracle_compression_revive.py — differential oracle for the conversation_compression revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs, seeds module constants (Assign + AnnAssign), drives each live
function under the SAME scripted seam results the C harness consumed, and
compares ordered trace + ret + post-state. BaseException is caught per case
(SystemExit included); missing scripts fail closed on both sides.

Gate:
  bash tests/run_one_oracle.sh compression_revive oracle_compression_revive.py "" "src/agent/conversation_compression_gaps.o src/agent/conversation_compression_gaps2.o lib/libjson/json.o"
"""
import ast
import builtins
import copy
import __future__
import json
import math
import os
import re
import sys
import threading as _threading_real
import types
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_ccr"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints (libjson emits %.0f); tuples -> lists; Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, Path):
        return x.as_posix()
    return x


class LockDouble:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class EventDouble:
    def __init__(self):
        self._s = False

    def set(self):
        self._s = True

    def is_set(self):
        return self._s

    def clear(self):
        self._s = False

    def wait(self, timeout=None):
        return self._s


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        self.exc = cls(msg)


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return entry.get("ret")
        return entry


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), want_classes=(), strict_consts=True):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_ccr_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts(strict):
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                if strict:
                    raise

    seed_consts(False)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in want_classes:
            exec(compile_node(n, relpath), ns)
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            exec(compile_node(n, relpath), ns)
    seed_consts(strict_consts)
    for name in list(want_funcs) + list(want_classes):
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "List": object, "Literal": object, "Tuple": object, "Mapping": object,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "threading": types.SimpleNamespace(Lock=LockDouble, local=_threading_real.local,
                                           Event=EventDouble),
        "logging": logging_double,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "contextvars": types.SimpleNamespace(ContextVar=type("ContextVar", (), {
            "__init__": lambda self, name, default=None: setattr(self, "default", default),
            "get": lambda self: self.default,
        })),
        "math": math, "re": re, "json": json, "copy": copy,
        "os": types.SimpleNamespace(environ={}),
        "sys": sys,
    }


# ── module namespaces (built once) ──────────────────────────────────────────

NS_CC = load_ns(
    "agent/conversation_compression.py", base_seeds("quarry_ccr_cc"),
    want_funcs=("context_compression_timed_out", "_get_context_compression_timeout_state",
                "compression_blocked_transiently", "compression_attempt_stalled"),
    want_classes=("CompressionCommitFence",), strict_consts=False)

NS_CODEX = load_ns(
    "agent/conversation_compression_codex.py", base_seeds("quarry_ccr_codex"),
    want_funcs=("_codex_compaction_cooldown_remaining",), strict_consts=False)

NS_CKP = load_ns(
    "agent/conversation_compression_checkpoint.py", base_seeds("quarry_ccr_ckp"),
    want_funcs=("_checkpoint_blocked",),
    want_classes=("CompressionCheckpointUnavailable",), strict_consts=False)


# ── recording seam factory ──────────────────────────────────────────────────

def make_rec(script, trace, armed):
    def rec(name, argb, make=None):
        def call(*a, **k):
            r = script.next(name)
            ev = {"ev": name, "args": argb(a, k), "ret": None}
            if isinstance(r, Raise):
                ev["ret"] = r.spec
                if armed[0]:
                    trace.append(ev)
                raise r.exc
            ev["ret"] = norm(r)
            if armed[0]:
                trace.append(ev)
            return make(r) if make else r
        return call
    return rec


def minus_io(work):
    work.pop("io", None)
    return work


# ── attribute doubles ───────────────────────────────────────────────────────

def attr_double(x, rec):
    """JSON object -> SimpleNamespace attribute double. Special markers:
    {"__callable__": true} -> scripted getter recorder;
    {"__local__": true, ...} -> real threading.local with the remaining attrs;
    {"__lock__": true} -> LockDouble."""
    if isinstance(x, dict):
        if x.get("__callable__") is True:
            return rec("get_active_compression_failure_cooldown",
                       lambda a, k: {"refresh": k.get("refresh")})
        if x.get("__local__") is True:
            loc = _threading_real.local()
            for k, v in x.items():
                if k != "__local__":
                    setattr(loc, k, v)
            return loc
        if x.get("__lock__") is True:
            return LockDouble()
        return types.SimpleNamespace(**{k: attr_double(v, rec) for k, v in x.items()})
    if isinstance(x, list):
        return [attr_double(v, rec) for v in x]
    return x


def de_double(x):
    """Inverse of attr_double for post-state writeback."""
    if isinstance(x, LockDouble):
        return {"__lock__": True}
    if isinstance(x, _threading_real.local):
        return {"__local__": True, **vars(x)}
    if isinstance(x, types.SimpleNamespace):
        return {k: de_double(v) for k, v in vars(x).items()}
    if isinstance(x, list):
        return [de_double(v) for v in x]
    return x


def build_fence(raw):
    """dict -> real CompressionCommitFence with fixture attrs; else raw self.
    __init__ calls time.monotonic(); construct under a CONSTANT clock so a
    recorder left in the shared namespace by an earlier case cannot fire."""
    if isinstance(raw, dict):
        saved = NS_CC.get("time")
        NS_CC["time"] = types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0)
        try:
            f = NS_CC["CompressionCommitFence"]()
        finally:
            NS_CC["time"] = saved
        for k, v in raw.items():
            setattr(f, k, v)
        return f
    return raw


# ── per-function drivers ────────────────────────────────────────────────────

def d_codex(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_CODEX["logger"] = types.SimpleNamespace(
        debug=rec("logger.debug", lambda a, k: {"msg": a[0]}))
    agent = attr_double(work.get("agent"), rec)
    return NS_CODEX["_codex_compaction_cooldown_remaining"](agent)


def d_timed_out(work, trace, script):
    raw = work.get("agent")
    if isinstance(raw, dict):
        agent = attr_double(raw, None)
        try:
            return NS_CC["context_compression_timed_out"](agent)
        finally:
            work["agent"] = de_double(agent)
    return NS_CC["context_compression_timed_out"](raw)


def d_blocked(work, trace, script):
    raw = work.get("agent")
    agent = attr_double(raw, None) if isinstance(raw, dict) else raw
    return NS_CC["compression_blocked_transiently"](agent)


def d_checkpoint(work, trace, script):
    exc = NS_CKP["_checkpoint_blocked"](work.get("reason"))
    return {"__exc_class__": type(exc).__name__, "message": str(exc)}


def d_stalled(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_CC["resolve_context_compression_timeouts"] = rec(
        "resolve_context_compression_timeouts", lambda a, k: {})
    NS_CC["time"] = types.SimpleNamespace(monotonic=rec("time.monotonic", lambda a, k: {}))
    fr = work.get("commit_fence")
    if isinstance(fr, dict):
        fence = types.SimpleNamespace(
            seconds_since_progress=rec("seconds_since_progress", lambda a, k: {}))
    else:
        fence = fr
    return NS_CC["compression_attempt_stalled"](
        commit_fence=fence, started_at=work.get("started_at"),
        idle_timeout_seconds=work.get("idle_timeout_seconds"))


def d_deadline(work, trace, script):
    rec = make_rec(script, trace, [True])
    fence = build_fence(work.get("fence"))  # construction uses constant time
    NS_CC["time"] = types.SimpleNamespace(monotonic=rec("time.monotonic", lambda a, k: {}))
    return NS_CC["CompressionCommitFence"].deadline_exceeded.fget(fence)


def d_progress(work, trace, script):
    fence = build_fence(work.get("fence"))
    return NS_CC["CompressionCommitFence"].progress_observed.fget(fence)


def d_watermark(work, trace, script):
    fence = build_fence(work.get("fence"))
    return NS_CC["CompressionCommitFence"].commit_watermark_fenced.fget(fence)


DRIVERS = {
    "codex_compaction_cooldown_remaining": d_codex,
    "context_compression_timed_out": d_timed_out,
    "compression_blocked_transiently": d_blocked,
    "checkpoint_blocked": d_checkpoint,
    "compression_attempt_stalled": d_stalled,
    "deadline_exceeded": d_deadline,
    "progress_observed": d_progress,
    "commit_watermark_fenced": d_watermark,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:  # SystemExit included — fail closed per case
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        minus_io(work)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}


def main():
    n = 0
    mismatches = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print(f"BAD JSON from harness: {e}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        driver = DRIVERS.get(func)
        if driver is None:
            mismatches += 1
            print(f"UNKNOWN func {func!r}")
            continue
        if not isinstance(got, dict) or "error" in got:
            mismatches += 1
            print(f"HARNESS ERROR {func}: {got}")
            continue
        try:
            want = run_case(driver, inp)
        except BaseException as e:
            mismatches += 1
            print(f"ORACLE DRIVER FAILURE {func}: {type(e).__name__}: {e}")
            continue
        if got != want:
            mismatches += 1
            if shown < 8:
                shown += 1
                print(f"MISMATCH {func} case#{n}")
                for key in ("ret", "trace", "state"):
                    if got.get(key) != want.get(key):
                        print(f"  {key} C  : {json.dumps(got.get(key), ensure_ascii=False)[:600]}")
                        print(f"  {key} PY : {json.dumps(want.get(key), ensure_ascii=False)[:600]}")
    print(f"ORACLE compression_revive: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()