#!/usr/bin/env python3
"""Oracle for the UNCLAIMED-remainder FLOW port of agent/monitoring/events.py
(src/agent/monitoring/monitoring_events_flow3.c, me3_*).

A CRLF-aware PoP grep found only the stub port (monitoring_events.c) claiming
`to_dict` and a stale `_now_ns`; the faithful FLOW remainder — dataclass
construction with declared defaults in wire order, the time.time_ns
default_factory, and {"event": EVENT, **asdict(self)} — is ported in flow3.

The harness emits {func, in, out:{ret,trace,state}}. The oracle:
  1. drives the LIVE events.py dataclasses (AST-extracted, READ-ONLY quarry)
     under the SAME scripted io, producing `live`;
  2. computes a pure-Python shadow mirror of the C port (no quarry import),
     producing `shadow` (the SHADOW-DIFF check);
and diffs both against the harness `out`. Fail-closed: 0 cases => exit 1.

Contract: None->null; JSON objects keep insertion order (wire order is checked
explicitly at the top level); absent fields take declared defaults, absent
required fields raise TypeError; absent ts_ns fires the scripted time.time_ns
seam; seam {"__raise__":cls,"msg":m} raises with cls canonicalized through the
FULL py3.14 builtin exception hierarchy (audited at startup).

Summary: 'ORACLE monitoring_events_flow3: N cases, M mismatches'
          'SHADOW-DIFF monitoring_events_flow3: S'
"""
import __future__
import ast
import builtins
import dataclasses
import json
import os
import sys
import types
import typing
from pathlib import Path

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(Path.home() / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

NAME = "monitoring_events_flow3"
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "monitoring" / "events.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "monitoring" / "events.py")

# ── AST extraction of the LIVE dataclasses ─────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag
_CLASS_NODES = [n for n in _TREE.body if isinstance(n, ast.ClassDef)]
if not _CLASS_NODES:
    print(f"ORACLE {NAME}: FATAL no classes in {SRC}", file=sys.stderr)
    sys.exit(1)

# ── FULL builtin exception hierarchy (mirror of the C me3_exc_parent table) ─
CANON = {"IOError": "OSError", "EnvironmentError": "OSError",
         "WindowsError": "OSError"}
EXC_PARENT = {
    "BaseException": None,
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "GeneratorExit": "BaseException", "BaseExceptionGroup": "BaseException",
    "ExceptionGroup": "Exception", "Exception": "BaseException",
    "ArithmeticError": "Exception", "FloatingPointError": "ArithmeticError",
    "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "AssertionError": "Exception", "AttributeError": "Exception",
    "BufferError": "Exception", "EOFError": "Exception",
    "ImportError": "Exception", "ModuleNotFoundError": "ImportError",
    "LookupError": "Exception", "IndexError": "LookupError",
    "KeyError": "LookupError", "MemoryError": "Exception",
    "NameError": "Exception", "UnboundLocalError": "NameError",
    "OSError": "Exception", "BlockingIOError": "OSError",
    "ChildProcessError": "OSError", "ConnectionError": "OSError",
    "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "FileExistsError": "OSError", "FileNotFoundError": "OSError",
    "InterruptedError": "OSError", "IsADirectoryError": "OSError",
    "NotADirectoryError": "OSError", "PermissionError": "OSError",
    "ProcessLookupError": "OSError", "TimeoutError": "OSError",
    "ReferenceError": "Exception", "RuntimeError": "Exception",
    "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "PythonFinalizationError": "RuntimeError", "StopAsyncIteration": "Exception",
    "StopIteration": "Exception", "SyntaxError": "Exception",
    "IndentationError": "SyntaxError", "TabError": "IndentationError",
    "_IncompleteInputError": "SyntaxError", "SystemError": "Exception",
    "TypeError": "Exception", "ValueError": "Exception",
    "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeTranslateError": "UnicodeError",
    "Warning": "Exception", "UserWarning": "Warning",
    "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "SyntaxWarning": "Warning", "RuntimeWarning": "Warning",
    "FutureWarning": "Warning", "ImportWarning": "Warning",
    "UnicodeWarning": "Warning", "BytesWarning": "Warning",
    "ResourceWarning": "Warning", "EncodingWarning": "Warning",
}


def audit_exc_table():
    """Every builtin exception kind must be present; each parent must be in
    the class's real MRO (bases[0] is wrong for ExceptionGroup and aliases)."""
    bad = []
    for n in dir(builtins):
        o = getattr(builtins, n)
        if not (isinstance(o, type) and issubclass(o, BaseException)):
            continue
        canon = CANON.get(n, n)
        parent = EXC_PARENT.get(canon)
        if parent is None:
            if canon != "BaseException":
                bad.append((n, "missing table entry"))
            continue
        mro = [c.__name__ for c in o.__mro__]
        if parent not in mro:
            bad.append((n, "parent %s not in mro %s" % (parent, mro)))
    return bad


# ── seam plumbing ──────────────────────────────────────────────────────────
CUR_CTX = None


class PortRaise(Exception):
    def __init__(self, cls):
        super().__init__(cls)
        self.cls = cls


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.trace = []
        self.counters = {}


def norm(v):
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    return v  # None / str


class SeamFn:
    """LIVE default_factory seam: scripted via CUR_CTX.io['time.time_ns']."""
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        ctx = CUR_CTX
        name = self.name
        script = ctx.io.get(name)
        chosen = script
        if isinstance(script, dict) and isinstance(script.get("$seq"), list):
            seq = script["$seq"]
            idx = int(ctx.counters.get(name, 0))
            if idx >= len(seq):
                ctx.trace.append({"ev": name, "args": {},
                                  "ret": {"__raise__": "RuntimeError",
                                          "msg": "script exhausted"}})
                raise PortRaise("RuntimeError")
            ctx.counters[name] = idx + 1
            chosen = seq[idx]
        args = {}  # time.time_ns takes no arguments
        if isinstance(chosen, dict) and "__raise__" in chosen:
            cls = chosen["__raise__"]
            msg = str(chosen.get("msg", ""))
            canon = CANON.get(cls, cls)
            E = getattr(builtins, canon, Exception)
            try:
                exc = E(msg)
            except Exception:
                exc = Exception(msg)
            rname = type(exc).__name__
            ctx.trace.append({"ev": name, "args": args,
                              "ret": {"__raise__": rname, "msg": str(exc)}})
            raise PortRaise(rname)
        ctx.trace.append({"ev": name, "args": args, "ret": norm(chosen)})
        return chosen


def build_ns():
    # Register a REAL module in sys.modules: dataclasses._is_type looks up
    # sys.modules[cls.__module__].__dict__ for ClassVar/KW_ONLY detection.
    mod = types.ModuleType("monev_live_events")
    sys.modules["monev_live_events"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "monev_live_events",
        "__builtins__": builtins,
        "dataclass": dataclasses.dataclass,
        "field": dataclasses.field,
        "asdict": dataclasses.asdict,
        "make_dataclass": dataclasses.make_dataclass,
        "time": types.SimpleNamespace(time_ns=SeamFn("time.time_ns")),
        "ClassVar": typing.ClassVar, "Optional": typing.Optional,
        "Any": typing.Any, "Dict": typing.Dict,
    })
    for node in _CLASS_NODES:
        m = ast.Module(body=[node], type_ignores=[])
        code = compile(m, SRC, "exec", flags=_FLAGS, dont_inherit=True)
        exec(code, ns)
    return ns


