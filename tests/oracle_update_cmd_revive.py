#!/usr/bin/env python3
"""Differential oracle for the update_cmd revival wave 2 (update_cmd_gaps.c).

Reads the harness's JSONL on stdin ({"func","in","out"}), recomputes the LIVE
hermes_cli/update_cmd_{fleet,stash,windows,git}.py reference for each case with
identically scripted seams, and compares the ordered seam trace + return
value.  Prints: ORACLE update_cmd_revive: N cases, M mismatches.

Seam scripts (in.io): a single value is reused for every call; a bare array
is consumed per call (the last element repeats once exhausted);
{"__const__": v} reuses v verbatim (list-valued returns); a value carrying
{"__raise__": kind, "msg": ...} scripts an exception.  print / logger.debug
are ambient (recorded, never scripted).  Missing scripts are fail-closed.

Projections shared with the C port (documented in update_cmd_gaps.c):
  - list/dict arguments ride JSON text in the const char* C parameters; the
    oracle parses them back (the harness echoes the fixture value).
  - integral floats are normalized to int on BOTH sides before driving, so
    str()/repr() of JSON numbers agrees with libjson's int-like formatting.
  - .lower() is ASCII-only in C; fixtures stay ASCII.
  - run_git / git_run seam args record only argv/args/timeout and
    git_cmd/args/cwd; constant kwargs are dropped on both sides.
"""
import ast
import builtins
import contextlib
import copy
import json
import logging
import os
import re
import shlex
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
    print("ORACLE update_cmd_revive: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
# The runner scrubs the environment; the quarry import chain needs a home.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "update_cmd_revive_home"))
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

ANN_FLAG = __future__.annotations.compiler_flag


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
    seed_consts()   # constants that reference functions / partial aliases
    return ns


_COMMON = dict(contextlib=contextlib, contextmanager=contextlib.contextmanager,
               suppress=contextlib.suppress, logging=logging, json=json, os=os,
               sys=sys, re=re, shlex=shlex, Path=Path, Callable=Callable,
               Optional=Optional, Any=Any, partial=partial, dataclass=dataclass,
               field=field, datetime=datetime, timedelta=timedelta,
               timezone=timezone, UTC=timezone.utc,
               windows_hide_flags=lambda: 0)

NS_F = _extract("hermes_cli/update_cmd_fleet.py", "hermes_cli.update_cmd_fleet",
                dict(_COMMON))
NS_W = _extract("hermes_cli/update_cmd_windows.py", "hermes_cli.update_cmd_windows",
                dict(_COMMON))
NS_S = _extract("hermes_cli/update_cmd_stash.py", "hermes_cli.update_cmd_stash",
                dict(_COMMON))
NS_G = _extract("hermes_cli/update_cmd_git.py", "hermes_cli.update_cmd_git",
                dict(_COMMON))
NS_C = _extract("hermes_cli/update_cmd_common.py", "hermes_cli.update_cmd_common",
                dict(_COMMON))

# update_receipt.py is huge: extract ONLY row_is_external + EXTERNAL_STATE
# (targeted nodes, no whole-module side effects).
_rpath = QUARRY / "hermes_cli/update_receipt.py"
_rsrc = _rpath.read_text(encoding="utf-8")
NS_R = types.ModuleType("hermes_cli.update_receipt_extract")
NS_R.__dict__.update(_COMMON)
NS_R.__dict__["__builtins__"] = builtins.__dict__
sys.modules["hermes_cli.update_receipt_extract"] = NS_R
for _node in ast.parse(_rsrc).body:
    _name = getattr(_node, "name", None)
    if _name is None and isinstance(_node, ast.Assign):
        _name = _node.targets[0].id if isinstance(_node.targets[0], ast.Name) else None
    if _name in ("row_is_external", "EXTERNAL_STATE"):
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(_rpath), "exec",
                     flags=ANN_FLAG), NS_R.__dict__)

