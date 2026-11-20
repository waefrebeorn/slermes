#!/usr/bin/env python3
"""Oracle for backup_flow4 -- the last UNCLAIMED def of hermes_cli/backup.py
beyond backup_flow2 / backup_flow3.

A CRLF-aware, AST/class-qualified PoP grep at authoring time found 64 defs /
63 unique names in the quarry backup.py; backup.c + backup_gaps.c +
backup_flow2.c + backup_flow3.c carry inline PoP markers for all but the two
nested `_lock_op` closures inside `_backup_operation_lock` (lines 202 / 208).
Those two closures differ only by the `os.name` branch and are driven here as
ONE FLOW core parameterised by `os` ("nt" / "posix"), matching the live Python
where `_backup_operation_lock` binds exactly one closure at runtime.

Drives the LIVE def AST-extracted from the quarry backup.py (no heavy import:
backup.py imports `persistence`, absent from the oracle venv). The two nested
closures are exec'd straight into a namespace with the file-handle / msvcrt /
fcntl doubles bound as free globals; every effectful collaborator is a
recording stub returning the SAME scripted result the C harness feeds its
universal seam: a MISSING key fail-closes with RuntimeError on both sides, a
present JSON null is a valid None, and an array script yields one result PER
CALL (last repeats). Scripted {"__raise__":cls} raises a REAL builtin of that
class (full py3.14 parent hierarchy, including the BaseException trio), so the
`except BaseException` recorder in run_live captures the exact class name and
message the C port propagates as a raise descriptor.

Input: stdin JSON lines from t_port_backup_flow4:
    {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}
Summary: ORACLE backup_flow4: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses the def or zero cases were compared.

--self-test drives every fixture LIVE (no C) and reports binding/script gaps.
--shadow  diffs a literal Python MIRROR of the C port against the LIVE drive
          over every fixture (the shadow-diff; must print 0 diffs).
"""
import ast
import copy
import json
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path as _Path

_REPO = _Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "backup.py")

# harness func -> live python name (the single unclaimed def family)
PY = {"lock_op": "_lock_op"}
WANTED = set(PY.values())

CUR = {"trace": [], "scripts": {}, "idx": {}}


# ---- seam recorder ----

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


# Full builtin exception hierarchy (py3.14) + backup.py custom classes.
_zlib_error = type("zlib.error", (Exception,), {})
_lzma_error = type("LZMAError", (Exception,), {})
_bad_zip = type("BadZipFile", (Exception,), {})
_sqlite_err = type("_SQLiteSnapshotError", (Exception,), {})
_backup_in_progress = type("BackupInProgressError", (RuntimeError,), {})

EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "FileExistsError": FileExistsError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "TimeoutError": TimeoutError, "BlockingIOError": BlockingIOError,
    "ProcessLookupError": ProcessLookupError, "ChildProcessError": ChildProcessError,
    "InterruptedError": InterruptedError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "AssertionError": AssertionError, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "ArithmeticError": ArithmeticError,
    "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
    "FloatingPointError": FloatingPointError, "MemoryError": MemoryError,
    "ReferenceError": ReferenceError, "SystemError": SystemError,
    "BufferError": BufferError, "EOFError": EOFError, "Exception": Exception,
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "BadZipFile": _bad_zip, "zlib.error": _zlib_error, "LZMAError": _lzma_error,
    "_SQLiteSnapshotError": _sqlite_err, "BackupInProgressError": _backup_in_progress,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


# ---- live collaborators + nested-closure extraction ----

class Obj:
    def __init__(self, **kw): self.__dict__.update(kw)


class _Handle:
    """File-handle double: the free var `handle` in both _lock_op closures."""
    def seek(self, offset):
        return seam("handle.seek", {"offset": offset})
    def fileno(self):
        return seam("handle.fileno", {})


class _Msvcrt:
    LK_NBLCK = 1
    LK_UNLCK = 8
    @staticmethod
    def locking(fd, flag, nbytes):
        return seam("msvcrt.locking", {"fd": fd, "flag": flag, "nbytes": nbytes})


class _Fcntl:
    LOCK_EX = 2
    LOCK_NB = 4
    LOCK_UN = 8
    @staticmethod
    def flock(fd, flag):
        return seam("fcntl.flock", {"fd": fd, "flag": flag})