# ── field specs (declaration = wire order; mirrors the C me3_* arrays) ─────
SPECS = {
    "GatewayHealthEvent": ("gateway_health", [
        ("name", "REQ", None), ("gateway_state", "NULL", None),
        ("old_state", "NULL", None), ("new_state", "NULL", None),
        ("exit_reason", "NULL", None), ("restart_requested", "NULL", None),
        ("active_agents", "INT", None), ("gateway_busy", "BOOL", None),
        ("gateway_drainable", "BOOL", None), ("platform_count", "INT", None),
        ("fatal_platform_count", "INT", None), ("profile", "NULL", None),
        ("install_id", "NULL", None), ("version", "NULL", None),
        ("supervision_mode", "NULL", None), ("pid", "NULL", None),
        ("ts_ns", "TS", None)]),
    "GatewayDiagnosticEvent": ("gateway_diagnostic", [
        ("name", "REQ", None), ("subsystem", "REQ", None),
        ("error_class", "STR", "unknown"), ("error_code", "NULL", None),
        ("platform", "NULL", None), ("old_state", "NULL", None),
        ("new_state", "NULL", None), ("profile", "NULL", None),
        ("version", "NULL", None), ("severity", "STR", "warning"),
        ("ts_ns", "TS", None), ("source_logger", "NULL", None)]),
    "CronExecutionEvent": ("cron_execution", [
        ("status", "REQ", None), ("job_key", "REQ", None),
        ("source", "STR", "unknown"), ("duration_ms", "NULL", None),
        ("delivery_outcome", "NULL", None), ("error_class", "NULL", None),
        ("ts_ns", "TS", None)]),
}


