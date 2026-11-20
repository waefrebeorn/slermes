#!/usr/bin/env python3
"""Oracle for src/cli/relay_metrics_flow3.c — drives the LIVE
hermes_cli/observability/relay_shared_metrics.py FIFTH-wave FLOW defs (9
functions) under IDENTICAL scripted universal seams. (The task brief named
"relay_metrics.py"; no such quarry file exists — the relay-metrics module IS
relay_shared_metrics.py, and every PoP in the port names it.)

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec) and the state window. Live defs are AST-extracted from the
quarry (the five @staticmethods are extracted as plain functions; _safe is
bound to a synthetic class exposing _guarded; _export is driven on a
SimpleNamespace self carrying callable store/send doubles). The logger seam is
a double. Claimed siblings / foreign I/O ride the recording Script.

A literal SHADOW MIRROR of the C algorithm is driven beside the live defs; the
two must agree on every fixture (shadow-diff 0) before the harness diff counts.

Fail-closed: an absent / exhausted script RAISES a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. Per-case BaseException catch -> __raise__ spec via identity match
against Script.handed (scripted raises round-trip BARE). make_exc covers the
FULL builtin hierarchy (Unicode trio + ExceptionGroup special constructors).

Summary: 'ORACLE relay_metrics_flow3: N cases, M mismatches'.
"""
import ast
import builtins
import copy
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "observability" / "relay_shared_metrics.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "observability" / "relay_shared_metrics.py")
NAME = "relay_metrics_flow3"

PORTED = [
    "_text", "_retry_ordinal", "_session_pair", "_identities_compatible",
    "_event_matches_task_turn", "_existing_model_call_key", "_guarded",
    "_safe", "_export",
]
MOD_FUNCS = {"_text", "_retry_ordinal", "_session_pair", "_identities_compatible"}
METHODS = {"_event_matches_task_turn", "_existing_model_call_key",
           "_guarded", "_safe", "_export"}

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)

def _compile(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   dont_inherit=True)

CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in MOD_FUNCS:
        _s = copy.copy(_node)
        _s.decorator_list = []
        CODE[_node.name] = _compile(_s)
    elif isinstance(_node, ast.ClassDef) and _node.name == "_Runtime":
        for _sub in _node.body:
            if isinstance(_sub, ast.FunctionDef) and _sub.name in METHODS:
                _s = copy.copy(_sub)
                _s.decorator_list = []
                CODE[_sub.name] = _compile(_s)
_missing = set(PORTED) - set(CODE)
if _missing:
    print("ORACLE %s: FATAL missing live defs: %s" % (NAME, sorted(_missing)),
          file=sys.stderr)
    sys.exit(1)
NS = {"__builtins__": builtins}
for _nm in PORTED:
    exec(CODE[_nm], NS)

# synthetic _Runtime exposing _guarded as a staticmethod (for _safe's cls._guarded)
FAKE_CLS = type("_Runtime", (), {"_guarded": staticmethod(NS["_guarded"])})

# ── projection helpers ──────────────────────────────────────────────────────
MISSING = object()


def canon(v):
    """Normalize a scripted value to the C trace representation."""
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, list):
        return [canon(x) for x in v]
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    return v