# Fail closed if the extraction missed anything the ports rely on.
_REQUIRED = {
    "NS_F": (NS_F, ["_receipt_looks_unfinished", "_receipt_reports_stale_runtime",
                    "_pending_fleet_restart_needed", "_restart_phase_failure_is_incomplete"]),
    "NS_W": (NS_W, ["_looks_like_desktop_control_plane", "_desktop_owns_gateway_lifecycle",
                    "_refuse_gateway_ancestor_tree_kill", "_try_call", "_BACKEND_PURPOSES"]),
    "NS_S": (NS_S, ["_warn_orphaned_update_autostashes", "_AUTOSTASH_WARN_AGE_DAYS",
                    "_AUTOSTASH_NAME_PREFIX"]),
    "NS_G": (NS_G, ["_git_is_trampoline", "_probe_fork_bomb", "_GIT_TEXT_KW"]),
    "NS_C": (NS_C, ["_best_effort"]),
    "NS_R": (NS_R.__dict__, ["row_is_external", "EXTERNAL_STATE"]),
}
for _label, (_ns, _names) in _REQUIRED.items():
    _missing = [n for n in _names if n not in _ns or _ns[n] is None]
    if _missing:
        print("ORACLE update_cmd_revive: extraction failed for %s: %s" % (_label, _missing),
              file=sys.stderr)
        sys.exit(2)
assert NS_W["_BACKEND_PURPOSES"] == ("serve", "dashboard")
assert NS_S["_AUTOSTASH_WARN_AGE_DAYS"] == 7
assert NS_S["_AUTOSTASH_NAME_PREFIX"] == "hermes-update-autostash-"
assert NS_R.__dict__["EXTERNAL_STATE"] == "external"

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
MOD_UR = _stub("hermes_cli.update_receipt")
MOD_USO = _stub("hermes_cli.update_serve_obligations")
MOD_UCG = _stub("hermes_cli.update_cmd_git")
MOD_CUST = _stub("hermes_cli.update_custody")
MOD_PI = _stub("hermes_cli.process_identity")
MOD_GW = _stub("hermes_cli.gateway")

# ── scripted seam machinery ──

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
        TRACE.append({"ev": name, "args": args, "ret": {"__noscript__": name}})
        raise _NoScript(name)
    if isinstance(entry, dict) and "__const__" in entry:
        val = entry["__const__"]
    elif isinstance(entry, list):
        idx = CURSORS.get(name, 0)
        CURSORS[name] = idx + 1
        val = entry[min(idx, len(entry) - 1)] if entry else None
    else:
        val = entry
    TRACE.append({"ev": name, "args": args, "ret": val})
    if isinstance(val, dict) and "__raise__" in val:
        raise _mk_exc(val["__raise__"], val.get("msg", ""))
    return copy.deepcopy(val)


def s_print(*a, **k):
    TRACE.append({"ev": "print", "args": {"text": " ".join(str(x) for x in a)},
                  "ret": None})
    return None


def s_logger_debug(fmt, *a):
    TRACE.append({"ev": "logger.debug", "args": {"msg": fmt % a if a else fmt},
                  "ret": None})
    return None


_LOGGER = types.SimpleNamespace(debug=s_logger_debug)


def s_current_checkout_sha():
    return call("current_checkout_sha", {})


def s_read_latest_receipt():
    return call("read_latest_receipt", {})


def s_retain(receipt):
    return call("retain_receipt_manual_serves", {"receipt": receipt})


def s_armed():
    return call("fleet_restart_obligation_armed", {})


def s_obsolete():
    return call("marker_only_restart_obsolete", {})


def s_owed(receipt, pending_manual):
    return call("receipt_owed_gateways",
                {"receipt": receipt, "pending_manual": pending_manual})


def s_covers(expected_sha, receipt, owed, *, accept_states=("current",)):
    return call("live_fleet_covers_receipt",
                {"expected_sha": expected_sha, "receipt": receipt, "owed": owed})


def s_git_run(git_cmd, args, cwd=None, *, check=False, env=None):
    v = call("git_run", {"git_cmd": git_cmd, "args": list(args),
                         "cwd": cwd.as_posix() if isinstance(cwd, Path) else cwd})
    return types.SimpleNamespace(**v) if isinstance(v, dict) else v


def s_run_git(argv, args, timeout=None, **kw):
    v = call("run_git", {"argv": argv, "args": list(args), "timeout": timeout})
    return types.SimpleNamespace(**v) if isinstance(v, dict) else v


def s_ledger():
    return call("ledger_entries", {})


def s_spawner_dead(e):
    return call("spawner_is_dead", {"entry": e})


def s_process(pid):
    v = call("psutil.Process", {"pid": pid})
    return types.SimpleNamespace(pid=pid, **(v if isinstance(v, dict) else {}))


def s_psutil():
    v = call("psutil", {})
    if v is None:
        return None
    return types.SimpleNamespace(Process=s_process)


def s_detect(**kw):
    return call("detect_venv_python_processes", {})


