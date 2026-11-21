#!/usr/bin/env python3
"""Leaf differential oracle for update_cmd_gaps.c wave 2 (7 re-ported bodies).

Reads the harness JSONL on stdin ({"func","in","out"}), recomputes the LIVE
hermes_cli reference for each case with identically scripted seams, and
compares the ordered seam trace + return value.  Prints:
    ORACLE update_cmd_gaps2: N cases, M mismatches.

Live canonical relaunch_stopped_serves = hermes_cli/_old_updater.py (the
old_updater_deps wrapper delegates to it and main.py freezes it via that
wrapper; update_cmd_windows._relaunch_stopped_serves has no caller).

Documented projections (identical on both sides):
  - list/dict arguments (the `token`) ride JSON text in the const char* C
    parameter; the harness echoes the fixture value and the oracle passes the
    live Python dict/list.
  - integral floats are normalized to int on BOTH sides before comparing.
  - void C signatures project their Python return away (ret == None); the
    ordered seam trace still proves the branch taken.
  - `_post_update_sqlite_runtime_status` returns (bool, info) in Python; the
    int C signature keeps only the bool.
  - the executable path passed to probe_sqlite_runtime and the fleet marker
    path are ambient: C calls the seam with {} / a fixed marker path, and the
    oracle pins the live helpers to those same values.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
import __future__
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from functools import partial
from pathlib import Path
from typing import Any, Callable, Optional

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE update_cmd_gaps2: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "update_cmd_gaps2_home"))
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

ANN_FLAG = __future__.annotations.compiler_flag
MARK_PATH = "/home/.hermes/fleet-restart-pending"


def _extract(rel, modname, seed):
    """AST-extract one quarry module into a registered module namespace."""
    path = QUARRY / rel
    src = path.read_text(encoding="utf-8")
    mod = types.ModuleType(modname)
    mod.__file__ = str(path)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seed)
    ns["__name__"] = modname
    ns["__builtins__"] = builtins.__dict__
    tree = ast.parse(src)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                     flags=ANN_FLAG), ns)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not isinstance(node.target, ast.Name):
                    continue
            else:
                continue
            try:
                exec_node(node)
            except Exception:
                pass

    seed_consts()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            try:
                exec_node(node)
            except Exception:
                pass
    seed_consts()
    return ns


_COMMON = dict(sys=sys, os=os, Path=Path, json=json,
               Callable=Callable, Optional=Optional, Any=Any,
               partial=partial, dataclass=dataclass, field=field,
               datetime=datetime, timedelta=timedelta, timezone=timezone,
               UTC=timezone.utc)

NS_F = _extract("hermes_cli/update_cmd_fleet.py", "hermes_cli.update_cmd_fleet", dict(_COMMON))
NS_S = _extract("hermes_cli/update_cmd_stash.py", "hermes_cli.update_cmd_stash", dict(_COMMON))
NS_M = _extract("hermes_cli/update_cmd_maint.py", "hermes_cli.update_cmd_maint", dict(_COMMON))
NS_O = _extract("hermes_cli/_old_updater.py", "hermes_cli._old_updater", dict(_COMMON))

_REQUIRED = {
    "NS_F": (NS_F, ["_service_unit_supports_graceful_sigusr1_restart",
                   "_warn_pending_fleet_restart", "_write_fleet_restart_pending_marker"]),
    "NS_S": (NS_S, ["_park_stashed_changes"]),
    "NS_M": (NS_M, ["_post_update_sqlite_runtime_status",
                    "_print_verified_update_completion"]),
    "NS_O": (NS_O, ["relaunch_stopped_serves"]),
}
for _label, (_ns, _names) in _REQUIRED.items():
    _missing = [n for n in _names if n not in _ns or _ns[n] is None]
    if _missing:
        print("ORACLE update_cmd_gaps2: extraction failed for %s: %s" % (_label, _missing),
              file=sys.stderr)
        sys.exit(2)

# ── stub modules for the in-function imports ──
_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = []
sys.modules["hermes_cli"] = _pkg


def _stub(name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    setattr(_pkg, name.split(".", 1)[1], m)
    return m


MOD_UC = _stub("hermes_cli.update_cmd")
MOD_SQL = _stub("hermes_cli.sqlite_runtime")
MOD_UHO = _stub("hermes_cli.update_host_obligation")

# ── scripted seam machinery (mirrors the revive oracle) ──
TRACE = []
NOSCRIPT = []
SCRIPTS = {}
CURSORS = {}
_MISSING = object()


class _NoScript(Exception):
    pass


def _mk_exc(kind, msg):
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


def call(name, args):
    entry = SCRIPTS.get(name, _MISSING)
    if entry is _MISSING:
        NOSCRIPT.append(name)
        TRACE.append({"ev": name, "args": copy.deepcopy(args), "ret": {"__noscript__": name}})
        raise _NoScript(name)
    if isinstance(entry, dict) and "__const__" in entry:
        val = entry["__const__"]
    elif isinstance(entry, list):
        idx = CURSORS.get(name, 0)
        CURSORS[name] = idx + 1
        val = entry[min(idx, len(entry) - 1)] if entry else None
    else:
        val = entry
    TRACE.append({"ev": name, "args": copy.deepcopy(args), "ret": val})
    if isinstance(val, dict) and "__raise__" in val:
        raise _mk_exc(val["__raise__"], val.get("msg", ""))
    return copy.deepcopy(val)


def s_print(*a, **k):
    TRACE.append({"ev": "print", "args": {"text": " ".join(str(x) for x in a)}, "ret": None})
    return None


def s_logger_debug(fmt, *a):
    TRACE.append({"ev": "logger.debug", "args": {"msg": fmt % a if a else fmt}, "ret": None})
    return None


_LOGGER = types.SimpleNamespace(debug=s_logger_debug)


# ── live dependency fakes → seams ──
def _fwd(path):
    """Normalise WindowsPath str() (backslashes) to the C fixture form."""
    return str(path).replace("\\", "/")


def s_probe(path):
    v = call("probe_sqlite_runtime", {})
    if isinstance(v, dict):
        return types.SimpleNamespace(**v)
    return v


def s_post_status():
    """hermes_cli.update_cmd._post_update_sqlite_runtime_status (faked at the
    boundary: print_verified_update_completion only branches on its result)."""
    v = call("_post_update_sqlite_runtime_status", {})
    if not isinstance(v, dict):
        return (False, None)
    info = v.get("info")
    if isinstance(info, dict):
        info = types.SimpleNamespace(**info)
    return (bool(v.get("ok", False)), info)


def s_print_completion(message):
    return call("_print_update_completion", {"message": message})


def s_partial_lines(sqlite_version):
    return call("_sqlite_partial_completion_lines", {"sqlite_version": sqlite_version})


def s_owns(path):
    return call("_pytest_owns_live_checkout", {"path": _fwd(path)})


def s_arm(path, *, expected_sha="", runtimes=None, profile="", owner=""):
    return call("arm_host_obligation", {
        "path": _fwd(path), "expected_sha": expected_sha, "runtimes": runtimes,
        "profile": profile, "owner": owner})


def s_profile():
    return call("current_profile_name", {})


def s_run_child(request):
    v = call("_run_child", request)
    if isinstance(v, dict):
        return (v.get("code", 1), v.get("completed") or {})
    return v


def s_record_disposition(outcome, stash_ref, detail="", *, chosen=False):
    return call("_record_stash_disposition", {
        "outcome": outcome, "stash_ref": stash_ref, "detail": detail,
        "chosen": chosen})


def install_case(case):
    global TRACE, NOSCRIPT, SCRIPTS, CURSORS
    TRACE = []
    NOSCRIPT = []
    SCRIPTS = case.get("io") or {}
    CURSORS = {}
    NS_F.update(print=s_print, logger=_LOGGER,
                _fleet_restart_pending_marker_path=lambda: Path(MARK_PATH),
                _current_profile_name=s_profile)
    NS_S.update(print=s_print, logger=_LOGGER,
                _record_stash_disposition=s_record_disposition)
    NS_M.update(print=s_print, logger=_LOGGER,
                _print_update_completion=s_print_completion,
                _sqlite_partial_completion_lines=s_partial_lines)
    NS_O.update(print=s_print, _run_child=s_run_child, sys=sys)
    MOD_UC._m = lambda: types.SimpleNamespace(_pytest_owns_live_checkout=s_owns)
    MOD_UC._post_update_sqlite_runtime_status = s_post_status
    MOD_SQL.probe_sqlite_runtime = s_probe
    MOD_UHO.arm_host_obligation = s_arm


VOID_FUNCS = {
    "warn_pending_fleet_restart", "park_stashed_changes",
    "write_fleet_restart_pending_marker", "relaunch_stopped_serves",
}


def run_live(func, case):
    args = case.get("args") or {}
    ctx = case.get("ctx") or {}
    if func == "service_unit_supports_graceful_sigusr1_restart":
        return NS_F["_service_unit_supports_graceful_sigusr1_restart"](args.get("unit"))
    if func == "warn_pending_fleet_restart":
        r = NS_F["_warn_pending_fleet_restart"](startup=bool(args.get("startup", False)))
        return None if func in VOID_FUNCS else r
    if func == "park_stashed_changes":
        NS_S["_park_stashed_changes"](args.get("stash_ref"))
        return None
    if func == "post_update_sqlite_runtime_status":
        ok, _info = NS_M["_post_update_sqlite_runtime_status"]()
        return bool(ok)
    if func == "print_verified_update_completion":
        return bool(NS_M["_print_verified_update_completion"](args.get("message")))
    if func == "write_fleet_restart_pending_marker":
        NS_F["_write_fleet_restart_pending_marker"](
            expected_sha=args.get("expected_sha", ""),
            runtimes=ctx.get("runtimes"), owner=ctx.get("owner", ""))
        return None
    if func == "relaunch_stopped_serves":
        NS_O["relaunch_stopped_serves"](copy.deepcopy(args.get("token")))
        return None
    raise KeyError("unknown func: %s" % func)


def compute_case(rec):
    func = rec.get("func", "")
    case = _norm_floats(rec.get("in") or {})
    install_case(case)
    try:
        try:
            ret = run_live(func, case)
        except _NoScript:
            ret = {"__noscript__": list(NOSCRIPT)}
        except BaseException as e:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    except Exception as e:
        ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return {"ret": ret, "trace": TRACE, "noscript": list(NOSCRIPT)}


def _norm_floats(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: _norm_floats(v) for k, v in x.items()}
    if isinstance(x, list):
        return [_norm_floats(v) for v in x]
    return x


def _canon(x):
    return _norm_floats(json.loads(json.dumps(x)))


def _first_diff(a, b, path="out"):
    if type(a) is not type(b) and not (isinstance(a, (int, float)) and isinstance(b, (int, float))):
        return "%s: type %s != %s" % (path, type(a).__name__, type(b).__name__)
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a:
                return "%s.%s: missing on C side" % (path, k)
            if k not in b:
                return "%s.%s: missing on PY side" % (path, k)
            d = _first_diff(a[k], b[k], "%s.%s" % (path, k))
            if d:
                return d
        return None
    if isinstance(a, list):
        if len(a) != len(b):
            return "%s: len %d != %d" % (path, len(a), len(b))
        for i, (x, y) in enumerate(zip(a, b)):
            d = _first_diff(x, y, "%s[%d]" % (path, i))
            if d:
                return d
        return None
    if a != b:
        return "%s: %r != %r" % (path, a, b)
    return None


def main():
    n = 0
    mismatches = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        n += 1
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print("MISMATCH case %d: harness line not JSON: %s" % (n, e))
            continue
        func = rec.get("func", "?")
        got = rec.get("out")
        if not isinstance(got, dict):
            mismatches += 1
            print("MISMATCH %s case %d: harness emitted no out object" % (func, n))
            continue
        try:
            want = compute_case(rec)
        except Exception as e:
            mismatches += 1
            print("MISMATCH %s case %d: oracle internal error: %r" % (func, n, e))
            continue
        g = _canon(got)
        w = _canon(want)
        if g.get("noscript") or w.get("noscript"):
            mismatches += 1
            print("MISMATCH %s case %d: unscripted seam C=%s PY=%s" %
                  (func, n, g.get("noscript"), w.get("noscript")))
            continue
        if g != w:
            mismatches += 1
            d = _first_diff(g, w) or "values differ"
            print("MISMATCH %s case %d: %s" % (func, n, d))
    print("ORACLE update_cmd_gaps2: %d cases, %d mismatches" % (n, mismatches))
    if n == 0:
        sys.exit(2)
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