def norm_args(v):
    v = canon(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm_value(x) for x in v), key=str)
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def make_exc(spec):
    """Build a real exception whose identity round-trips via Script.handed."""
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", spec.get("msg", ""))
    cls = getattr(builtins, kind, None)
    if cls is None:
        cls = RuntimeError
    try:
        if kind in ("UnicodeDecodeError", "UnicodeEncodeError"):
            exc = cls("utf-8", b"x", 0, 1, str(msg))
        elif kind == "UnicodeTranslateError":
            exc = cls("x", 0, 1, str(msg))
        elif kind in ("ExceptionGroup", "BaseExceptionGroup"):
            inner = ([KeyboardInterrupt(str(msg))] if kind == "BaseExceptionGroup"
                     else [RuntimeError(str(msg))])
            exc = cls(str(msg), inner)
        else:
            exc = cls(str(msg))
    except Exception:
        exc = RuntimeError(str(msg))
    exc.__script_spec__ = spec
    return exc


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.handed = []

    def call(self, name, payload):
        payload = payload or {}
        entry = MISSING
        if name in self.io:
            raw = self.io[name]
            if isinstance(raw, list):  # bare array = per-call QUEUE
                idx = self.cursors.get(name, 0)
                if idx < len(raw):
                    entry = raw[idx]
                    self.cursors[name] = idx + 1
                else:
                    entry = MISSING
            else:
                entry = raw
        if entry is MISSING:
            spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
            self.trace.append({"ev": name, "args": norm_args(payload),
                               "ret": canon(spec)})
            exc = make_exc(spec)
            self.handed.append((exc, spec))
            raise exc
        actual = entry
        if isinstance(entry, dict) and entry.get("__script__") is True:
            actual = entry.get("ret")
        self.trace.append({"ev": name, "args": norm_args(payload),
                           "ret": canon(actual)})
        if is_raise(actual):
            exc = make_exc(actual)
            self.handed.append((exc, actual))
            raise exc
        return actual  # scripted null -> None (matches C NULL pointer)


class Ctx:
    def __init__(self, inp):
        self.script = Script(inp.get("io"))
        self.trace = self.script.trace


def _callable(script, name):
    def _cb(*a, **kw):
        return script.call(name, {"args": list(a), "kwargs": dict(kw)})
    _cb.__name__ = name
    return _cb


def finish(ctx, fn):
    try:
        return {"ret": norm_value(fn()), "trace": ctx.trace,
                "st": norm_value(ctx.inp.get("state") or {})}
    except BaseException as e:  # noqa: BLE001
        spec = None
        for obj, s in ctx.script.handed:
            if obj is e:
                spec = s
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace,
                "st": norm_value(ctx.inp.get("state") or {})}


# ── LIVE drive (authoritative reference) ───────────────────────────────────
def drive_live(ctx, func, inp):
    S = ctx.script
    ev = inp.get("event") or {}

    def _logger_warning(msg, exc_info=False):
        return S.call("logger.warning",
                      {"args": [msg], "kwargs": {"exc_info": exc_info}})
    NS["logger"] = SimpleNamespace(warning=_logger_warning)

    if func == "_text":
        return NS["_text"](ev, inp.get("key", ""))
    if func == "_retry_ordinal":
        return NS["_retry_ordinal"](ev)
    if func == "_session_pair":
        return NS["_session_pair"](ev, inp.get("key", ""))
    if func == "_identities_compatible":
        return NS["_identities_compatible"](tuple(inp.get("candidate") or []),
                                            tuple(inp.get("observed") or []))
    if func == "_event_matches_task_turn":
        t = inp.get("task") or {}
        task = SimpleNamespace(
            retired_turn_ids=set(t.get("retired_turn_ids") or []),
            turn_ids=set(t.get("turn_ids") or []))
        return NS["_event_matches_task_turn"](task, ev)
    if func == "_existing_model_call_key":
        sess = SimpleNamespace(model_calls={tuple(m) for m in
                             ((inp.get("session") or {}).get("model_calls") or [])})
        return NS["_existing_model_call_key"](sess, ev)
    if func in ("_guarded", "_safe"):
        marker = inp.get("callback") or {}
        name = marker.get("__callable__", "cb") if isinstance(marker, dict) else "cb"
        cb = _callable(S, name)
        args = inp.get("args") or []
        kwargs = inp.get("kwargs") or {}
        if func == "_guarded":
            return NS["_guarded"](inp.get("message", ""), cb, *args, **kwargs)
        return NS["_safe"](FAKE_CLS, cb, *args, **kwargs)
    if func == "_export":
        store_cb = _callable(S, "store.create_and_export_package_if_due")
        send_cb = _callable(S, "_send_exported_packages")
        slf = SimpleNamespace()
        slf._safe = lambda cb, *a, **kw: NS["_safe"](FAKE_CLS, cb, *a, **kw)
        slf.subscriber = SimpleNamespace(
            store=SimpleNamespace(create_and_export_package_if_due=store_cb))
        slf._send_exported_packages = send_cb
        return NS["_export"](slf)
    raise RuntimeError("unknown func " + str(func))


