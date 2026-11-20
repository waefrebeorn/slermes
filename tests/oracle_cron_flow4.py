#!/usr/bin/env python3
"""Oracle for the hermes_cli/cron.py REMAINDER FLOW port (cron_flow4, fourth wave).

AST-extracts seven LIVE unclaimed defs (no heavy hermes_cli import), seeds the
module constants, replaces every I/O dependency with a recording stub driven by
the SAME per-name scripts the C harness feeds its universal seam (in.io), and
compares the ordered seam-call trace plus the return value.

Seam shapes (both sides): a bare JSON array is a SEQUENCE (one result per call,
last element repeats after exhaustion); any other value is reused; a scripted
null is None. FAIL-CLOSED: a missing or empty script raises
RuntimeError("no script for <name>"), recorded as the event ret. The C port's
full builtin exception table (cr4_exc_edges) is mirrored by M_PARENT: every
Exception subclass is swallowed by `except Exception`, while the BaseException
trio / BaseExceptionGroup / CancelledError propagate. `except OSError` matches
the whole subtree and re-raises verbatim when no store report exists.

Modeling (twin of the port header): color(text, Colors.X) -> text (no ANSI on
either side); print(...) fires "print" with {"text": ...}; the in-function
`from cron.jobs import TICKER_INTERVAL_SECONDS` projects to the fixture's
ticker_interval (default 60); `_current_cron_store().cron_dir` is one seam;
CLAIMED siblings (_active_cron_provider_name, _cron_doctor_issues_for_job,
_print_banner) are seamed, never re-ported.

Usage:
  python tests/oracle_cron_flow4.py                 # gate: harness JSONL on stdin
  python tests/oracle_cron_flow4.py --shadow        # literal-C mirror vs live, fixtures
"""
import ast
import builtins
import contextlib
import json
import os
import re
import sys
import types
import __future__
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Iterable, List, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "cron.py")

CUR = {"func": None, "in": {}, "trace": [], "idx": {}}

# ---- full builtin exception hierarchy (real classes, so except-site identity
#      and BaseException propagation stay faithful) ----
_BUILTIN_EXC = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "CancelledError": __import__("asyncio").CancelledError,
    "BaseExceptionGroup": BaseExceptionGroup, "Exception": Exception,
    "ExceptionGroup": ExceptionGroup, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "ArithmeticError": ArithmeticError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError,
    "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError, "MemoryError": MemoryError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
    "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "PythonFinalizationError": getattr(builtins, "PythonFinalizationError", RuntimeError),
    "_IncompleteInputError": getattr(builtins, "_IncompleteInputError", SyntaxError),
    "EnvironmentError": OSError, "IOError": OSError, "WindowsError": OSError,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    cls = _BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    # Unicode*Error builtins reject a 1-arg ctor on py3.14; bypass __init__.
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    if kind == "ExceptionGroup":
        return cls(msg, [ValueError(msg)])
    if kind == "BaseExceptionGroup":
        return cls(msg, [SystemExit(msg)])
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    io = CUR["in"].get("io") or {}
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, dict) and sc.get("__script__") is True:
            r = sc.get("ret")  # list-valued RESULT (bare array = per-call sequence)
        elif isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def m_seam(name, args):
    """Literal-C recorder: the C dispatcher RETURNS raise specs, it never
    raises. The mirror uses this so its explicit cr4_* swallow checks are
    exercised instead of Python's exception machinery."""
    args = {k: v for k, v in args.items() if v is not None}
    io = CUR["in"].get("io") or {}
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, dict) and sc.get("__script__") is True:
            r = sc.get("ret")
        elif isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return r


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


def denorm(x):
    if isinstance(x, BaseException):
        return raise_spec(x)
    if isinstance(x, (types.SimpleNamespace, AttrDouble)):
        d = vars(x) if isinstance(x, types.SimpleNamespace) else object.__getattribute__(x, "_d")
        return {k: denorm(v) for k, v in d.items()}
    if isinstance(x, PurePosixPath):
        return x.as_posix()
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(denorm(v) for v in x)
    return x


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def run_flow(fn):
    try:
        return {"ret": denorm(fn()), "state": None}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": None}


# ---- live-def extraction ----
WANTED = (
    "_builtin_gateway_liveness", "cron_incidents", "_ticker_age_is_fresh",
    "_store_unwritable_report", "_probe_then_list_jobs",
    "_print_store_unwritable", "cron_doctor",
)