LIVE = {}
_EXTRACTED = [False]


def extract_once():
    if _EXTRACTED[0]:
        return
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    nodes = [n for n in ast.walk(tree)
             if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == "_lock_op"]
    if len(nodes) < 2:
        print("FATAL expected >=2 nested _lock_op defs, found %d" % len(nodes))
        sys.exit(2)
    LIVE.clear()
    for node in nodes:
        seg = ast.get_source_segment(src, node) or ""
        osname = "nt" if "msvcrt.locking" in seg else "posix"
        d = {"__name__": "hermes_cli.backup_flow4_extracted",
             "handle": _Handle(), "msvcrt": _Msvcrt(), "fcntl": _Fcntl()}
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), d)
        LIVE[osname] = d["_lock_op"]
    missing = WANTED - {"_lock_op"}
    if missing or set(LIVE) != {"nt", "posix"}:
        print("FATAL extraction incomplete: LIVE=%s" % sorted(LIVE))
        sys.exit(2)
    _EXTRACTED[0] = True


# ---- live drive ----

def drive(func, inp):
    osname = "nt" if inp.get("os") == "nt" else "posix"
    flag = int(inp.get("flag", 0))
    LIVE[osname](flag)          # returns None; any seam raise propagates
    return None


def _numnorm(o):
    if type(o) is float and o.is_integer(): return int(o)
    if isinstance(o, dict): return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)): return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def reset_case(inp):
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}


def run_live(func, inp):
    extract_once()
    reset_case(inp)
    try:
        ret = drive(func, inp)
        return {"ret": ret, "trace": CUR["trace"]}, None
    except BaseException as ex:
        return {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                "trace": CUR["trace"]}, ex


def fixture():
    return _REPO / "tests" / "fixtures" / "backup_flow4.jsonl"


def load_cases():
    p = fixture()
    if not p.exists(): return []
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


SEAM_NAMES = ["handle.seek", "handle.fileno", "msvcrt.locking", "fcntl.flock"]


def self_test():
    extract_once()
    cases = load_cases()
    check_func_set(cases)
    bad = 0
    for c in cases:
        _, ex = run_live(c["func"], c["in"])
        if isinstance(ex, AttributeError) and any(
                ("'%s'" % n) in str(ex) for n in list(PY) + SEAM_NAMES):
            print("SELFTEST BINDING %s: %s: %s" % (c["func"], type(ex).__name__, ex))
            bad += 1
        elif isinstance(ex, NameError):
            print("SELFTEST NAMEERROR %s: %s" % (c["func"], ex))
            bad += 1
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


# ---- shadow-only seam variants (no raise; return the raw spec) ----

def sseam(name, args):
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
    return r


def _is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


# ---- shadow mirror (literal transcription of the C port) ----

def shadow_drive(func, inp):
    reset_case(inp)
    osname = inp.get("os", "posix")
    nt = osname == "nt"
    flag = inp.get("flag", 0)
    if nt:
        r = sseam("handle.seek", {"offset": 0})
        if _is_raise(r): return r
        fd = sseam("handle.fileno", {})
        if _is_raise(fd): return fd
        r = sseam("msvcrt.locking", {"fd": fd, "flag": flag, "nbytes": 1})
        if _is_raise(r): return r
    else:
        fd = sseam("handle.fileno", {})
        if _is_raise(fd): return fd
        r = sseam("fcntl.flock", {"fd": fd, "flag": flag})
        if _is_raise(r): return r
    return None


def shadow_mode():
    cases = load_cases()
    check_func_set(cases)
    extract_once()
    diffs = 0
    for c in cases:
        want, _ = run_live(c["func"], c["in"])
        try:
            shadow_ret = shadow_drive(c["func"], c["in"])
        except BaseException as ex:
            shadow_ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
        got = {"ret": shadow_ret, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            diffs += 1
            print("SHADOW DIFF %s" % c["func"])
            print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("SHADOW backup_flow4: %d cases, %d diffs" % (len(cases), diffs))
    sys.exit(1 if diffs else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(); return
    if "--shadow" in sys.argv:
        shadow_mode(); return
    extract_once()
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
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
            print("  C : %s" % json.dumps(got, ensure_ascii=False, default=str))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE backup_flow4: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE backup_flow4: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