# ── SHADOW MIRROR (literal transcription of the C algorithm) ───────────────
def _shadow_text(v):
    if v is None or v is False or v == 0 or v == "" or v == [] or v == {}:
        return ""
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, float) and v.is_integer():
        return str(int(v))
    return str(v)


def drive_shadow(ctx, func, inp):
    """Compute the expected ret by re-executing the C decision sequence."""
    S = ctx.script
    ev = inp.get("event") or {}

    def call(cb_name, args=None, kwargs=None):
        return S.call(cb_name, {"args": args or [], "kwargs": kwargs or {}})

    def guarded(message, cb, args, kwargs):
        try:
            ret = call(cb, args, kwargs)
            if is_raise(ret):
                # C returns the raise spec as a VALUE; emulate the raise here
                exc = make_exc(ret)
                S.handed.append((exc, ret))
                raise exc
            return ret
        except BaseException as e:
            spec = next((s for o, s in S.handed if o is e), None)
            if spec is not None and _is_exception_kind(spec.get("__raise__", "")):
                ret = call("logger.warning", [message], {"exc_info": True})
                if is_raise(ret):
                    exc = make_exc(ret); S.handed.append((exc, ret)); raise exc
                return None
            raise

    if func == "_text":
        return _shadow_text(ev.get(inp.get("key", "")))
    if func == "_retry_ordinal":
        v = ev.get("retry_count")
        return v if (isinstance(v, int) and not isinstance(v, bool) and v > 0) else 0
    if func == "_session_pair":
        sid = _shadow_text(ev.get("session_id"))
        val = _shadow_text(ev.get(inp.get("key", "")))
        return [sid, val] if sid and val else None
    if func == "_identities_compatible":
        c, o = inp.get("candidate") or [], inp.get("observed") or []
        if not o[2] or c[2] != o[2]:
            return False
        return all((not l or not r or l == r) for l, r in zip(c[:2], o[:2]))
    if func == "_event_matches_task_turn":
        t = inp.get("task") or {}
        tid = _shadow_text(ev.get("turn_id"))
        if not tid:
            return True
        if tid in (t.get("retired_turn_ids") or []):
            return False
        turns = t.get("turn_ids") or []
        if turns and tid not in turns:
            return False
        return True
    if func == "_existing_model_call_key":
        rid = _shadow_text(ev.get("api_request_id"))
        if not rid:
            return None
        tid = _shadow_text(ev.get("task_id"))
        mcalls = (inp.get("session") or {}).get("model_calls") or []
        present = any(m[0] == tid and m[1] == rid for m in mcalls)
        if present or tid:
            return [tid, rid] if present else None
        cands = [m for m in mcalls if m[1] == rid]
        return list(cands[0]) if len(cands) == 1 else None
    if func == "_guarded":
        marker = inp.get("callback") or {}
        name = marker.get("__callable__", "cb") if isinstance(marker, dict) else "cb"
        return guarded(inp.get("message", ""), name, inp.get("args") or [],
                       inp.get("kwargs") or {})
    if func == "_safe":
        marker = inp.get("callback") or {}
        name = marker.get("__callable__", "cb") if isinstance(marker, dict) else "cb"
        return guarded("Hermes shared metrics operation failed", name,
                       inp.get("args") or [], inp.get("kwargs") or {})
    if func == "_export":
        exported = guarded("Hermes shared metrics operation failed",
                           "store.create_and_export_package_if_due", [], {})
        if is_raise(exported):
            return exported
        if exported is not None:
            guarded("Hermes shared metrics operation failed",
                    "_send_exported_packages", [], {})
        return None
    raise RuntimeError("unknown func " + str(func))