class AttrDouble:
    """args object; getattr(args, name, default) must see a real miss for absent keys."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError(name)


class Colors:
    RED = "red"; YELLOW = "yellow"; GREEN = "green"
    DIM = "dim"; CYAN = "cyan"; BLUE = "blue"


def print_seam(*a, **k):
    return seam("print", {"text": " ".join(str(x) for x in a)})


def extract_live():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "cr4_extracted_cron_py", "__file__": SRC,
        "re": re, "json": json, "contextlib": contextlib,
        "Path": PurePosixPath, "project_root_str": lambda: "/proj",
        "Colors": Colors, "color": lambda text, *a, **k: text,
        "print": print_seam,
        "Any": Any, "Dict": Dict, "Iterable": Iterable, "List": List,
        "Optional": Optional,
    }
    tree = ast.parse(src)

    def seed_assigns():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not (isinstance(node.target, ast.Name) and node.value is not None):
                    continue
            else:
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass

    seed_assigns()
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for name in WANTED:
        if name not in ns:
            raise KeyError("live def never extracted: " + name)
    # Assign seeding can shadow injected doubles; restore after extraction.
    ns["print"] = print_seam
    ns["color"] = lambda text, *a, **k: text
    ns["Colors"] = Colors
    return ns


NS = extract_live()


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    if "." in name:
        parent, leaf = name.rsplit(".", 1)
        pm = sys.modules.setdefault(parent, types.ModuleType(parent))
        pm.__path__ = []
        setattr(pm, leaf, m)
    return m


def mat_store(r):
    return types.SimpleNamespace(cron_dir=r) if r is not None else None


def install_case():
    CUR["idx"] = {}
    CUR["trace"] = []
    inp = CUR["in"]
    interval = inp.get("ticker_interval", 60)
    NS["_active_cron_provider_name"] = lambda: seam("active_provider_name", {})
    NS["_cron_doctor_issues_for_job"] = lambda job: seam("doctor_issues", {"job": denorm(job)})
    NS["_print_banner"] = lambda title: seam("print_banner", {"title": title})

    mod("cron")
    mod("cron.jobs",
        TICKER_INTERVAL_SECONDS=interval,
        _current_cron_store=lambda: mat_store(seam("current_cron_store", {})),
        get_ticker_heartbeat_age=lambda: seam("ticker_heartbeat_age", {}),
        ticker_heartbeat_writer_alive=lambda: seam("ticker_writer_alive", {}),
        list_jobs=lambda include_disabled: seam("list_jobs", {"include_disabled": include_disabled}))
    mod("cron.store_health",
        probe_report=lambda d: seam("probe_report", {"dir": denorm(d)}))
    mod("cron.incidents",
        ack_incident=lambda iid: seam("ack_incident", {"incident_id": iid}),
        list_incidents=lambda state: seam("list_incidents", {"state": state}))
    mod("gateway")
    mod("gateway.status",
        is_gateway_runtime_lock_active=lambda: seam("runtime_lock_active", {}))
    mod("hermes_cli")
    mod("hermes_cli.gateway",
        find_gateway_pids=lambda: seam("find_gateway_pids", {}),
        named_profile_served_by_running_multiplexer=lambda: seam("multiplexer_serves", {}))
    NS["print"] = print_seam
    NS["color"] = lambda text, *a, **k: text
    NS["Colors"] = Colors


F2L = {
    "builtin_gateway_liveness": "_builtin_gateway_liveness",
    "ticker_age_is_fresh": "_ticker_age_is_fresh",
    "store_unwritable_report": "_store_unwritable_report",
    "print_store_unwritable": "_print_store_unwritable",
    "probe_then_list_jobs": "_probe_then_list_jobs",
    "incidents": "cron_incidents",
    "doctor": "cron_doctor",
}


def drive(func, inp):
    fn = NS[F2L[func]]
    if func == "builtin_gateway_liveness":
        return run_flow(lambda: fn())
    if func == "ticker_age_is_fresh":
        return run_flow(lambda: fn(inp.get("age")))
    if func == "store_unwritable_report":
        return run_flow(lambda: fn())
    if func == "print_store_unwritable":
        return run_flow(lambda: fn(inp.get("report"), brief=inp.get("brief", False)))
    if func == "probe_then_list_jobs":
        return run_flow(lambda: fn(include_disabled=inp.get("include_disabled", True),
                                   brief=inp.get("brief", False)))
    if func == "incidents":
        return run_flow(lambda: fn(AttrDouble(inp.get("args") or {})))
    if func == "doctor":
        return run_flow(lambda: fn())
    raise ValueError("unknown func: " + func)


# ---- literal transcription of src/cli/cron_flow4.c (for shadow-diff) ----
# The mirror must read the C byte-for-byte, including its exception parent
# table and accessor shapes; it must NOT "correct" the C logic.
M_PARENT = {
    "BaseException": None, "SystemExit": "BaseException",
    "KeyboardInterrupt": "BaseException", "GeneratorExit": "BaseException",
    "CancelledError": "BaseException", "BaseExceptionGroup": "BaseException",
    "Exception": "BaseException", "ExceptionGroup": "Exception",
    "StopIteration": "Exception", "StopAsyncIteration": "Exception",
    "ArithmeticError": "Exception", "FloatingPointError": "ArithmeticError",
    "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "AssertionError": "Exception", "AttributeError": "Exception",
    "BufferError": "Exception", "EOFError": "Exception", "ImportError": "Exception",
    "ModuleNotFoundError": "ImportError", "LookupError": "Exception",
    "IndexError": "LookupError", "KeyError": "LookupError", "MemoryError": "Exception",
    "NameError": "Exception", "UnboundLocalError": "NameError", "OSError": "Exception",
    "EnvironmentError": "OSError", "IOError": "OSError", "WindowsError": "OSError",
    "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "ConnectionError": "OSError", "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "FileExistsError": "OSError",
    "FileNotFoundError": "OSError", "InterruptedError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError",
    "TimeoutError": "OSError", "ReferenceError": "Exception",
    "RuntimeError": "Exception", "NotImplementedError": "RuntimeError",
    "RecursionError": "RuntimeError", "PythonFinalizationError": "RuntimeError",
    "SyntaxError": "Exception", "IndentationError": "SyntaxError",
    "TabError": "IndentationError", "_IncompleteInputError": "SyntaxError",
    "SystemError": "Exception", "TypeError": "Exception", "ValueError": "Exception",
    "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeTranslateError": "UnicodeError",
    "Warning": "Exception", "DeprecationWarning": "Warning",
    "PendingDeprecationWarning": "Warning", "RuntimeWarning": "Warning",
    "SyntaxWarning": "Warning", "UserWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "UnicodeWarning": "Warning", "BytesWarning": "Warning",
    "EncodingWarning": "Warning", "ResourceWarning": "Warning",
}


def m_is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def m_kind(v):
    return v.get("__raise__") if m_is_raise(v) else None


def m_caught(kind, base):
    if kind is None:
        return False
    if base == "Exception" and kind not in M_PARENT and kind != "BaseException":
        return True
    cur = kind
    for _ in range(48):
        if cur is None:
            break
        if cur == base:
            return True
        cur = M_PARENT.get(cur)
    return False


def m_truthy(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return bool(v)
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def m_gets(o, k, d):
    v = o.get(k) if isinstance(o, dict) else None
    return v if isinstance(v, str) else d


def m_raise_only(v):
    return {"__raise__": v.get("__raise__"), "msg": v.get("msg", "")}


def m_ticker_age_is_fresh(age, interval):
    if age is None:
        return False
    if not isinstance(age, (int, float)) or isinstance(age, bool):
        age = 0
    return bool(age <= interval * 3.0 + 20.0)


def m_builtin_gateway_liveness(interval):
    r = m_seam("active_provider_name", {})
    if m_is_raise(r):
        return None if m_caught(m_kind(r), "Exception") else r
    if (r if isinstance(r, str) else "") != "builtin":
        return True
    lk = m_seam("runtime_lock_active", {})
    if m_is_raise(lk):
        if not m_caught(m_kind(lk), "Exception"):
            return lk
    elif m_truthy(lk):
        return True
    pids = m_seam("find_gateway_pids", {})
    if m_is_raise(pids):
        return None if m_caught(m_kind(pids), "Exception") else pids
    if isinstance(pids, list) and len(pids) > 0:
        return True
    age = m_seam("ticker_heartbeat_age", {})
    if m_is_raise(age):
        return None if m_caught(m_kind(age), "Exception") else age
    if not m_ticker_age_is_fresh(age, interval):
        return False
    mx = m_seam("multiplexer_serves", {})
    if m_is_raise(mx):
        return None if m_caught(m_kind(mx), "Exception") else mx
    served = m_truthy(mx)
    alive = False
    if not served:  # Python `A or B` short-circuits; C port must match
        wr = m_seam("ticker_writer_alive", {})
        if m_is_raise(wr):
            return None if m_caught(m_kind(wr), "Exception") else wr
        alive = m_truthy(wr)
    return bool(served or alive)


def m_store_unwritable_report():
    store = m_seam("current_cron_store", {})
    if m_is_raise(store):
        return store
    return m_seam("probe_report", {"dir": store if isinstance(store, str) else ""})


def m_print_store_unwritable(rep, brief):
    store = m_gets(rep, "store", ""); err = m_gets(rep, "error", "")
    fix = m_gets(rep, "fix", ""); since = m_gets(rep, "since", "")
    if brief:
        r = m_seam("print", {"text":
            "\u26a0 Cron store %s is NOT writable (%s) \u2014 scheduled jobs "
            "are being skipped; %s. See `hermes cron status`." % (store, err, fix)})
        return r if m_is_raise(r) else None
    r = m_seam("print", {"text":
        "\u26a0 Cron store is NOT writable \u2014 scheduled jobs are being skipped"})
    if m_is_raise(r):
        return r
    r = m_seam("print", {"text": "  %s: %s (last successful write %s)" % (store, err, since)})
    if m_is_raise(r):
        return r
    r = m_seam("print", {"text": "  Fix: %s. Each due job then fires once on the next tick." % fix})
    return r if m_is_raise(r) else None


def m_probe_then_list_jobs(include_disabled, brief):
    report = m_store_unwritable_report()
    if m_is_raise(report):
        return report
    if report is not None:
        pr = m_print_store_unwritable(report, brief)
        if m_is_raise(pr):
            return pr
    jobs = m_seam("list_jobs", {"include_disabled": include_disabled})
    if m_is_raise(jobs):
        if m_caught(m_kind(jobs), "OSError"):
            saved = m_raise_only(jobs)
            if report is None:
                return saved
            return [report, None]
        return jobs
    return [report, jobs]


def m_ws_collapse(s):
    return re.sub(r"\s+", " ", s or "").strip()


def m_clip_error(t):
    if len(t) <= 160:
        return t
    return t[:157].rstrip() + "..."


def m_incidents(args, _inp):
    action = m_gets(args, "incident_action", "list")
    if action == "ack":
        idv = args.get("incident_id")
        iid = idv if isinstance(idv, str) else ""
        if not iid:
            r = m_seam("print", {"text":
                "\u2717 Incident ID required: hermes cron incidents ack <incident_id>"})
            return r if m_is_raise(r) else 1
        r = m_seam("ack_incident", {"incident_id": idv})
        if m_is_raise(r):
            return r
        ok = m_truthy(r)
        msg = ("\u2713 Incident %s acknowledged (closed)." if ok else
               "Incident %s not found or already closed.") % iid
        r = m_seam("print", {"text": msg})
        return r if m_is_raise(r) else 0
    state = args.get("state")
    incs = m_seam("list_incidents", {"state": state})
    if m_is_raise(incs):
        return incs
    n = len(incs) if isinstance(incs, list) else 0
    if n == 0:
        r = m_seam("print", {"text": "No cron failure incidents recorded."})
        if m_is_raise(r):
            return r
        if isinstance(state, str) and state:
            r = m_seam("print", {"text": "  (filtered by state '%s')" % state})
            if m_is_raise(r):
                return r
        return 0
    r = m_seam("print_banner", {"title": "Cron Failure Incidents"})
    if m_is_raise(r):
        return r
    for i in range(n):
        inc = incs[i]
        st = m_gets(inc, "state", ""); iid = m_gets(inc, "id", "")
        err = m_clip_error(m_ws_collapse(m_gets(inc, "error", "")))
        rows = [("Job", m_gets(inc, "job_id", "?")),
                ("Type", m_gets(inc, "failure_type", "unknown")),
                ("First seen", m_gets(inc, "first_seen_at", "?")),
                ("Last seen", m_gets(inc, "last_seen_at", "?")),
                ("Error", err), ("Output", m_gets(inc, "output_file", None))]
        r = m_seam("print", {"text": "  %s  %s" % (iid, st)})
        if m_is_raise(r):
            return r
        for label, val in rows:
            if label == "Output" and not val:
                continue
            r = m_seam("print", {"text": "    %-12s%s" % (label + ":", val or "")})
            if m_is_raise(r):
                return r
        r = m_seam("print", {"text": ""})
        if m_is_raise(r):
            return r
    r = m_seam("print", {"text":
        "  %d incident(s)  |  ack one with: hermes cron incidents ack <id>" % n})
    return r if m_is_raise(r) else 0


def m_doctor(_inp):
    jobs = m_seam("list_jobs", {"include_disabled": False})
    if m_is_raise(jobs):
        return jobs
    nj = len(jobs) if isinstance(jobs, list) else 0
    findings = []
    for i in range(nj):
        job = jobs[i]
        issues = m_seam("doctor_issues", {"job": denorm(job)})
        if m_is_raise(issues):
            return issues
        if isinstance(issues, list) and len(issues) > 0:
            findings.append((job, issues))
    if not findings:
        r = m_seam("print", {"text": "\u2713 Cron doctor found no issues"})
        if m_is_raise(r):
            return r
        note = "  Checked %d active job(s)." % nj if nj else "  No active jobs configured."
        r = m_seam("print", {"text": note})
        return r if m_is_raise(r) else 0
    count = sum(len(iss) for _, iss in findings)
    r = m_seam("print", {"text":
        "Cron doctor found %d issue(s) across %d job(s):" % (count, len(findings))})
    if m_is_raise(r):
        return r
    r = m_seam("print", {"text": ""})
    if m_is_raise(r):
        return r
    for job, issues in findings:
        r = m_seam("print", {"text": "  %s %s" % (
            m_gets(job, "id", "?"), m_gets(job, "name", "(unnamed)"))})
        if m_is_raise(r):
            return r
        for issue in issues:
            r = m_seam("print", {"text": "    - %s" % (issue if isinstance(issue, str) else "")})
            if m_is_raise(r):
                return r
    # live `print()` sits AFTER the for-loop, not inside it
    r = m_seam("print", {"text": ""})
    if m_is_raise(r):
        return r
    r = m_seam("print", {"text":
        "Review the findings above, then run `hermes cron doctor` again."})
    return r if m_is_raise(r) else 1


def mirror_drive(func, inp):
    interval = inp.get("ticker_interval", 60)
    if func == "builtin_gateway_liveness":
        ret = m_builtin_gateway_liveness(interval)
    elif func == "ticker_age_is_fresh":
        ret = m_ticker_age_is_fresh(inp.get("age"), interval)
    elif func == "store_unwritable_report":
        ret = m_store_unwritable_report()
    elif func == "print_store_unwritable":
        ret = m_print_store_unwritable(inp.get("report"), bool(inp.get("brief", False)))
    elif func == "probe_then_list_jobs":
        ret = m_probe_then_list_jobs(bool(inp.get("include_disabled", True)),
                                     bool(inp.get("brief", False)))
    elif func == "incidents":
        ret = m_incidents(inp.get("args") or {}, inp)
    elif func == "doctor":
        ret = m_doctor(inp)
    else:
        raise ValueError("unknown func: " + func)
    return {"ret": ret, "state": None}


def shadow(fixture):
    cases = mism = 0
    with open(fixture, encoding="utf-8") as fh:
        for line in fh:
            if not line.lstrip().startswith("{"):
                continue
            rec = json.loads(line)
            func = rec.get("func")
            inp = norm_nums(rec.get("in") or {})
            cases += 1
            CUR["func"] = func; CUR["in"] = inp
            install_case()
            try:
                mirror = mirror_drive(func, inp)
            except BaseException as e:
                mirror = {"ret": raise_spec(e), "state": None}
            mirror["trace"] = CUR["trace"]
            CUR["in"] = inp
            install_case()
            try:
                live = drive(func, inp)
            except BaseException as e:
                live = {"ret": raise_spec(e), "state": None}
            live["trace"] = CUR["trace"]
            if canon(mirror) != canon(live):
                mism += 1
                print("SHADOW MISMATCH %s" % func)
                print("  C-mirror: %s" % json.dumps(mirror, ensure_ascii=False, default=str))
                print("  PY-live : %s" % json.dumps(live, ensure_ascii=False, default=str))
    print("SHADOW cron_flow4: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


def main(argv):
    if "--shadow" in argv:
        fx = os.path.join(_REPO, "tests", "fixtures", "cron_flow4.jsonl")
        return shadow(fx)
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func; CUR["in"] = inp
        install_case()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE cron_flow4: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
