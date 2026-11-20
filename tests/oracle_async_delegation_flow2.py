#!/usr/bin/env python3
"""Oracle for src/tools/async_delegation_flow2.c — drives the LIVE unclaimed
remainder of tools/async_delegation.py under IDENTICAL scripted universal seams.

Reads harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes the
LIVE reference from rec["in"] and compares ordered trace / return / state.
The live module is imported directly (quarry venv); module globals and the
in-function collaborators are replaced with recording doubles fed the SAME
scripts the C harness feeds. Claimed names stay SEAMED (never re-ported).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. Scripted
BaseExceptions use the real builtin classes and propagate past except Exception.

Value model: state keys are full skeletons on every case; frozenset/set state
echoes are sorted; integral floats normalize to ints on both sides; callables
project to {"__callable__": seam}; raise specs are {"__raise__", "message"}.

Summary: 'ORACLE async_delegation_flow2: N cases, M mismatches'.
"""
import ast
import builtins
import collections
import copy
import json
import math
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "tools" / "async_delegation.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
sys.path.insert(0, str(QUARRY))

import tools.async_delegation as MOD  # noqa: E402

FUNCS = [
    "_single_crash", "_batch_crash", "_batch_status", "_stalled_error_text",
    "_stall_meta", "_stalled_result", "_keep_finished_children", "_recovered_results",
    "is_interim_delegation_event", "completion_copy_id", "_call_interrupt",
    "mark_copy_held", "unmark_copy_held", "refused_for_session", "allow_for_session",
    "_unfence", "refuse_for_session", "_session_records", "_queued_completion_ids", "_copy_may_be_live",
    "_note_reoffers", "maybe_sweep_orphaned_completions", "_event_delivery",
    "return_completion_offer",
]

CURRENT = {"rec": None}