# ── LIVE driver (real events.py dataclasses) ───────────────────────────────
def drive_live(rec, ns):
    p = rec["in"]
    f = rec["func"]
    if f == "to_dict":
        fields = p.get("fields") or {}
        names = list(fields.keys())
        DC = dataclasses.make_dataclass(
            "_DynEvent",
            [(k, object, dataclasses.field(default=None)) for k in names],
            bases=(ns["_MonitoringEvent"],))
        DC.EVENT = p.get("event", "")
        obj = DC(**{k: fields[k] for k in names})
        return obj.to_dict()
    cls = ns[f]
    fs = SPECS[f][1]
    kw = {k: p[k] for k, _, _ in fs if k in p}
    obj = cls(**kw)  # missing required -> real TypeError
    return obj.to_dict()


# ── shadow mirror (pure Python model of the C port) ────────────────────────
def build_shadow_fields(ctx, p, fs):
    obj = {}
    for name, kind, dv in fs:
        if name in p:
            obj[name] = norm(p[name])
            continue
        if kind == "REQ":
            raise PortRaise("TypeError")
        if kind == "TS":
            obj[name] = SeamFn("time.time_ns")()  # uses CUR_CTX
        elif kind == "STR":
            obj[name] = dv
        elif kind == "INT":
            obj[name] = 0
        elif kind == "BOOL":
            obj[name] = False
        else:
            obj[name] = None
    return obj


def shadow(rec):
    global CUR_CTX
    p = rec["in"]
    ctx = Ctx(p.get("io") or {})
    CUR_CTX = ctx
    try:
        f = rec["func"]
        if f == "to_dict":
            out = {"event": p.get("event", "")}
            for k, v in (p.get("fields") or {}).items():
                out[k] = norm(v)
        else:
            tag, fs = SPECS[f]
            out = {"event": tag}
            out.update(build_shadow_fields(ctx, p, fs))
        return {"ret": out, "trace": ctx.trace, "state": {}}
    except PortRaise as e:
        return {"ret": {"__raise__": e.cls}, "trace": ctx.trace, "state": {}}
    except BaseException as e:
        return {"ret": {"__raise__": type(e).__name__},
                "trace": ctx.trace, "state": {}}


# ── wire-order check (dict equality alone ignores order) ───────────────────
def ret_keys(d):
    if isinstance(d, dict) and "__raise__" not in d:
        return list(d.keys())
    return None


def order_matches(got, ref):
    return ret_keys(got.get("ret")) == ret_keys(ref.get("ret"))


def diff_line(got, ref, tag):
    return ("    %s=%s" % (tag, json.dumps(got, ensure_ascii=False)[:1400]))


def main():
    global CUR_CTX
    ns = build_ns()
    bad = audit_exc_table()
    if bad:
        print("ORACLE %s: FATAL exception table audit: %r" % (NAME, bad),
              file=sys.stderr)
        sys.exit(1)
    n = mism = shadow_diffs = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        # LIVE reference (real events.py driven under the same scripts)
        CUR_CTX = Ctx((rec.get("in") or {}).get("io") or {})
        try:
            lr = drive_live(rec, ns)
            live = {"ret": norm(lr), "trace": CUR_CTX.trace, "state": {}}
        except BaseException as e:
            live = {"ret": {"__raise__": (e.cls if isinstance(e, PortRaise)
                                          else type(e).__name__)},
                    "trace": CUR_CTX.trace, "state": {}}
        # Shadow mirror (model of the C port)
        sh = shadow(rec)
        sd = (got != sh) or not order_matches(got, sh)
        md = (got != live) or not order_matches(got, live)
        if sd:
            shadow_diffs += 1
        if sd or md:
            mism += 1
            if shown < 12:
                shown += 1
                print("MISMATCH %s case %d (shadow_diff=%s live_diff=%s):"
                      % (rec.get("func"), n, sd, md))
                print(diff_line(got, None, "C  "))
                print(diff_line(sh, None, "SH "))
                print(diff_line(live, None, "PY "))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    print("SHADOW-DIFF %s: %d" % (NAME, shadow_diffs))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