# full builtin hierarchy, mirroring RM3_EXC_TABLE (Exception-family check)
_EXC_PARENT = {
    "BaseException": None, "BaseExceptionGroup": "BaseException",
    "GeneratorExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "SystemExit": "BaseException", "CancelledError": "BaseException",
    "Exception": "BaseException", "ArithmeticError": "Exception",
    "AssertionError": "Exception", "AttributeError": "Exception",
    "BufferError": "Exception", "EOFError": "Exception", "ImportError": "Exception",
    "LookupError": "Exception", "MemoryError": "Exception", "NameError": "Exception",
    "OSError": "Exception", "ReferenceError": "Exception", "RuntimeError": "Exception",
    "StopAsyncIteration": "Exception", "StopIteration": "Exception",
    "SyntaxError": "Exception", "SystemError": "Exception", "TypeError": "Exception",
    "ValueError": "Exception", "Warning": "Exception", "ExceptionGroup": "Exception",
    "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "ConnectionError": "OSError", "FileExistsError": "OSError",
    "FileNotFoundError": "OSError", "InterruptedError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError",
    "TimeoutError": "OSError", "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError", "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "IndexError": "LookupError",
    "KeyError": "LookupError", "ModuleNotFoundError": "ImportError",
    "UnboundLocalError": "NameError", "NotImplementedError": "RuntimeError",
    "RecursionError": "RuntimeError", "IndentationError": "SyntaxError",
    "TabError": "IndentationError", "UnicodeError": "ValueError",
    "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "DeprecationWarning": "Warning",
    "PendingDeprecationWarning": "Warning", "RuntimeWarning": "Warning",
    "SyntaxWarning": "Warning", "UserWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "UnicodeWarning": "Warning", "BytesWarning": "Warning",
    "ResourceWarning": "Warning", "EncodingWarning": "Warning",
    "EnvironmentError": "OSError", "IOError": "OSError", "WindowsError": "OSError",
}


def _is_exception_kind(kind):
    cur = kind
    for _ in range(64):
        if cur == "Exception":
            return True
        if cur == "BaseException" or cur is None:
            return False
        cur = _EXC_PARENT.get(cur, "BASE?")
        if cur == "BASE?":
            return False
    return False


def run_case(func, inp, mode="live"):
    ctx = Ctx(inp)
    ctx.inp = inp
    fn = drive_live if mode == "live" else drive_shadow
    return finish(ctx, lambda: fn(ctx, func, inp))


def main():
    n = mism = shadow_diff = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        live = run_case(func, inp, "live")
        shadow = run_case(func, inp, "shadow")
        if live != shadow:
            shadow_diff += 1
            if shadow_diff <= 8:
                print("SHADOW-DIFF [%s] case %d" % (func, n))
                print("  LIVE  : %s" % json.dumps(live, ensure_ascii=False)[:600])
                print("  SHADOW: %s" % json.dumps(shadow, ensure_ascii=False)[:600])
        want = norm_value(live)
        got = norm_value({"ret": rec.get("ret"), "trace": rec.get("trace"),
                          "st": rec.get("st")})
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:700]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:700]))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    if shadow_diff:
        print("SHADOW-DIFF %d case(s) (live vs mirror)" % shadow_diff)
    if n == 0:
        return 1
    return 1 if (mism or shadow_diff) else 0


def selftest(fixtures):
    bad = 0
    seen = set()
    for i, rec in enumerate(fixtures):
        func = rec.get("func")
        seen.add(func)
        if func not in PORTED:
            bad += 1
            print("SELFTEST case %d: func %s not in PORTED" % (i, func))
            continue
        for mode in ("live", "shadow"):
            out = run_case(func, rec.get("in") or {}, mode)
            spec = out["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print("SELFTEST %d %s %s: NameError %s" % (i, func, mode, spec.get("message")))
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("message", "")
                if any(("'" + nm + "'") in msg for nm in PORTED):
                    bad += 1
                    print("SELFTEST %d %s %s: suspicious AttributeError %s" % (i, func, mode, msg))
    missing = set(PORTED) - seen
    if missing:
        bad += 1
        print("SELFTEST: funcs with no fixture: %s" % sorted(missing))
    print("SELFTEST %s: %d cases, %d suspicious" % (NAME, len(fixtures), bad))
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "relay_metrics_flow3.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())
