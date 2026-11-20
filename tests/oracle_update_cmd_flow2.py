#!/usr/bin/env python3
"""Oracle for update_cmd_flow2 — the REMAINING UNCLAIMED update-pipeline FLOW
cores of hermes_cli/update_cmd.py (LIVE authority, py3.14 quarry venv).

Ports covered (19): _source_completion_request, _complete_source_update,
_reconcile_diverged_checkout, _rollback_if_pulled_syntax_error,
_roll_back_broken_update, _move_checkout_to, _pull_updates,
_switch_branch_at_commit_point, _refuse_before_commit_point,
_exit_after_failed_branch_switch, _apply_parked_branch_guard,
_prepare_checkout_for_update, _resolve_update_options,
_begin_update_receipt_and_plan, _record_update_initiator, _prepare_git_command,
_handle_update_called_process_error, _finish_already_up_to_date,
_apply_pulled_update.

CLAIMED siblings (ported by update_cmd_flow) are SEAMED; UNCLAIMED ported
siblings compose for real. _best_effort (update_cmd_common) is AST-extracted and
run live (its logger.debug is silent). _checkout_move (update_cmd_pause_gate) is
a SEAMED enter/exit CM pair (its pause-record writes are opaque). In-function
imports are SILENT (stub modules pre-installed in sys.modules).

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the quarry,
module constants seeded, every effect replaced by a recording stub returning the
SAME scripted results the C harness feeds its universal seam. Compares the
ordered DECISION SEQUENCE (trace) plus the return value (and the mutated request
window for the three mutation flows). Scripted {"__raise__": cls} raises a REAL
exception; drive() catches BaseException so KeyboardInterrupt/SystemExit pass
through live except-Exception clauses exactly as the C port propagates them. A
MISSING seam key fail-closes (RuntimeError) on both sides; a scripted null is a
valid None.

Input: stdin JSON lines from t_port_update_cmd_flow2:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...][, "state":...]}}
Summary: ORACLE update_cmd_flow2: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import contextlib
import copy
import dataclasses
import datetime as _datetime
import json
import os
import subprocess
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "update_cmd_flow2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "update_cmd.py")
SRC_COMMON = os.path.join(QUARRY, "hermes_cli", "update_cmd_common.py")

PY = {
    "source_completion_request": "_source_completion_request",
    "complete_source_update": "_complete_source_update",
    "reconcile_diverged_checkout": "_reconcile_diverged_checkout",
    "rollback_if_pulled_syntax_error": "_rollback_if_pulled_syntax_error",
    "roll_back_broken_update": "_roll_back_broken_update",
    "move_checkout_to": "_move_checkout_to",
    "pull_updates": "_pull_updates",
    "switch_branch_at_commit_point": "_switch_branch_at_commit_point",
    "refuse_before_commit_point": "_refuse_before_commit_point",
    "exit_after_failed_branch_switch": "_exit_after_failed_branch_switch",
    "apply_parked_branch_guard": "_apply_parked_branch_guard",
    "prepare_checkout_for_update": "_prepare_checkout_for_update",
    "resolve_update_options": "_resolve_update_options",
    "begin_update_receipt_and_plan": "_begin_update_receipt_and_plan",
    "record_update_initiator": "_record_update_initiator",
    "prepare_git_command": "_prepare_git_command",
    "handle_update_called_process_error": "_handle_update_called_process_error",
    "finish_already_up_to_date": "_finish_already_up_to_date",
    "apply_pulled_update": "_apply_pulled_update",
}
WANTED = set(PY.values())
STATEFUL = {"complete_source_update", "finish_already_up_to_date", "apply_pulled_update"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "statewin": None}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    args = copy.deepcopy(args)
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


class UpstreamTargetBroken(Exception):
    def __init__(self, sha, path, error):
        super().__init__(f"upstream/main ({sha[:10]}) has a syntax error in {path}")
        self.sha, self.path, self.error = sha, path, error


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
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError, "SystemError": SystemError,
    "BufferError": BufferError, "EOFError": EOFError, "Exception": Exception,
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ResourceWarning": ResourceWarning,
    "SubprocessError": subprocess.SubprocessError,
    "UpstreamTargetBroken": UpstreamTargetBroken,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        if name == "TimeoutExpired":
            raise subprocess.TimeoutExpired(r.get("cmd"), r.get("timeout", 300),
                                            output=r.get("output"), stderr=r.get("stderr"))
        if name == "CalledProcessError":
            raise subprocess.CalledProcessError(r.get("returncode", 1), r.get("cmd"),
                                                output=r.get("output"), stderr=r.get("stderr"))
        if name == "UpstreamTargetBroken":
            raise UpstreamTargetBroken(r.get("sha", ""), r.get("path", ""), r.get("error", ""))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


NS = {}


def posix(p):
    if p is None:
        return None
    if isinstance(p, PathDouble):
        return p.posix
    if isinstance(p, Path):
        return p.as_posix()
    return p


class PathDouble:
    def __init__(self, s):
        self.posix = s if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix

    def resolve(self):
        return PathDouble(seam("path_resolve", {"path": self.posix}))

    def __truediv__(self, name):
        return PathDouble(self.posix.rstrip("/") + "/" + name)

    def exists(self):
        return seam("path_exists", {"path": self.posix})


class MarkerDouble:
    def __init__(self, path):
        self.path = path

    def unlink(self, missing_ok=False):
        return seam("marker_unlink", {"path": self.path})

    def is_file(self):
        return seam("marker_is_file", {"path": self.path})

    def read_text(self, encoding=None):
        return seam("marker_read_text", {"path": self.path, "encoding": encoding})


class InputFnDouble:
    __uc_marker__ = True

    def __call__(self, *a, **k):
        raise RuntimeError("gw_input_fn called unexpectedly")


def proj_fn(fn):
    if callable(fn):
        return {"__callable__": getattr(fn, "__uc_marker__", True)}
    return None


class TaggedFn:
    def __init__(self, marker, seam_name, argfn):
        self.__uc_marker__ = marker
        self._seam = seam_name
        self._argfn = argfn

    def __call__(self, *a, **k):
        return seam(self._seam, self._argfn(*a, **k))


class CurrentDouble:
    def get(self):
        r = seam("receipt_current_get", {})
        if isinstance(r, dict):
            return types.SimpleNamespace(**r)
        return r

    def reset(self, token):
        return seam("receipt_current_reset", {"token": token})


class PlanDouble:
    def to_dict(self):
        return seam("plan_to_dict", {})


class TimeDouble:
    def strftime(self, fmt):
        return seam("time_strftime", {"fmt": fmt})


class DateTimeDouble:
    @classmethod
    def now(cls, tz=None):
        return cls()

    def strftime(self, fmt):
        return seam("utc_now_strftime", {"fmt": fmt})


class CheckoutMoveCM:
    def __init__(self, token, targets, paths, revert):
        self.token = token
        self.targets = list(targets)
        self.paths = list(paths)
        self.revert = revert

    def __enter__(self):
        seam("checkout_move.enter", {"token": self.token, "targets": self.targets,
                                     "paths": self.paths, "revert": self.revert})
        return {}

    def __exit__(self, et, ev, tb):
        exc = {"kind": et.__name__, "msg": str(ev)} if et is not None else None
        seam("checkout_move.exit", {"token": self.token, "targets": self.targets,
                                    "paths": self.paths, "revert": self.revert, "exc": exc})
        return False


def checkout_move_double(token, *targets, paths=(), revert=False):
    return CheckoutMoveCM(token, targets, paths, revert)


class MainDouble:
    def __init__(self):
        self._resume_windows_gateways_after_update = TaggedFn(
            "m._resume_windows_gateways_after_update",
            "resume_windows_gateways_after_update", lambda token: {"token": token})
        self._sync_with_upstream_if_needed = lambda git_cmd, cwd, *, assume_yes=False, input_fn=None, checkout_move=None: seam(
            "sync_with_upstream_if_needed", {"git_cmd": git_cmd, "root": posix(cwd),
                                             "assume_yes": assume_yes, "input_fn": proj_fn(input_fn),
                                             "checkout_move": checkout_move})
        self._discard_stashed_changes = lambda git_cmd, cwd, ref: seam(
            "discard_stashed_changes", {"git_cmd": git_cmd, "root": posix(cwd), "ref": ref})
        self._park_stashed_changes = lambda ref: seam("park_stashed_changes", {"ref": ref})
        self._restore_stashed_changes = lambda git_cmd, cwd, ref, *, prompt_user=False, input_fn=None, checkout_move=None: seam(
            "restore_stashed_changes", {"git_cmd": git_cmd, "root": posix(cwd), "ref": ref,
                                        "prompt_user": prompt_user, "input_fn": proj_fn(input_fn),
                                        "checkout_move": checkout_move})
        self._stash_local_changes_if_needed = lambda git_cmd, cwd, *, checkout_move=None, ask_first=False, untracked_targets=(): seam(
            "stash_local_changes_if_needed", {"git_cmd": git_cmd, "root": posix(cwd),
                                              "checkout_move": checkout_move, "ask_first": ask_first,
                                              "untracked_targets": list(untracked_targets)})
        self._assess_parked_branch_switch = lambda git_cmd, cwd, current_branch, branch: _mat_pair(seam(
            "assess_parked_branch_switch", {"git_cmd": git_cmd, "root": posix(cwd),
                                            "current_branch": current_branch, "branch": branch}))
        self._print_parked_branch_skip_warning = lambda git_cmd, cwd, current_branch, branch, reason: seam(
            "print_parked_branch_skip_warning", {"git_cmd": git_cmd, "root": posix(cwd),
                                                 "current_branch": current_branch, "branch": branch, "reason": reason})
        self._print_parked_branch_kept_notice = lambda current_branch, branch, detail: seam(
            "print_parked_branch_kept_notice", {"current_branch": current_branch, "branch": branch, "detail": detail})
        self._get_origin_url = lambda git_cmd, cwd: seam("get_origin_url", {"git_cmd": git_cmd, "root": posix(cwd)})
        self._is_windows = lambda: seam("is_windows", {})

    @property
    def PROJECT_ROOT(self):
        return PathDouble(seam("project_root", {}))


def _mat_pair(r):
    if isinstance(r, list):
        return tuple(r)
    return r


class CompletionConfigDouble:
    @property
    def _LAST_SIBLING_SNAPSHOTS(self):
        return seam("last_sibling_snapshots", {})


class SysDouble:
    def __init__(self, platform):
        self.platform = platform
        self.stdin = types.SimpleNamespace(isatty=lambda: seam("stdin_isatty", {}))
        self.stdout = types.SimpleNamespace(isatty=lambda: seam("stdout_isatty", {}))

    def exit(self, code=0):
        raise SystemExit(code)


class OsDouble:
    def getpid(self):
        return seam("getpid", {})


def e_proj(e):
    return {"returncode": e.returncode, "cmd": e.cmd, "output": e.output, "stderr": e.stderr}


def broken_proj(b):
    return {"sha": b.sha, "path": b.path, "error": b.error}


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return o.posix
    if isinstance(o, MarkerDouble):
        return o.path
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, subprocess.CompletedProcess):
        return {"args": norm(o.args), "returncode": o.returncode,
                "stdout": norm(o.stdout), "stderr": norm(o.stderr)}
    if dataclasses.is_dataclass(o) and not isinstance(o, type):
        return {f.name: norm(getattr(o, f.name)) for f in dataclasses.fields(o)}
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if callable(o):
        return {"__callable__": getattr(o, "__uc_marker__", True)}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


# ── extraction ──

def extract():
    import logging
    import shlex
    from dataclasses import dataclass
    from typing import NoReturn

    src = open(SRC, encoding="utf-8").read()
    src_common = open(SRC_COMMON, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.update_cmd_extracted2",
        "json": json, "os": os, "sys": sys, "subprocess": subprocess,
        "shlex": shlex, "_time": TimeDouble(), "logging": logging,
        "Path": Path, "dataclass": dataclass, "NoReturn": NoReturn,
        "suppress": contextlib.suppress, "contextmanager": contextlib.contextmanager,
        "logger": RecLogger(), "UTC": _datetime.UTC,
        "_ORPHAN_RESCUE_REF_MAX_AGE_DAYS": 30,
    }
    tree = ast.parse(src)
    const_nodes = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            const_nodes.append(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            const_nodes.append(node)
    for node in const_nodes:
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    ns["logger"] = RecLogger()
    ns["_time"] = TimeDouble()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.ClassDef) and node.name in ("_CheckoutPlan", "_UpdateOptions"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    common_tree = ast.parse(src_common)
    for node in common_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_best_effort":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_COMMON, "exec"), ns)
    ns["logger"] = RecLogger()
    missing = (WANTED | {"_best_effort", "_CheckoutPlan", "_UpdateOptions"}) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("_UPDATE_CRITICAL_FILES", "_ARM_REFUSED", "_REF_MOVED", "NETWORK_GIT_TIMEOUT_SECONDS"):
        if k not in ns:
            print("FATAL missing constant: %s" % k)
            sys.exit(2)
    global NS
    NS = ns
    return ns


# ── recording stubs ──

def stub_mod(full, **attrs):
    m = types.ModuleType(full)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[full] = m
    parts = full.split(".")
    if len(parts) > 1:
        parent = sys.modules.get(".".join(parts[:-1]))
        if parent is not None:
            setattr(parent, parts[-1], m)
    return m


def git_run_stub(git_cmd, args, cwd=None, *, check=False, network=False):
    r = seam("git_run", {"git_cmd": git_cmd, "args": args, "cwd": posix(cwd),
                         "check": check, "network": network})
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def print_stub(*a, file=None, **k):
    text = a[0] if a else ""
    seam("print", {"text": text})
    return None


def install_stubs(ns):
    # claimed siblings (update_cmd_flow ports) — SEAMED
    ns["_git_run"] = git_run_stub
    ns["_capture_head_sha"] = lambda git_cmd, cwd: seam("capture_head_sha", {"git_cmd": git_cmd, "cwd": posix(cwd)})
    ns["_update_movement_baseline"] = lambda git_cmd, pre_pull_sha, pre_sync_sha, rollback_branch, target_sha: seam(
        "update_movement_baseline", {"git_cmd": git_cmd, "pre_pull_sha": pre_pull_sha,
                                     "pre_sync_sha": pre_sync_sha, "rollback_branch": rollback_branch,
                                     "target_sha": target_sha})
    ns["_refuse_unselected_head"] = lambda git_cmd, target_sha, unmoved: seam(
        "refuse_unselected_head", {"git_cmd": git_cmd, "target_sha": target_sha, "unmoved": unmoved})
    ns["_verify_head_after_pull"] = lambda git_cmd, branch, pre_pull_sha, *, in_place_update, _windows_gateway_resume: seam(
        "verify_head_after_pull", {"git_cmd": git_cmd, "branch": branch, "pre_pull_sha": pre_pull_sha,
                                   "in_place_update": in_place_update, "token": _windows_gateway_resume})
    ns["_landed_off_target"] = lambda git_cmd, target_sha, *, merged=True: seam(
        "landed_off_target", {"git_cmd": git_cmd, "target_sha": target_sha, "merged": merged})
    ns["_missing_branch_stop"] = lambda git_cmd, branch: seam("missing_branch_stop", {"git_cmd": git_cmd, "branch": branch})
    ns["_resolved_commit"] = lambda git_cmd, ref: seam("resolved_commit", {"git_cmd": git_cmd, "ref": ref})
    ns["_is_shallow_checkout"] = lambda git_cmd: seam("is_shallow_checkout", {"git_cmd": git_cmd})
    ns["_tip_shas"] = lambda git_cmd, target_ref, base="HEAD": _mat_pair(seam(
        "tip_shas", {"git_cmd": git_cmd, "target_ref": target_ref, "base": base}))
    ns["_base_git_cmd"] = lambda: seam("base_git_cmd", {})
    ns["_map_ssl_cert_file_for_git"] = lambda git_cmd: seam("map_ssl_cert_file_for_git", {"git_cmd": git_cmd})
    ns["_upstream_broken_header"] = lambda broken: seam("upstream_broken_header", {"broken": broken_proj(broken)})
    ns["_updates_config"] = lambda: seam("updates_config", {})
    ns["_finalize_receipt"] = lambda status, debug_message: seam(
        "finalize_receipt", {"status": status, "debug_message": debug_message})

    def settle_stub(request):
        r = seam("settle_windows_resume", {"request": request})
        if isinstance(request, dict):
            request["windows_resume_settled"] = True
        return r
    ns["_settle_windows_resume"] = settle_stub

    # foreign bare globals
    ns["print"] = print_stub
    ns["_m"] = lambda: MainDouble()
    ns["_record_stop"] = lambda reason, *, without_receipt=None: seam(
        "record_stop", {"reason": reason, "without_receipt": without_receipt})
    ns["_unrestored_autostash_notice"] = lambda: seam("unrestored_autostash_notice", {})
    ns["stop_for_relaunch"] = lambda incomplete=False: seam("stop_for_relaunch", {"incomplete": incomplete})
    ns["get_hermes_home"] = lambda: seam("get_hermes_home", {})
    ns["_current_checkout_sha"] = lambda: seam("current_checkout_sha", {})
    ns["_write_fleet_restart_pending_marker"] = lambda expected_sha="": seam(
        "write_fleet_restart_pending_marker", {"expected_sha": expected_sha})
    ns["run_completion"] = lambda request: seam("run_completion", {"request": request})
    ns["_accept_completion_pm_receipt"] = lambda pm_receipt, update_id: seam(
        "accept_completion_pm_receipt", {"pm_receipt": pm_receipt, "update_id": update_id})
    ns["adopt_retired_channel"] = lambda request: seam("adopt_retired_channel", {"request": request})
    ns["_completion_config"] = CompletionConfigDouble()
    ns["_completion_receipt"] = types.SimpleNamespace(
        _current=CurrentDouble(),
        begin_update_receipt=lambda **k: seam("begin_update_receipt", {}),
        record_stage=lambda name, outcome, **facts: seam("record_stage", {"stage": name, "status": outcome, **facts}),
        finalize_interrupted_update_receipt=lambda stop_reason, *, exit_code=130: seam(
            "finalize_interrupted_update_receipt", {"message": stop_reason}),
        adopt_terminal_receipt=lambda data: seam("adopt_terminal_receipt", {"receipt": data}),
        record_fact=lambda key, value: seam("record_fact", {"key": key, "value": value}),
        git_error_stop_class=lambda exc: seam("git_error_stop_class", {"e": e_proj(exc)}))
    ns["_commit"] = types.SimpleNamespace(
        tree_syntax_error=lambda root, relpaths: _mat_pair(seam(
            "tree_syntax_error", {"root": posix(root), "critical_files": list(relpaths)})),
        arm_tree_move=lambda git_cmd, root, *, pre, target, stash, rollback: seam(
            "arm_tree_move", {"git_cmd": git_cmd, "root": posix(root), "pre": pre,
                              "target": target, "stash": stash, "rollback": rollback}),
        files_added_by=lambda git_cmd, root, pre, target: seam(
            "files_added_by", {"git_cmd": git_cmd, "root": posix(root), "pre": pre, "target": target}),
        drop_added_files=lambda root, added: seam("drop_added_files", {"root": posix(root), "added": added}),
        tree_whole_at=lambda git_cmd, root, sha: seam("tree_whole_at", {"git_cmd": git_cmd, "root": posix(root), "sha": sha}),
        settle_failed_tree_move=lambda root: seam("settle_failed_tree_move", {"root": posix(root)}),
        disarm_commit_obligations=lambda: seam("disarm_commit_obligations", {}),
        owe_restore_of=lambda root, pre: seam("owe_restore_of", {"root": posix(root), "sha": pre}),
        arm_commit_point=lambda git_cmd, root, expected_sha, **move: seam(
            "arm_commit_point", {"git_cmd": git_cmd, "root": posix(root), "target_sha": expected_sha,
                                 "pre": move.get("pre"), "target": move.get("target"), "stash": move.get("stash")}),
        preflight_refusal=lambda git_cmd, root, target_ref, critical_files: seam(
            "preflight_refusal", {"git_cmd": git_cmd, "root": posix(root), "target_ref": target_ref,
                                  "critical_files": list(critical_files)}))
    ns["_posix_pause"] = types.SimpleNamespace(pause_at_commit_point=lambda: seam("pause_at_commit_point", {}))
    ns["_checkout_move"] = checkout_move_double
    ns["_moves_for"] = lambda token: seam("moves_for", {"token": token})
    ns["interrupted_pull_marker"] = lambda root: MarkerDouble(seam("interrupted_pull_marker", {"root": posix(root)}))
    ns["is_object_id"] = lambda value: seam("is_object_id", {"value": value})
    ns["_prune_orphan_rescue_refs"] = lambda git_cmd, root, branch: seam(
        "prune_orphan_rescue_refs", {"git_cmd": git_cmd, "root": posix(root), "branch": branch})
    ns["_park_detached_head"] = lambda git_cmd, root, branch: seam(
        "park_detached_head", {"git_cmd": git_cmd, "root": posix(root), "branch": branch})
    ns["_branch_head_suffix"] = lambda git_cmd, root: seam("branch_head_suffix", {"git_cmd": git_cmd, "root": posix(root)})
    ns["_target_would_move_head"] = lambda git_cmd, root, target_ref: seam(
        "target_would_move_head", {"git_cmd": git_cmd, "root": posix(root), "target_ref": target_ref})
    ns["_count_commits_between"] = lambda git_cmd, root, pre, post: seam(
        "count_commits_between", {"git_cmd": git_cmd, "root": posix(root), "pre_sha": pre, "post_sha": post})
    ns["_checkout_version"] = lambda: seam("checkout_version", {})
    ns["_ensure_non_trampoline_git"] = lambda git_cmd: seam("ensure_non_trampoline_git", {"git_cmd": git_cmd})
    ns["_enclosing_repo_of_checkout"] = lambda git_cmd, root: seam(
        "enclosing_repo_of_checkout", {"git_cmd": git_cmd, "root": posix(root)})
    ns["_damaged_checkout_message"] = lambda root: seam("damaged_checkout_message", {"root": posix(root)})
    ns["_discard_lockfile_churn"] = lambda git_cmd, root, *, checkout_move=None: seam(
        "discard_lockfile_churn", {"git_cmd": git_cmd, "root": posix(root), "checkout_move": checkout_move})
    ns["_normalize_managed_eol"] = lambda git_cmd, root, *, checkout_move=None: seam(
        "normalize_managed_eol", {"git_cmd": git_cmd, "root": posix(root), "checkout_move": checkout_move})
    ns["_is_fork"] = lambda origin_url: seam("is_fork", {"origin_url": origin_url})
    ns["_format_update_failure_stage"] = lambda e: seam("format_update_failure_stage", {"e": e_proj(e)})
    ns["_should_zip_fallback_on_update_error"] = lambda e: seam("should_zip_fallback_on_update_error", {"e": e_proj(e)})
    ns["_called_process_error_is_python_dep_install"] = lambda e: seam("called_process_error_is_python_dep_install", {"e": e_proj(e)})
    ns["_print_called_process_error_tail"] = lambda e: seam("print_called_process_error_tail", {"e": e_proj(e)})
    ns["_update_via_zip"] = lambda args, *, had_desktop_app_before_update=False, target_sha=None, completion_request=None, **kw: seam(
        "update_via_zip", {"args": vars(args) if isinstance(args, types.SimpleNamespace) else args,
                           "had_desktop_app_before_update": had_desktop_app_before_update,
                           "target_sha": target_sha, "completion_request": completion_request, **kw})
    ns["_clear_pending_autostash"] = lambda: seam("clear_pending_autostash", {})
    ns["_push_synced_fork"] = lambda git_cmd, root: seam("push_synced_fork", {"git_cmd": git_cmd, "root": posix(root)})
    ns["UpstreamTargetBroken"] = UpstreamTargetBroken

    # stub packages for in-function imports (every intermediate registered)
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = []
    sys.modules["hermes_cli"] = pkg
    stub_mod("hermes_cli.update_receipt",
             begin_update_receipt=lambda **k: seam("begin_update_receipt", {}),
             owe_followup=lambda update_id, kind, message, retry=None: seam(
                 "owe_followup", {"update_id": update_id, "kind": kind, "message": message, "retry": retry}),
             git_output_stop_class=lambda detail, cmds, returncode: seam(
                 "git_output_stop_class", {"detail": detail, "cmds": cmds, "returncode": returncode}))
    stub_mod("hermes_cli.update_stage", publish_stage=lambda stage: seam("publish_stage", {"stage": stage}))
    stub_mod("hermes_cli.update_completion", settle_lost_completion=lambda request, message: seam(
        "settle_lost_completion", {"request": request, "message": message}))
    stub_mod("hermes_cli.update_inventory",
             collect_runtime_inventory=lambda: _mat_plan_rec(seam("collect_runtime_inventory", {})),
             record_plan_in_receipt=lambda plan: seam("record_plan_in_receipt", {"plan": _plan_proj(plan)}))
    stub_mod("hermes_cli.update_lock", update_marker_path=lambda: MarkerDouble(seam("update_marker_path", {})))
    stub_mod("hermes_cli.update_downgrade_guard", downgrade_refusal=lambda git_cmd, root, target_ref: seam(
        "downgrade_refusal", {"git_cmd": git_cmd, "root": posix(root), "target_ref": target_ref}))
    stub_mod("hermes_cli._subprocess_compat", expose_pm_git=lambda root: seam("expose_pm_git", {"root": posix(root)}))
    stub_mod("hermes_cli.source_check", _github_compare_behind=lambda base_sha, target_sha: seam(
        "github_compare_behind", {"base_sha": base_sha, "target_sha": target_sha}))
    dt = types.ModuleType("datetime")
    dt.datetime = DateTimeDouble
    dt.timezone = _datetime.timezone
    dt.UTC = _datetime.UTC
    sys.modules["datetime"] = dt


def _mat_plan_rec(r):
    if isinstance(r, dict):
        ns = types.SimpleNamespace(**r)
        if isinstance(getattr(ns, "runtimes", None), list):
            ns.runtimes = [types.SimpleNamespace(**x) if isinstance(x, dict) else x for x in ns.runtimes]
        return ns
    return r


def _plan_proj(plan):
    if isinstance(plan, types.SimpleNamespace):
        d = dict(vars(plan))
        if isinstance(d.get("runtimes"), list):
            d["runtimes"] = [vars(x) if isinstance(x, types.SimpleNamespace) else x for x in d["runtimes"]]
        return d
    return plan


# ── materialization ──

def mat_obj(d):
    return types.SimpleNamespace(**d) if isinstance(d, dict) else d


def mat_plan(d):
    return PlanDouble() if isinstance(d, dict) else d


def mat_fn(marker):
    return InputFnDouble() if marker else None


def mat_track(d):
    if not isinstance(d, dict):
        return d
    ns = types.SimpleNamespace(**d)
    if getattr(ns, "args", None) == NS["_ARM_REFUSED"]:
        ns.args = NS["_ARM_REFUSED"]
    elif getattr(ns, "args", None) == NS["_REF_MOVED"]:
        ns.args = NS["_REF_MOVED"]
    return ns


def mat_e(d):
    if isinstance(d, dict):
        return subprocess.CalledProcessError(d.get("returncode", 1), d.get("cmd"),
                                             output=d.get("output"), stderr=d.get("stderr"))
    return d


# ── drive ──

def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    name = PY[func]
    f = ns[name]
    ns["os"] = OsDouble()
    ns["sys"] = SysDouble(inp.get("platform", "linux"))
    git_cmd = inp.get("git_cmd")
    g = inp.get

    if func == "source_completion_request":
        return norm(f(mat_obj(g("opts")), mat_plan(g("plan")), g("snapshot_id"),
                      g("windows_resume"), g("desktop", False), g("gateway_mode", False)))
    if func == "complete_source_update":
        req = copy.deepcopy(g("request"))
        CUR["statewin"] = req
        f(req)
        return None
    if func == "reconcile_diverged_checkout":
        f(git_cmd, g("branch", ""), g("pre_pull_sha"), target_ref=g("target_ref"))
        return None
    if func == "rollback_if_pulled_syntax_error":
        f(git_cmd, g("pre_pull_sha"), rollback_branch=g("rollback_branch"), _windows_gateway_resume=g("token"))
        return None
    if func == "roll_back_broken_update":
        f(git_cmd, g("pre_pull_sha"), g("failing_path", ""), g("syntax_error"),
          rollback_branch=g("rollback_branch"),
          header=g("header", "\u2717 Pulled code has a syntax error in a critical file:"),
          _windows_gateway_resume=g("token"), stop_class=g("stop_class", "syntax_rollback"))
        return None
    if func == "move_checkout_to":
        f(git_cmd, g("branch", ""), g("merge_ref", ""), g("target_sha", ""), g("pre_pull_sha"))
        return None
    if func == "pull_updates":
        return f(git_cmd, g("branch", ""), g("auto_stash_ref"),
                 prompt_for_restore=g("prompt_for_restore", False), gw_input_fn=mat_fn(g("gw_input_fn")),
                 discard_local_changes=g("discard_local_changes", False), keep_stash=g("keep_stash", False),
                 target_ref=g("target_ref"), pre_sync_sha=g("pre_sync_sha"), rollback_branch=g("rollback_branch"),
                 sync_upstream=g("sync_upstream", False), assume_yes=g("assume_yes", False),
                 in_place_update=g("in_place_update", False), _windows_gateway_resume=g("token"),
                 expected_sha=g("expected_sha"))
    if func == "switch_branch_at_commit_point":
        return norm(f(git_cmd, g("branch", ""), g("target_ref", ""), pre=g("pre"), stash=g("stash")))
    if func == "refuse_before_commit_point":
        f(git_cmd, g("target_ref", ""), g("token"))
        return None
    if func == "exit_after_failed_branch_switch":
        f(git_cmd, g("branch", ""), mat_track(g("track_result")), g("auto_stash_ref"),
          mat_fn(g("gw_input_fn")), g("token"))
        return None
    if func == "apply_parked_branch_guard":
        return norm(f(git_cmd, g("branch", ""), g("current_branch", ""),
                      switch_branch=g("switch_branch", False), _windows_gateway_resume=g("token")))
    if func == "prepare_checkout_for_update":
        return norm(f(git_cmd, g("branch", ""), g("current_branch", ""), is_fork=g("is_fork", False),
                      assume_yes=g("assume_yes", False), gateway_mode=g("gateway_mode", False),
                      gw_input_fn=mat_fn(g("gw_input_fn")), switch_branch=g("switch_branch", False),
                      target_ref=g("target_ref"), _windows_gateway_resume=g("token")))
    if func == "resolve_update_options":
        return norm(f(mat_obj(g("args")), g("gateway_mode", False)))
    if func == "begin_update_receipt_and_plan":
        return norm(f(inp.get("args")))
    if func == "record_update_initiator":
        f()
        return None
    if func == "prepare_git_command":
        return norm(f(checkout_move=g("checkout_move")))
    if func == "handle_update_called_process_error":
        f(mat_e(g("e")), mat_obj(g("args")), g("gateway_mode", False), g("had_desktop_app_before_update", False),
          target_sha=g("target_sha"), target_repository=g("target_repository"), completion_request=g("completion_request"))
        return None
    if func == "finish_already_up_to_date":
        req = copy.deepcopy(g("completion_request"))
        CUR["statewin"] = req
        f(git_cmd, g("branch", ""), g("current_branch", ""), mat_obj(g("plan")),
          gw_input_fn=mat_fn(g("gw_input_fn")), completion_request=req,
          _windows_gateway_resume=g("token"), held_message=g("held_message"))
        return None
    if func == "apply_pulled_update":
        req = copy.deepcopy(g("completion_request"))
        CUR["statewin"] = req
        f(git_cmd, g("branch", ""), g("movement_baseline"), mat_obj(g("plan")),
          _windows_gateway_resume=g("token"), completion_request=req)
        return None
    raise ValueError("unhandled func: %s" % func)


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


SEAM_NAMES = [
    "print", "record_stop", "project_root", "path_resolve", "path_exists", "get_hermes_home",
    "last_sibling_snapshots", "plan_to_dict", "receipt_current_get", "begin_update_receipt",
    "receipt_current_reset", "record_stage", "publish_stage", "write_fleet_restart_pending_marker",
    "current_checkout_sha", "run_completion", "settle_lost_completion",
    "finalize_interrupted_update_receipt", "accept_completion_pm_receipt", "adopt_terminal_receipt",
    "resume_windows_gateways_after_update", "owe_followup", "adopt_retired_channel",
    "unrestored_autostash_notice", "stop_for_relaunch", "time_strftime", "utc_now_strftime",
    "prune_orphan_rescue_refs", "tree_syntax_error", "arm_tree_move", "checkout_move.enter",
    "checkout_move.exit", "files_added_by", "drop_added_files", "tree_whole_at",
    "settle_failed_tree_move", "disarm_commit_obligations", "owe_restore_of",
    "interrupted_pull_marker", "marker_unlink", "marker_is_file", "arm_commit_point",
    "is_object_id", "sync_with_upstream_if_needed", "push_synced_fork", "clear_pending_autostash",
    "discard_stashed_changes", "park_stashed_changes", "restore_stashed_changes", "moves_for",
    "preflight_refusal", "downgrade_refusal", "assess_parked_branch_switch",
    "print_parked_branch_skip_warning", "branch_head_suffix", "print_parked_branch_kept_notice",
    "park_detached_head", "stdin_isatty", "stdout_isatty", "target_would_move_head",
    "stash_local_changes_if_needed", "pause_at_commit_point", "github_compare_behind",
    "count_commits_between", "checkout_version", "update_marker_path", "marker_read_text",
    "getpid", "record_fact", "collect_runtime_inventory", "record_plan_in_receipt",
    "expose_pm_git", "ensure_non_trampoline_git", "enclosing_repo_of_checkout",
    "damaged_checkout_message", "discard_lockfile_churn", "normalize_managed_eol",
    "get_origin_url", "is_fork", "format_update_failure_stage", "should_zip_fallback_on_update_error",
    "update_via_zip", "called_process_error_is_python_dep_install", "print_called_process_error_tail",
    "is_windows", "git_error_stop_class", "git_output_stop_class",
    "git_run", "capture_head_sha", "update_movement_baseline", "refuse_unselected_head",
    "verify_head_after_pull", "landed_off_target", "missing_branch_stop", "resolved_commit",
    "is_shallow_checkout", "tip_shas", "base_git_cmd", "map_ssl_cert_file_for_git",
    "upstream_broken_header", "updates_config", "finalize_receipt", "settle_windows_resume",
]


def self_test(ns):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "update_cmd_flow2.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").splitlines() if l.strip()]
    fixture_funcs = {c["func"] for c in cases}
    if fixture_funcs != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(fixture_funcs - set(PY)), sorted(set(PY) - fixture_funcs)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("SELFTEST FATAL <5 cases for: %s" % thin)
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    failclosed = 0
    names = set(PY.values()) | set(PY) | set(SEAM_NAMES)
    for c in cases:
        inp = c["in"]
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["statewin"] = None
        try:
            drive(ns, c["func"], inp)
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec))
                bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec))
                bad += 1
        if any(isinstance(e.get("ret"), dict) and e["ret"].get("msg", "").startswith("unscripted seam:")
               for e in CUR["trace"]):
            failclosed += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors, %d cases hit fail-closed seams"
          % (len(cases), bad, failclosed))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(extract())
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
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["statewin"] = None
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        if func in STATEFUL:
            want["state"] = norm(CUR["statewin"]) if CUR["statewin"] is not None else {}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE update_cmd_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE update_cmd_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
