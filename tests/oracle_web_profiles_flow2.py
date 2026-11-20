#!/usr/bin/env python3
"""Oracle for web_profiles_flow2 — the UNCLAIMED pure/leaf helpers of
hermes_cli/web_routers/profiles.py.

A CRLF-aware PoP grep at authoring time found 17 of the module's 67 AST defs
already claimed by profiles.c / profiles_gaps.c / web_server.c /
web_server_wrappers.c / ws_revive2.c / ws_revive4.c. The 14 unclaimed helpers
ported here are claimed by src/cli/web_profiles_flow2.c (one qualified PoP each).

The oracle drives the LIVE defs AST-extracted from the quarry (no heavy import).
Every effectful collaborator is replaced by a recording stub returning the SAME
scripted results the C harness feeds its universal seam: a MISSING key
fail-closes with RuntimeError on both sides, a present JSON null is a valid
None, and an array script yields one result PER CALL (last repeats). Scripted
{"__raise__": cls} raises a REAL builtin of that class (full py3.14 parent
hierarchy incl. the BaseException trio and the exception-group families), so
KeyboardInterrupt/SystemExit/GeneratorExit/BaseExceptionGroup pass through
_best_effort while ExceptionGroup and the Exception family are caught. Args and
ret are DEEP-COPIED into the trace at call time, matching the C harness.

Input: stdin JSON lines from t_port_web_profiles_flow2:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE web_profiles_flow2: N cases, M mismatches. Fail-closed (exit 2)
on extraction misses, func-set drift, <5 cases/func, or zero compared cases.

--self-test drives every fixture LIVE (no C) and reports binding/script gaps.
--shadow  diffs a literal Python MIRROR of the C port against the LIVE drive
          over every fixture (the shadow-diff; must print 0 diffs).
"""
import ast
import copy
import json
import os
import sys
import types
from enum import Enum
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "web_routers", "profiles.py")

# harness func -> live python name (identical here; kept as a map for drift checks).
PY = {
    "recency": "_recency",
    "csv_list": "_csv_list",
    "slice_has_rows": "_slice_has_rows",
    "failed_load_slice": "_failed_load_slice",
    "profiles_failed": "_profiles_failed",
    "no_store_file": "_no_store_file",
    "tag_rows": "_tag_rows",
    "pinned_window": "_pinned_window",
    "linux_terminal_commands": "_linux_terminal_commands",
    "profile_state_db": "_profile_state_db",
    "corrupt_profile_stores": "_corrupt_profile_stores",
    "retryable_profile_errors": "_retryable_profile_errors",
    "scoped_profile_targets": "_scoped_profile_targets",
    "best_effort": "_best_effort",
}
WANTED = set(PY.values())
SEAM_NAMES = ["store_location", "storage_health", "profile_targets", "fn", "log_exception"]

CUR = {"trace": [], "scripts": {}, "idx": {}}
NS = {}


# ── seam recorder ──

def seam(name, args):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, dict) and "__arr__" in sc:
            r = sc["__arr__"]
        elif isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    return maybe_raise(r)


# Full py3.14 builtin exception parent hierarchy.
EXC_MAP = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "PythonFinalizationError": getattr(__builtins__, "PythonFinalizationError", RuntimeError),
    "SyntaxError": SyntaxError, "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
    "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        msg = r.get("msg", "")
        if name in ("BaseExceptionGroup", "ExceptionGroup"):
            # Subclass so str(ex) == msg exactly (the C port propagates the raw
            # script msg; the stock __str__ appends " (1 sub-exception)").
            base = EXC_MAP[name]
            cls = type(name, (base,), {
                "__str__": lambda self: str(self.args[0]) if self.args else ""})
            raise cls(msg, [ValueError("child")])
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(msg)
    return r


def is_exception_name(name):
    """Mirror of wp2_is_exception(): is this class an Exception subclass?"""
    cls = EXC_MAP.get(name)
    if cls is None:
        return True
    return issubclass(cls, Exception)