def s_parent_is_live(proc):
    return call("parent_is_live", {"pid": proc.pid})


def s_holder(cmdline):
    return call("holder_subcommand", {"cmdline": cmdline})


def s_ancestor(pid):
    return call("is_pid_ancestor_of_current_process", {"pid": pid})


class _DT(datetime):
    """datetime double: now() is scripted (epoch seconds); strptime stays real."""

    @classmethod
    def now(cls, tz=None):
        epoch = call("now_utc", {})
        return datetime.fromtimestamp(epoch, timezone.utc)


def install_case(case):
    global TRACE, NOSCRIPT, SCRIPTS, CURSORS
    TRACE = []
    NOSCRIPT = []
    SCRIPTS = case.get("io") or {}
    CURSORS = {}
    # fleet: seam the unported siblings; keep the two ported receipts real.
    NS_F.update(logger=_LOGGER,
                _fleet_restart_obligation_armed=s_armed,
                _marker_only_restart_obsolete=s_obsolete,
                _receipt_owed_gateways=s_owed,
                _live_fleet_covers_receipt=s_covers)
    # windows: _try_call / _best_effort / the classifier stay real.
    NS_W.update(print=s_print, logger=_LOGGER,
                _hermes_holder_subcommand=s_holder,
                _psutil=s_psutil,
                _detect_venv_python_processes=s_detect,
                _parent_is_live=s_parent_is_live,
                _best_effort=NS_C["_best_effort"])
    NS_C["logger"] = _LOGGER
    NS_S.update(print=s_print, logger=_LOGGER,
                datetime=_DT, timedelta=timedelta, UTC=timezone.utc)
    MOD_UC._current_checkout_sha = s_current_checkout_sha
    MOD_UC._m = lambda: types.SimpleNamespace()
    MOD_UR.read_latest_receipt = s_read_latest_receipt
    MOD_UR.row_is_external = NS_R.__dict__["row_is_external"]
    MOD_USO.retain_receipt_manual_serves = s_retain
    MOD_UCG._git_run = s_git_run
    MOD_CUST.run_git = s_run_git
    MOD_PI.ledger_entries = s_ledger
    MOD_PI.spawner_is_dead = s_spawner_dead
    MOD_GW._is_pid_ancestor_of_current_process = s_ancestor


def run_live(func, case):
    args = case.get("args") or {}
    ctx = case.get("ctx") or {}
    if func == "receipt_looks_unfinished":
        return NS_F["_receipt_looks_unfinished"](args.get("receipt"))
    if func == "receipt_reports_stale_runtime":
        return NS_F["_receipt_reports_stale_runtime"](ctx.get("receipt"),
                                                      args.get("expected_sha"))
    if func == "warn_orphaned_update_autostashes":
        return NS_S["_warn_orphaned_update_autostashes"](args.get("git_cmd"),
                                                         Path(args.get("cwd")))
    if func == "desktop_owns_gateway_lifecycle":
        return NS_W["_desktop_owns_gateway_lifecycle"]()
    if func == "pending_fleet_restart_needed":
        kw = {}
        if ctx.get("receipt_kw") is not None:
            kw["receipt"] = ctx["receipt_kw"]
        if ctx.get("pending_manual_kw") is not None:
            kw["pending_manual"] = ctx["pending_manual_kw"]
        return NS_F["_pending_fleet_restart_needed"](**kw)
    if func == "refuse_gateway_ancestor_tree_kill":
        return NS_W["_refuse_gateway_ancestor_tree_kill"](
            args.get("gateway_pids"), gateway_mode=ctx.get("gateway_mode", False))
    if func == "restart_phase_failure_is_incomplete":
        return NS_F["_restart_phase_failure_is_incomplete"](
            ctx.get("surviving"), ctx.get("pre_restart_pids"))
    if func == "looks_like_desktop_control_plane":
        return NS_W["_looks_like_desktop_control_plane"](args.get("cmdline"))
    if func == "git_is_trampoline":
        return NS_G["_git_is_trampoline"](args.get("git_cmd"))
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
        except BaseException as e:   # SystemExit included — fail-closed
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    except Exception as e:
        ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return {"ret": ret, "trace": TRACE, "noscript": list(NOSCRIPT)}


def _norm_floats(x):
    """Integral floats compare as ints (libjson serializes them int-like)."""
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
    print("ORACLE update_cmd_revive: %d cases, %d mismatches" % (n, mismatches))
    if n == 0:
        sys.exit(2)   # fail-closed: an empty run is not a pass
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
