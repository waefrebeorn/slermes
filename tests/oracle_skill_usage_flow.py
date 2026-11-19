#!/usr/bin/env python3
"""Oracle for skill_usage_flow.c — the UNCLAIMED tools/skill_usage.py FLOW cores
+ leaves (LIVE authority): sidecar I/O (_read_lines, _load_usage_strict,
is_locked, locked_skill_names), record helpers (_int_or_zero, _backfilled,
_report_row, _bump), provenance scans (_read_bundled_names,
_toggle_suppressed_name, _iter_skill_mds, _scan_local_skills, _match_skill_dir,
is_external_only, _external_skill_names), the locked-update orchestrator
(_locked_update), mutator wrappers (_set_field, _mutate_and_emit,
reanchor_clock, set_locked), the file-lock CM (_flock, skill_file_lock) and the
archive/restore mover (_relocate).

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy tools import), module constants seeded, and every claimed sibling
(_empty_record, latest_activity_at, activity_count, _usage_file, load_usage,
save_usage, _non_negative_int, _now_iso, read_suppressed_names,
_read_hub_installed_names, is_protected_builtin, read_skill_name, _find_skill_dir,
_find_external_skill_dir, _skills_dir, _mutate, _emit_skill_lifecycle,
_usage_file_lock, set_state, is_bundled) plus every foreign collaborator (Path
filesystem probes, open/fd, fcntl/msvcrt, json.loads, utils.atomic_write_text,
agent.skill_utils helpers, tools.skill_ledger, shutil.move, os.utime) replaced by
recording stubs returning the SAME scripted results the C harness feeds its
universal seam. Ported-to-ported calls compose REAL on both sides, so their
internal seam events appear in both traces.

Mutator closures passed to the CLAIMED _mutate are projected to their live
__qualname__ and never execute (the claimed sibling owns running them).
Path-valued results materialize as PathDouble (PurePosixPath algebra, seamed
filesystem probes) so str() keeps POSIX separators on this Windows host.
skill_file_lock is driven as a real @contextmanager with a "body" seam at the
yield; _usage_file_lock is a claimed CM double (enter/exit seam pair).

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. logger.debug
is an inert recording event (RecLogger never raises; scripted null in fixtures).
Uncaught raises (incl. the BaseException trio) are caught into a ret spec;
`except Exception` / `except OSError` / suppress sites swallow exactly their live
kinds (the full builtin hierarchy is honored via real exception classes).

Input: stdin JSON lines from t_port_skill_usage_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}}}
Summary: ORACLE skill_usage_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import copy
import json
import logging
import os
import sys
import threading
import types
import __future__
from contextlib import contextmanager
from pathlib import PurePath, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path_repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUARRY = os.path.join(os.path.dirname(_REPO), "hermes-agent")
if not os.path.isdir(QUARRY):
    print("ORACLE skill_usage_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))
os.environ.setdefault("LOCALAPPDATA", os.path.join(os.path.dirname(_REPO), "AppData", "Local"))
os.environ.setdefault("HERMES_HOME", os.path.join(_REPO, ".testbin", "skill_usage_flow_home"))

SRC = os.path.join(QUARRY, "tools", "skill_usage.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# harness dispatch name -> live def name
PY = {
    "int_or_zero": "_int_or_zero",
    "read_lines": "_read_lines",
    "backfilled": "_backfilled",
    "report_row": "_report_row",
    "load_usage_strict": "_load_usage_strict",
    "is_locked": "is_locked",
    "locked_skill_names": "locked_skill_names",
    "match_skill_dir": "_match_skill_dir",
    "is_external_only": "is_external_only",
    "external_skill_names": "_external_skill_names",
    "iter_skill_mds": "_iter_skill_mds",
    "read_bundled_names": "_read_bundled_names",
    "toggle_suppressed_name": "_toggle_suppressed_name",
    "scan_local_skills": "_scan_local_skills",
    "bump": "_bump",
    "locked_update": "_locked_update",
    "set_field": "_set_field",
    "mutate_and_emit": "_mutate_and_emit",
    "reanchor_clock": "reanchor_clock",
    "set_locked": "set_locked",
    "flock": "_flock",
    "skill_file_lock": "skill_file_lock",
    "relocate": "_relocate",
}
TOP_TARGETS = list(PY.values())

CUR = {"scripts": {}, "idx": {}, "trace": [], "state": None}


class RecLogger:
    def debug(self, *a, **k):
        fmt = a[0] if a else ""
        args = [str(x) if isinstance(x, BaseException) else x for x in a[1:]]
        ev = {"fmt": fmt, "args": args}
        if "exc_info" in k:
            ev["exc_info"] = k["exc_info"]
        CUR["trace"].append({"ev": "logger.debug", "args": ev, "ret": None})

    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
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


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return str(o)
    if isinstance(o, PurePath):
        return o.as_posix()
    if isinstance(o, FdDouble):
        return o.tag
    if isinstance(o, BaseException):
        return str(o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


# ── Path / fd doubles (PurePosixPath algebra, seamed filesystem probes) ──

class PathDouble:
    def __init__(self, s):
        self._s = str(s)

    def __str__(self):
        return self._s

    def as_posix(self):
        return self._s

    def __truediv__(self, other):
        return PathDouble(str(PurePosixPath(self._s) / str(other)))

    @property
    def parent(self):
        return PathDouble(str(PurePosixPath(self._s).parent))

    @property
    def name(self):
        return PurePosixPath(self._s).name

    def __eq__(self, o):
        return isinstance(o, PathDouble) and self._s == o._s

    def __hash__(self):
        return hash(self._s)

    def exists(self):
        return seam("path.exists", {"path": self._s})

    def is_dir(self):
        return seam("path.is_dir", {"path": self._s})

    def read_text(self, encoding=None, errors=None):
        a = {"path": self._s}
        if encoding is not None:
            a["encoding"] = encoding
        if errors is not None:
            a["errors"] = errors
        return seam("path.read_text", a)

    def mkdir(self, parents=False, exist_ok=False):
        return seam("path.mkdir", {"path": self._s, "parents": parents, "exist_ok": exist_ok})

    def stat(self):
        r = seam("path.stat", {"path": self._s})
        return types.SimpleNamespace(**r) if isinstance(r, dict) else r

    def write_text(self, text, encoding=None):
        a = {"path": self._s, "text": text}
        if encoding is not None:
            a["encoding"] = encoding
        return seam("path.write_text", a)

    def rglob(self, pattern):
        r = seam("path.rglob", {"path": self._s, "pattern": pattern})
        return [PathDouble(p) for p in r] if isinstance(r, list) else r

    def rename(self, dest):
        return seam("path.rename", {"src": self._s, "dest": str(dest)})


class FdDouble:
    def __init__(self, tag):
        self.tag = tag

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def seek(self, pos):
        return seam("fd.seek", {"fd": self.tag, "pos": pos})

    def fileno(self):
        return seam("fd.fileno", {"fd": self.tag})


class UsageLockCM:
    """_usage_file_lock (claimed) double: enter/exit seam pair, never suppresses."""
    def __enter__(self):
        return seam("_usage_file_lock.enter", {})

    def __exit__(self, et, ev, tb):
        exc = {"kind": et.__name__, "msg": str(ev)} if et is not None else None
        seam("_usage_file_lock.exit", {"exc": exc})
        return False


# ── recording stubs ──

def mk(name, proj):
    def f(*a, **k):
        return seam(name, proj(a, k))
    f.__name__ = name
    return f


def _path_result(r):
    return PathDouble(r) if isinstance(r, str) else r


def _set_result(r):
    return set(r) if isinstance(r, list) else r


def _mutator_proj(m):
    return getattr(m, "__qualname__", getattr(m, "__name__", "?"))


def _mutate_rec(skill_name, mutator, *, require_curation_eligible=False):
    return seam("_mutate", {"skill_name": skill_name, "mutator": _mutator_proj(mutator),
                            "require_curation_eligible": require_curation_eligible})


def _emit_rec(skill_name, action, *, record=None, task_id=None, session_id=None):
    return seam("_emit_skill_lifecycle", {"skill_name": skill_name, "action": action,
                                          "record": copy.deepcopy(record), "task_id": task_id,
                                          "session_id": session_id})


def _read_skill_name_rec(skill_md, fallback):
    return seam("read_skill_name", {"skill_md": str(skill_md), "fallback": fallback})


def _atomic_write_rec(path, text, tmp_prefix=None):
    return seam("atomic_write_text", {"path": str(path), "text": text, "tmp_prefix": tmp_prefix})


def _open_rec(path, mode, encoding=None):
    r = seam("open", {"path": str(path), "mode": mode, "encoding": encoding})
    return FdDouble(r) if isinstance(r, str) else r


def _fcntl_flock_rec(fd, op):
    return seam("fcntl.flock", {"fd": fd.tag if isinstance(fd, FdDouble) else fd, "operation": op})


def _msvcrt_locking_rec(fd, op, n):
    return seam("msvcrt.locking", {"fd": fd, "lock": op, "nbytes": n})


def _capture_before_rec(src, **kw):
    return seam("skill_ledger.capture_before", {"src": str(src), "capture_kwargs": kw})


def _record_mutation_rec(action, skill_name, before=None, after_root=None):
    return seam("skill_ledger.record_mutation",
                {"action": action, "skill_name": skill_name,
                 "before": copy.deepcopy(before), "after_root": str(after_root)})


def _shutil_move_rec(src, dest):
    return seam("shutil.move", {"src": src, "dest": dest})


def _os_utime_rec(path):
    return seam("os.utime", {"path": str(path)})


def _json_loads_rec(text):
    return seam("json.loads", {"text": text})


def seam_bindings():
    return {
        # claimed siblings
        "_empty_record": mk("_empty_record", lambda a, k: {}),
        "latest_activity_at": mk("latest_activity_at", lambda a, k: {"record": copy.deepcopy(a[0])}),
        "activity_count": mk("activity_count", lambda a, k: {"record": copy.deepcopy(a[0])}),
        "_usage_file": lambda: _path_result(seam("_usage_file", {})),
        "load_usage": mk("load_usage", lambda a, k: {}),
        "save_usage": mk("save_usage", lambda a, k: {"data": copy.deepcopy(a[0])}),
        "_non_negative_int": mk("_non_negative_int", lambda a, k: {"value": a[0]}),
        "_now_iso": mk("_now_iso", lambda a, k: {}),
        "read_suppressed_names": lambda: _set_result(seam("read_suppressed_names", {})),
        "_read_hub_installed_names": lambda: _set_result(seam("_read_hub_installed_names", {})),
        "is_protected_builtin": mk("is_protected_builtin", lambda a, k: {"skill_name": a[0]}),
        "read_skill_name": _read_skill_name_rec,
        "_find_skill_dir": mk("_find_skill_dir", lambda a, k: {"skill_name": a[0]}),
        "_find_external_skill_dir": mk("_find_external_skill_dir", lambda a, k: {"skill_name": a[0]}),
        "_skills_dir": lambda: _path_result(seam("_skills_dir", {})),
        "_mutate": _mutate_rec,
        "_emit_skill_lifecycle": _emit_rec,
        "_usage_file_lock": lambda: UsageLockCM(),
        "set_state": mk("set_state", lambda a, k: {"skill_name": a[0], "state": a[1]}),
        "is_bundled": mk("is_bundled", lambda a, k: {"skill_name": a[0]}),
        # foreign collaborators (bare globals)
        "is_excluded_skill_path": mk("is_excluded_skill_path", lambda a, k: {"path": str(a[0])}),
        "is_external_skill_path": mk("is_external_skill_path", lambda a, k: {"path": str(a[0])}),
        "atomic_write_text": _atomic_write_rec,
        "get_hermes_home": lambda: _path_result(seam("get_hermes_home", {})),
    }


def install_module_stubs():
    """sys.modules stubs for every in-function import the targets execute."""
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    agent = mod("agent")
    agent.__path__ = []
    su = mod("agent.skill_utils",
             get_all_skills_dirs=lambda: [_path_result(p) for p in seam("get_all_skills_dirs", {})],
             is_excluded_skill_path=mk("is_excluded_skill_path", lambda a, k: {"path": str(a[0])}),
             is_external_skill_path=mk("is_external_skill_path", lambda a, k: {"path": str(a[0])}))
    agent.skill_utils = su

    tools = mod("tools")
    tools.__path__ = []
    ledger = types.SimpleNamespace(capture_before=_capture_before_rec,
                                   record_mutation=_record_mutation_rec)
    tools.skill_ledger = ledger
    sys.modules["tools.skill_ledger"] = ledger

    mod("shutil", move=_shutil_move_rec)
    mod("utils", atomic_write_text=_atomic_write_rec)
    mod("hermes_constants", get_hermes_home=lambda: _path_result(seam("get_hermes_home", {})))


# ── extraction ──

def extract():
    os_double = types.SimpleNamespace(utime=_os_utime_rec)
    json_double = types.SimpleNamespace(loads=_json_loads_rec, dumps=lambda *a, **k: "")
    ns = {
        "__name__": "tools.skill_usage_flow_extracted",
        "__builtins__": builtins.__dict__,
        "copy": copy, "os": os_double, "json": json_double, "logging": logging,
        "threading": threading, "Path": PathDouble, "PurePosixPath": PurePosixPath,
        "contextmanager": contextmanager, "suppress": __import__("contextlib").suppress,
        "logger": RecLogger(), "fcntl": None, "msvcrt": None,
        "open": _open_rec,   # shadow builtin open for skill_file_lock
    }
    ns.update(seam_bindings())
    tree = ast.parse(SRC_TEXT)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), ns)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                try:
                    exec_node(node)
                except Exception:
                    pass

    seed_consts()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_TARGETS:
            exec_node(node)
    seed_consts()
    ns["logger"] = RecLogger()   # logger = logging.getLogger(...) is an Assign

    missing = [n for n in TOP_TARGETS if n not in ns]
    if missing:
        print("ORACLE skill_usage_flow: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    # skill_file_lock's @contextmanager decorator is PART of the FunctionDef
    # node (ast.Module(body=[node]) keeps decorator_list), so it is already a
    # contextmanager factory — do NOT re-wrap.
    return ns


# ── drive ──

def drive(ns, func, inp):
    g = inp.get
    CUR["state"] = None

    if func == "int_or_zero":
        return ns["_int_or_zero"](g("value")), None
    if func == "read_lines":
        return ns["_read_lines"](PathDouble(g("path", "")), g("fail_log", "")), None
    if func == "backfilled":
        return ns["_backfilled"](g("rec")), None
    if func == "report_row":
        return ns["_report_row"](g("name", ""), g("raw"), **(g("extra") or {})), None
    if func == "load_usage_strict":
        return ns["_load_usage_strict"](), None
    if func == "is_locked":
        return ns["is_locked"](g("skill_name", "")), None
    if func == "locked_skill_names":
        return ns["locked_skill_names"](strict=g("strict", False)), None
    if func == "match_skill_dir":
        mds = [PathDouble(p) for p in (g("skill_mds") or [])]
        return ns["_match_skill_dir"](mds, g("skill_name", "")), None
    if func == "is_external_only":
        return ns["is_external_only"](g("skill_name", "")), None
    if func == "external_skill_names":
        return ns["_external_skill_names"](), None
    if func == "iter_skill_mds":
        return list(ns["_iter_skill_mds"](PathDouble(g("base", "")),
                                          local_only=g("local_only", True))), None
    if func == "read_bundled_names":
        return ns["_read_bundled_names"](), None
    if func == "toggle_suppressed_name":
        ns["_toggle_suppressed_name"](g("skill_name", ""), add=g("add", False))
        return None, None
    if func == "scan_local_skills":
        keep = lambda name, md, bundled, usage: seam(
            "keep", {"name": name, "skill_md": str(md),
                     "bundled": sorted(bundled), "usage": copy.deepcopy(usage)})
        return ns["_scan_local_skills"](keep), None
    if func == "bump":
        rec = copy.deepcopy(g("rec") or {})
        CUR["state"] = {"rec": rec}
        ns["_bump"](rec, g("count_key", ""), g("ts_key", ""))
        return None, CUR["state"]
    if func == "locked_update":
        CUR["state"] = {"data": {}}
        real_lus = ns["_load_usage_strict"]

        def wrapped():
            d = real_lus()
            CUR["state"]["data"] = d
            return d

        ns["_load_usage_strict"] = wrapped
        op = lambda data: seam("op", {"data": copy.deepcopy(data)})
        guard = (lambda: seam("guard", {})) if g("has_guard", False) else None
        try:
            ret = ns["_locked_update"](g("skill_name", ""), op, g("fail_log", ""), guard)
        finally:
            ns["_load_usage_strict"] = real_lus
        return ret, CUR["state"]
    if func == "set_field":
        return ns["_set_field"](g("skill_name", ""), g("key", ""), g("value")), None
    if func == "mutate_and_emit":
        mut = lambda rec: None
        mut.__qualname__ = g("mutator", "")
        hk = g("hook_kwargs") or {}
        ns["_mutate_and_emit"](g("skill_name", ""), g("action", ""), mut, **hk)
        return None, None
    if func == "reanchor_clock":
        ns["reanchor_clock"](g("skill_name", ""))
        return None, None
    if func == "set_locked":
        return ns["set_locked"](g("skill_name", ""), g("locked", False)), None
    if func == "flock":
        fd = FdDouble(g("fd", ""))
        ns["fcntl"] = types.SimpleNamespace(flock=_fcntl_flock_rec, LOCK_EX=2, LOCK_UN=8) \
            if g("fcntl", False) else None
        ns["msvcrt"] = types.SimpleNamespace(locking=_msvcrt_locking_rec, LK_LOCK=1, LK_UNLCK=0) \
            if g("msvcrt", False) else None
        return ns["_flock"](fd, g("lock", True)), None
    if func == "skill_file_lock":
        ns["fcntl"] = types.SimpleNamespace(flock=_fcntl_flock_rec, LOCK_EX=2, LOCK_UN=8) \
            if g("fcntl", False) else None
        ns["msvcrt"] = types.SimpleNamespace(locking=_msvcrt_locking_rec, LK_LOCK=1, LK_UNLCK=0) \
            if g("msvcrt", False) else None
        hl = types.SimpleNamespace()
        if g("has_paths", False):
            hl.paths = {PathDouble(p) for p in (g("held") or [])}
        ns["_held_locks"] = hl
        CUR["state"] = {"held": [], "has_paths": False}
        try:
            with ns["skill_file_lock"](g("lock_path", "")):
                seam("body", {})
            ret = None
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
        held = getattr(hl, "paths", set())
        CUR["state"] = {"held": sorted(str(p) for p in held), "has_paths": hasattr(hl, "paths")}
        return ret, CUR["state"]
    if func == "relocate":
        return ns["_relocate"](PathDouble(g("src", "")), PathDouble(g("dest", "")),
                               g("skill_name", ""), g("action", ""),
                               **(g("capture_kwargs") or {})), None
    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_module_stubs()
    cases = 0
    mism = 0
    per_func = {}
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
        per_func[func] = per_func.get(func, 0) + 1
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["state"] = None
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        try:
            ret, state = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = CUR["state"]
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE skill_usage_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE skill_usage_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()