# ── extraction ──

def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "hermes_cli.web_profiles_flow2_extracted",
        "json": json, "os": os, "sys": sys, "frozenset": frozenset,
    }

    class StorageState(Enum):
        OK = "ok"
        CORRUPT = "corrupt"

    ns["StorageState"] = StorageState
    tree = ast.parse(src)

    # module-level literal constant the terminal-command helper reads
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_LINUX_TERMINALS" for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    # target defs (plus _corrupt_profile_stores, called by _retryable_profile_errors)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    if "_LINUX_TERMINALS" not in ns:
        print("FATAL missing constant: _LINUX_TERMINALS")
        sys.exit(2)
    global NS
    NS = ns
    return ns


# ── recording doubles / stubs ──

def FN_STUB():
    return seam("fn", {})


class HealthDouble:
    def __init__(self, state):
        self.state = state

    def storage_state(self):
        v = self.state.get("state") if isinstance(self.state, dict) else self.state
        return NS["StorageState"].CORRUPT if v == "corrupt" else NS["StorageState"].OK


def install_stubs(ns):
    def profile_scope(home=None, **kw):
        return types.SimpleNamespace(home=home)

    def store_location(scope):
        return seam("store_location", {"home": scope.home})

    def storage_health(scope):
        return HealthDouble(seam("storage_health", {"home": scope.home}))

    ns["Path"] = lambda p: p
    ns["profile_scope"] = profile_scope
    ns["store_location"] = store_location
    ns["storage_health"] = storage_health
    ns["_profile_targets"] = lambda log_label: seam("profile_targets", {"log_label": log_label})
    ns["_log"] = types.SimpleNamespace(
        exception=lambda msg, *a: seam("log_exception", {"log_msg": msg, "args": list(a)}),
        warning=lambda *a, **k: seam("log_warning", {"args": list(a)}),
    )


# ── live drive ──

def drive(ns, func, inp):
    d = copy.deepcopy
    if func == "recency":
        return ns["_recency"](d(inp.get("s") or {}))
    if func == "csv_list":
        return ns["_csv_list"](inp.get("value"))
    if func == "slice_has_rows":
        return ns["_slice_has_rows"](d(inp.get("slices") or {}))
    if func == "failed_load_slice":
        return ns["_failed_load_slice"](d(inp.get("errors") or []), **d(inp.get("extra") or {}))
    if func == "profiles_failed":
        return ns["_profiles_failed"](d(inp.get("errors") or []))
    if func == "no_store_file":
        errs = d(inp["errors"]) if inp.get("errors") is not None else None
        ns["_no_store_file"](inp.get("name") or "", errs)
        return errs
    if func == "tag_rows":
        rows = d(inp.get("rows") or [])
        return ns["_tag_rows"](rows, inp.get("name") or "", inp.get("now", 0))
    if func == "pinned_window":
        return ns["_pinned_window"](d(inp.get("rows") or []), inp.get("offset", 0), inp.get("cap", 0))
    if func == "linux_terminal_commands":
        return ns["_linux_terminal_commands"](inp.get("command") or "")
    if func == "profile_state_db":
        return ns["_profile_state_db"](inp.get("home") or "")
    if func == "corrupt_profile_stores":
        return ns["_corrupt_profile_stores"](d(inp.get("targets") or []))
    if func == "retryable_profile_errors":
        return ns["_retryable_profile_errors"](d(inp.get("errors") or []), d(inp.get("scanned") or []))
    if func == "scoped_profile_targets":
        return ns["_scoped_profile_targets"](inp.get("log_label") or "", inp.get("profile"))
    if func == "best_effort":
        return ns["_best_effort"](inp.get("log_msg") or "", *inp.get("args", []),
                                  fn=FN_STUB, default=d(inp.get("default")))
    raise ValueError("unhandled func: %s" % func)