# ---- builtin exception table (real classes, py3.14 gate interpreter) ----
EXC_CLASSES = {}
for _n in dir(builtins):
    _o = getattr(builtins, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        EXC_CLASSES[_n] = _o
try:
    import asyncio as _asyncio
    EXC_CLASSES["CancelledError"] = _asyncio.CancelledError
except Exception:
    pass


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    if kind in ("BaseExceptionGroup", "ExceptionGroup"):
        sub = SystemExit("sub") if kind == "BaseExceptionGroup" else ValueError("sub")
        return cls(msg, [sub])
    return cls(msg)


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


_MISSING = object()


def _unwrap_script(val):
    if isinstance(val, dict) and "__raise__" in val:
        return Raise(val, build_exc(val))
    if isinstance(val, dict) and val.get("__script__") is True:
        return val.get("ret")
    return val


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return _unwrap_script(val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        return Raise(spec, build_exc(spec))


def raise_spec(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def norm(v):
    if isinstance(v, enum_enum()):
        return v.value
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, set):
        return sorted(norm(x) for x in v)
    if isinstance(v, frozenset):
        return sorted(norm(x) for x in v)
    if isinstance(v, collections.Counter):
        return {str(k): norm(x) for k, x in v.items()}
    if isinstance(v, SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not k.startswith("__ad2")}
    if callable(v):
        return {"__callable__": getattr(v, "__ad2_seam__", True)}
    return v


def enum_enum():
    import enum
    return enum.Enum


def norm_args(v):
    v = norm(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj(name, a, k):
    if name.startswith("logger."):
        d = {"msg": a[0] if a else "",
             "args": [str(x) if isinstance(x, BaseException) else norm(x) for x in a[1:]]}
        if k.get("exc_info"):
            d["exc_info"] = True
        return d
    if name == "hermes_home_key":
        return {"path": a[0] if a else k.get("path")}
    if name in ("os.getpid", "time.time", "time.monotonic", "uuid.uuid4",
                "sweep_orphaned_completions"):
        return {}
    if name in ("claim_completion_delivery", "complete_completion_delivery",
                "release_completion_delivery"):
        return {"delegation_id": a[0] if a else k.get("delegation_id"),
                "claim_id": a[1] if len(a) > 1 else k.get("claim_id")}
    if name == "deliver_fn":
        return {"delegation_id": a[0] if a else k.get("delegation_id"),
                "claim_id": a[1] if len(a) > 1 else k.get("claim_id")}
    if name == "interrupt_fn":
        return {}
    return {**{("arg%d" % i): norm(x) for i, x in enumerate(a)}, **k}


class NoLock:
    def __enter__(self): return self
    def __exit__(self, *a): return False


class LoggerDouble:
    def __init__(self, rec):
        self._rec = rec

    def _log(self, level, msg, args, kw):
        return self._rec("logger." + level)(msg, *args, **kw)

    def debug(self, msg, *a, **k): return self._log("debug", msg, a, k)
    def info(self, msg, *a, **k): return self._log("info", msg, a, k)
    def warning(self, msg, *a, **k): return self._log("warning", msg, a, k)
    def error(self, msg, *a, **k): return self._log("error", msg, a, k)
    def exception(self, msg, *a, **k): return self._log("exception", msg, a, k)


def materialize(v):
    if isinstance(v, dict):
        cb = v.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        if "__id__" in v:
            ns = SimpleNamespace(**{k: materialize(x) for k, x in v.items() if k != "__id__"})
            ns.__ad2_id__ = v["__id__"]
            return ns
        return {k: materialize(x) for k, x in v.items()}
    if isinstance(v, list):
        return [materialize(x) for x in v]
    return v


def make_recorder(name, script, trace):
    def call(*a, **k):
        args = norm_args(proj(name, a, k))
        raw = script.next(name)
        if isinstance(raw, Raise):
            trace.append({"ev": name, "args": args, "ret": raw.spec})
            raise raw.exc
        trace.append({"ev": name, "args": args, "ret": norm(raw)})
        if name == "uuid.uuid4" and isinstance(raw, dict):
            return SimpleNamespace(**raw)
        return materialize(raw)
    call.__ad2_seam__ = name
    return call


STATE_KEYS = ("records", "refused", "refused_by", "offered", "startup_replayed",
              "reoffered_at", "held_copies", "last_orphan_sweep")
LOCK_NAMES = ("_records_lock", "_orphan_lock", "_held_lock", "_executor_lock",
              "_monitor_lock", "_DB_LOCK")


def load_state(sj):
    MOD._records = copy.deepcopy(sj.get("records", {}))
    MOD._refused = {tok: frozenset(
        (tuple(x) if isinstance(x, (list, tuple)) else tuple(x.split("|", 1)))
        for x in sels) for tok, sels in sj.get("refused", {}).items()}
    by = {}
    for key, toks in sj.get("refused_by", {}).items():
        f, v = key.split("|", 1)
        by[(f, v)] = set(toks)
    MOD._refused_by = by
    MOD._offered = set((h, i) for h, i in sj.get("offered", []))
    MOD._startup_replayed = set(sj.get("startup_replayed", []))
    ra = {}
    for key, t in sj.get("reoffered_at", {}).items():
        h, i = key.split("|", 1)
        ra[(h, i)] = t
    MOD._reoffered_at = ra
    MOD._held_copies = collections.Counter(sj.get("held_copies", {}))
    MOD._last_orphan_sweep = dict(sj.get("last_orphan_sweep", {}))


def dump_state():
    return {
        "records": norm(MOD._records),
        "refused": {tok: sorted([f + "|" + str(v) for f, v in sels])
                    for tok, sels in MOD._refused.items()},
        "refused_by": {f + "|" + str(v): sorted(toks)
                       for (f, v), toks in MOD._refused_by.items()},
        "offered": sorted([list(x) for x in MOD._offered]),
        "startup_replayed": sorted(MOD._startup_replayed),
        "reoffered_at": {h + "|" + i: t for (h, i), t in MOD._reoffered_at.items()},
        "held_copies": {str(k): v for k, v in MOD._held_copies.items()},
        "last_orphan_sweep": dict(MOD._last_orphan_sweep),
    }


# ---- live invocation per function ----
def drive(func, inp):
    stj = inp.get("state", {})
    for key in STATE_KEYS:
        stj.setdefault(key, {} if key not in ("offered", "startup_replayed") else [])
    load_state(stj)

    script = Script(inp.get("io"))
    trace = []
    CURRENT["rec"] = lambda name: make_recorder(name, script, trace)
    rec = CURRENT["rec"]

    saved = {}
    patch = {
        "logger": LoggerDouble(rec),
        "os": SimpleNamespace(getpid=rec("os.getpid"), getcwd=rec("os.getcwd")),
        "time": SimpleNamespace(time=rec("time.time"), monotonic=rec("time.monotonic")),
        "uuid": SimpleNamespace(uuid4=rec("uuid.uuid4")),
        "hermes_home_key": rec("hermes_home_key"),
        "get_hermes_home": rec("get_hermes_home"),
        "claim_completion_delivery": rec("claim_completion_delivery"),
        "complete_completion_delivery": rec("complete_completion_delivery"),
        "release_completion_delivery": rec("release_completion_delivery"),
        "sweep_orphaned_completions": rec("sweep_orphaned_completions"),
    }
    for nm in LOCK_NAMES:
        patch[nm] = NoLock()
    for nm, val in patch.items():
        saved[nm] = getattr(MOD, nm, _MISSING)
        setattr(MOD, nm, val)

    try:
        ret = invoke(func, inp, rec)
        out = {"ret": norm(ret), "trace": trace, "st": dump_state()}
    except BaseException as e:
        out = {"ret": raise_spec(e), "trace": trace, "st": dump_state()}
    finally:
        for nm, val in saved.items():
            if val is _MISSING:
                try: delattr(MOD, nm)
                except AttributeError: pass
            else:
                setattr(MOD, nm, val)
        CURRENT["rec"] = None
    return out


def g(inp, k, default=None):
    return inp.get(k, default)


def invoke(func, inp, rec):
    if func == "_single_crash":
        return MOD._single_crash(g(inp, "error"), g(inp, "duration"))
    if func == "_batch_crash":
        return MOD._batch_crash(g(inp, "error"), g(inp, "duration"))
    if func == "_batch_status":
        return MOD._batch_status(g(inp, "combined", {}))
    if func == "_stalled_error_text":
        return MOD._stalled_error_text(g(inp, "event_record", {}))
    if func == "_stall_meta":
        return MOD._stall_meta(g(inp, "event_record", {}))
    if func == "_stalled_result":
        return MOD._stalled_result(g(inp, "delegation_id", ""), g(inp, "event_record", {}))
    if func == "_keep_finished_children":
        return MOD._keep_finished_children(g(inp, "record", {}), g(inp, "result", {}), g(inp, "status", ""))
    if func == "_recovered_results":
        return MOD._recovered_results(g(inp, "task", {}), g(inp, "stored"), g(inp, "error", ""))
    if func == "is_interim_delegation_event":
        return MOD.is_interim_delegation_event(g(inp, "evt", {}))
    if func == "completion_copy_id":
        return MOD.completion_copy_id(g(inp, "evt", {}))
    if func == "_call_interrupt":
        fn = materialize(g(inp, "fn"))
        return MOD._call_interrupt(fn, g(inp, "msg", ""), *g(inp, "cargs", []))
    if func == "mark_copy_held":
        return MOD.mark_copy_held(g(inp, "delegation_id", ""))
    if func == "unmark_copy_held":
        return MOD.unmark_copy_held(g(inp, "delegation_id", ""))
    if func == "refused_for_session":
        return MOD.refused_for_session(g(inp, "token"))
    if func == "allow_for_session":
        return MOD.allow_for_session(g(inp, "token"))
    if func == "_unfence":
        return MOD._unfence(g(inp, "token"))
    if func == "refuse_for_session":
        return MOD.refuse_for_session(g(inp, "token"), session_key=g(inp, "session_key", ""),
                                      parent_session_ids=tuple(g(inp, "parent_session_ids", [])))
    if func == "_session_records":
        return MOD._session_records(g(inp, "statuses", []), g(inp, "session_key", ""),
                                    g(inp, "origin_ui_session_id", ""), g(inp, "parent_session_id", ""))
    if func == "_queued_completion_ids":
        def matq(q):
            if isinstance(q, dict):
                mutex = NoLock() if q.get("mutex") is not None else None
                queue = [materialize(x) for x in q["queue"]] if q.get("queue") is not None else None
                return SimpleNamespace(mutex=mutex, queue=queue)
            return materialize(q)
        seen, qargs = set(), []
        for q in g(inp, "queues", []):
            key = q.get("__id__", "anon") if isinstance(q, dict) else None
            if key is not None and key in seen:
                continue
            if key is not None:
                seen.add(key)
            qargs.append(matq(q))
        return MOD._queued_completion_ids(*qargs)
    if func == "_copy_may_be_live":
        return MOD._copy_may_be_live(g(inp, "delegation_id", ""), g(inp, "pid", 0),
                                     g(inp, "updated_at", 0.0), set(g(inp, "live_ids", [])),
                                     g(inp, "home", ""), set(g(inp, "offered", [])), g(inp, "now", 0.0))
    if func == "_note_reoffers":
        return MOD._note_reoffers(g(inp, "home", ""), g(inp, "lost", []), g(inp, "now", 0.0))
    if func == "maybe_sweep_orphaned_completions":
        if "now" in inp:
            return MOD.maybe_sweep_orphaned_completions(g(inp, "target_queue"), now=g(inp, "now"))
        return MOD.maybe_sweep_orphaned_completions(g(inp, "target_queue"))
    if func == "_event_delivery":
        return MOD._event_delivery(materialize(g(inp, "fn")), g(inp, "evt", {}), g(inp, "claim_id", ""))
    if func == "return_completion_offer":
        return MOD.return_completion_offer(g(inp, "evt", {}))
    raise KeyError("unknown func " + func)


def main():
    nd = 0
    bad = 0
    selftest = os.environ.get("AD2_SELFTEST") == "1"
    for line in sys.stdin:
        line = line.rstrip("\n")
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception:
            continue
        if selftest:
            try:
                out = drive(rec["func"], rec["in"])
                errs = []
                if isinstance(out["ret"], dict) and "__raise__" in out["ret"]:
                    errs.append(out["ret"]["__raise__"])
                if any("no script for" in str(e.get("ret", "")) for e in out["trace"]):
                    errs.append("NO-SCRIPT")
                print(rec["func"], "ret=", json.dumps(out["ret"])[:120], "trace_n=", len(out["trace"]), "errs=", errs)
            except BaseException as e:
                bad += 1
                print("DRIVE-ERR", rec.get("func"), type(e).__name__, str(e)[:120])
            nd += 1
            continue
        nd += 1
        want = drive(rec["func"], rec["in"])
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        diffs = []
        for k in ("ret", "trace", "st"):
            if norm(got.get(k)) != want[k]:
                diffs.append(k)
        if diffs:
            bad += 1
            print("MISMATCH", rec.get("func"), diffs)
            print("  want ret:", json.dumps(want["ret"], default=str)[:400])
            print("  got  ret:", json.dumps(norm(got.get("ret")), default=str)[:400])
            if "trace" in diffs:
                print("  want trace:", json.dumps(want["trace"], default=str)[:600])
                print("  got  trace:", json.dumps(norm(got.get("trace")), default=str)[:600])
            if "st" in diffs:
                print("  want st:", json.dumps(want["st"], default=str)[:600])
                print("  got  st:", json.dumps(norm(got.get("st")), default=str)[:600])
    if nd == 0:
        print("ORACLE async_delegation_flow2: 0 cases, 1 mismatches (no cases fed)")
        return 1
    print("ORACLE async_delegation_flow2: %d cases, %d mismatches" % (nd, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