# ── comparison ──

def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def run_live(func, inp):
    """Drive live; return (want_object, raised_or_None)."""
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}
    try:
        ret = drive(NS, func, inp)
        return {"ret": ret, "trace": CUR["trace"]}, None
    except BaseException as ex:
        return ({"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                 "trace": CUR["trace"]}, ex)


def fixture():
    return _REPO / "tests" / "fixtures" / "web_profiles_flow2.jsonl"


def load_cases():
    p = fixture()
    if not p.exists():
        return []
    return [json.loads(l) for l in p.read_text(encoding="utf-8").splitlines() if l.strip()]


def check_func_set(cases):
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(ff - set(PY)), sorted(set(PY) - ff)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("FATAL <5 cases for: %s" % thin)
        sys.exit(2)


def self_test():
    ns = extract()
    install_stubs(ns)
    cases = load_cases()
    check_func_set(cases)
    bad = 0
    for c in cases:
        _, ex = run_live(c["func"], c["in"])
        if isinstance(ex, AttributeError) and any(
                ("%s" % n) in str(ex) for n in list(PY) + SEAM_NAMES):
            print("SELFTEST BINDING %s: %s: %s" % (c["func"], type(ex).__name__, ex))
            bad += 1
        elif isinstance(ex, NameError):
            print("SELFTEST NAMEERROR %s: %s" % (c["func"], ex))
            bad += 1
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


# ── literal Python mirror of the C port (for --shadow) ──

_NO_STORE_MSG = ("the sidebar reads a profile's session store file, and this "
                 "profile's store has no file")
_TERMS = [
    ("x-terminal-emulator", "-e"), ("gnome-terminal", "--"), ("konsole", "-e"),
    ("xfce4-terminal", None), ("mate-terminal", None), ("lxterminal", None),
    ("tilix", "-e"), ("alacritty", "-e"), ("kitty", ""), ("xterm", "-e"),
]


def _shadow_inner(func, inp):
    d = copy.deepcopy
    if func == "recency":
        s = inp.get("s") or {}
        if s.get("last_active"):
            return s["last_active"]
        if s.get("started_at"):
            return s["started_at"]
        return 0

    if func == "csv_list":
        v = inp.get("value") or ""
        v = v if isinstance(v, str) else ""
        return [x.strip() for x in v.split(",") if x.strip()]

    if func == "slice_has_rows":
        s = inp.get("slices") or {}
        return any(bool(s.get(k)) for k in ("recents", "cron", "messaging"))

    if func == "failed_load_slice":
        errs = inp.get("errors") or []
        out = {"failed": True, "retry": True,
               "errors": [dict(e) if isinstance(e, dict) else {} for e in errs]}
        for k, v in (inp.get("extra") or {}).items():
            out[k] = v
        return out

    if func == "profiles_failed":
        out = {}
        for e in (inp.get("errors") or []):
            if not isinstance(e, dict):
                continue
            p = e.get("profile")
            if not p or not isinstance(p, str):
                continue
            out[p] = {"failed": True, "retry": True, "error": e.get("error", "")}
        return out

    if func == "no_store_file":
        if inp.get("errors") is None:
            return None
        out = d(inp["errors"])
        out.append({"profile": inp.get("name") or "", "error": _NO_STORE_MSG})
        return out

    if func == "tag_rows":
        out = d(inp.get("rows") or [])
        name = inp.get("name") or ""
        now = inp.get("now", 0)
        for s in out:
            if not isinstance(s, dict):
                continue
            s["profile"] = name
            s["is_default_profile"] = (name == "default")
            ended = s.get("ended_at")
            ended_none = ended is None
            chosen = s["last_active"] if "last_active" in s else (
                s["started_at"] if "started_at" in s else 0)
            last = chosen if (isinstance(chosen, (int, float)) and not isinstance(chosen, bool)) else 0
            s["is_active"] = bool(ended_none and (now - last < 300))
            s["archived"] = bool(s.get("archived"))
            s["pinned"] = bool(s.get("pinned"))
        return out

    if func == "pinned_window":
        rows = inp.get("rows") or []
        off = inp.get("offset", 0)
        end = off + inp.get("cap", 0)
        window = [d(r) for r in rows[off:end]]
        if len(rows) > end:
            window += [d(r) for r in rows[end:] if (isinstance(r, dict) and r.get("pinned"))]
        return window

    if func == "linux_terminal_commands":
        cmd = inp.get("command") or ""
        quoted = "sh -lc '%s'" % cmd
        out = []
        for exe, flag in _TERMS:
            if flag is None:
                args = [exe, "-e", quoted]
            elif flag:
                args = [exe, flag, "sh", "-lc", cmd]
            else:
                args = [exe, "sh", "-lc", cmd]
            out.append([exe, args])
        return out

    if func == "profile_state_db":
        return seam("store_location", {"home": inp.get("home") or ""})

    if func == "corrupt_profile_stores":
        out = {}
        for t in (inp.get("targets") or []):
            if not isinstance(t, list) or len(t) < 2:
                continue
            name = t[0] if isinstance(t[0], str) else None
            if not name:
                continue
            r = seam("storage_health", {"home": t[1] if isinstance(t[1], str) else ""})
            if isinstance(r, dict) and "__raise__" in r:
                return r
            v = r.get("state") if isinstance(r, dict) else r
            if v == "corrupt":
                out[name] = "corrupt"
        return out

    if func == "retryable_profile_errors":
        corrupt = _shadow_inner("corrupt_profile_stores",
                                {"targets": inp.get("scanned") or []})
        if isinstance(corrupt, dict) and "__raise__" in corrupt:
            return corrupt
        out = []
        for e in (inp.get("errors") or []):
            if not isinstance(e, dict):
                continue
            p = e.get("profile")
            if p in corrupt:
                continue
            out.append(d(e))
        return out

    if func == "scoped_profile_targets":
        raw = inp.get("profile") or "all"
        sc = raw.strip() or "all"
        targets = seam("profile_targets", {"log_label": inp.get("log_label") or ""})
        if isinstance(targets, dict) and "__raise__" in targets:
            return targets
        if sc == "all":
            return d(targets or [])
        out = []
        for t in (targets or []):
            if isinstance(t, list) and t and t[0] == sc:
                out.append(d(t))
        return out

    if func == "best_effort":
        try:
            return seam("fn", {})
        except BaseException as ex:
            name = type(ex).__name__
            spec = {"__raise__": name, "msg": str(ex)}
            if is_exception_name(name):
                seam("log_exception", {"log_msg": inp.get("log_msg") or "",
                                       "args": d(inp.get("args") or [])})
                return d(inp.get("default"))
            return spec

    raise ValueError("unhandled func: %s" % func)


def shadow_drive(func, inp):
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}
    try:
        return _shadow_inner(func, inp)
    except BaseException as ex:
        return {"__raise__": type(ex).__name__, "msg": str(ex)}


def shadow_mode():
    cases = load_cases()
    check_func_set(cases)
    extract()
    install_stubs(NS)
    diffs = 0
    for c in cases:
        want, _ = run_live(c["func"], c["in"])
        shadow_ret = shadow_drive(c["func"], c["in"])
        got = {"ret": shadow_ret, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            diffs += 1
            print("SHADOW DIFF %s" % c["func"])
            print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False))
            print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False))
    print("SHADOW web_profiles_flow2: %d cases, %d diffs" % (len(cases), diffs))
    sys.exit(1 if diffs else 0)


def main():
    if "--self-test" in sys.argv:
        self_test()
        return
    if "--shadow" in sys.argv:
        shadow_mode()
        return
    ns = extract()
    install_stubs(ns)
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
        want, _ = run_live(func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE web_profiles_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE web_profiles_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
