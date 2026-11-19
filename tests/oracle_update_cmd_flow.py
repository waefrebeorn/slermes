#!/usr/bin/env python3
"""Oracle for update_cmd_flow — the UNCLAIMED update-pipeline FLOW cores of
hermes_cli/update_cmd.py (LIVE authority, py3.14 quarry venv).

Ports covered: retired shims (get_default_hermes_root, _ensure_venv_pip,
_pip_install_prefix), config/receipt leaves (_updates_config,
_record_update_step, _record_update_skip, _record_pre_update_backup_outcome,
_record_snapshot_stage, _finalize_receipt), git plumbing (_git_run,
_base_git_cmd, _is_shallow_checkout, _tip_shas, _resolved_commit,
_current_branch_name, _landed_off_target, _update_movement_baseline,
_missing_branch_stop, _refuse_unselected_head, _verify_head_after_pull,
_map_ssl_cert_file_for_git, _heal_stale_shallow_checkout) and channel/resume
(_source_update_channel, _update_run_channel, _settle_windows_resume,
_resume_paused_gateways_at_exit, _pause_gateways_for_update,
_upstream_broken_header).

Claimed siblings (_capture_head_sha, _resolve_pre_update_backup_mode,
_no_prompt_git_kwargs) and foreign collaborators are SEAMED; unclaimed ported
siblings compose for real. _best_effort (update_cmd_common, unclaimed) is
AST-extracted and run live (its logger.debug is silent). In-function imports
are SILENT (stub modules pre-installed in sys.modules); only effectful calls
fire seams.

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the
quarry (no heavy import), module constants seeded (Assign AND AnnAssign),
every effect replaced by a recording stub returning the SAME scripted results
the C harness feeds its universal seam. Compares the ordered DECISION SEQUENCE
(trace of seam calls) plus the return value (and the mutated request window
for settle_windows_resume). Scripted {"__raise__": cls} raises a REAL exception
of that class; drive() catches BaseException so KeyboardInterrupt/SystemExit
pass through live except-Exception clauses exactly as the C port propagates
them. A MISSING seam key fail-closes (RuntimeError) on both sides; a scripted
null is a valid None result.

Input: stdin JSON lines from t_port_update_cmd_flow:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...][, "state":...]}}
Summary: ORACLE update_cmd_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import contextlib
import copy
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
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "update_cmd_flow_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "update_cmd.py")
SRC_COMMON = os.path.join(QUARRY, "hermes_cli", "update_cmd_common.py")

# C harness func -> live python name.
PY = {
    "get_default_hermes_root": "get_default_hermes_root",
    "ensure_venv_pip": "_ensure_venv_pip",
    "pip_install_prefix": "_pip_install_prefix",
    "updates_config": "_updates_config",
    "record_update_step": "_record_update_step",
    "record_update_skip": "_record_update_skip",
    "record_pre_update_backup_outcome": "_record_pre_update_backup_outcome",
    "record_snapshot_stage": "_record_snapshot_stage",
    "finalize_receipt": "_finalize_receipt",
    "git_run": "_git_run",
    "base_git_cmd": "_base_git_cmd",
    "is_shallow_checkout": "_is_shallow_checkout",
    "tip_shas": "_tip_shas",
    "resolved_commit": "_resolved_commit",
    "current_branch_name": "_current_branch_name",
    "landed_off_target": "_landed_off_target",
    "update_movement_baseline": "_update_movement_baseline",
    "missing_branch_stop": "_missing_branch_stop",
    "refuse_unselected_head": "_refuse_unselected_head",
    "verify_head_after_pull": "_verify_head_after_pull",
    "map_ssl_cert_file_for_git": "_map_ssl_cert_file_for_git",
    "heal_stale_shallow_checkout": "_heal_stale_shallow_checkout",
    "source_update_channel": "_source_update_channel",
    "update_run_channel": "_update_run_channel",
    "settle_windows_resume": "_settle_windows_resume",
    "resume_paused_gateways_at_exit": "_resume_paused_gateways_at_exit",
    "pause_gateways_for_update": "_pause_gateways_for_update",
    "upstream_broken_header": "_upstream_broken_header",
}
WANTED = set(PY.values())
STATEFUL = {"settle_windows_resume"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "statewin": None}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
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


class RetiredUpdaterHook(RuntimeError):
    pass


class ChannelError(ValueError):
    pass


class UpstreamTargetBroken(Exception):
    pass


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
    "RetiredUpdaterHook": RetiredUpdaterHook,
    "ChannelError": ChannelError,
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
    """A Path stand-in: verbatim POSIX stringification (no normalization)."""

    def __init__(self, s):
        self.posix = s if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix


class TaggedFn:
    """A callable object (no __get__, so attribute access does not bind self)
    carrying a marker for atexit.unregister and firing a seam when called."""

    def __init__(self, marker, seam_name, argfn):
        self.__uc_marker__ = marker
        self._seam = seam_name
        self._argfn = argfn

    def __call__(self, *a, **k):
        return seam(self._seam, self._argfn(*a, **k))


class MainDouble:
    def __init__(self):
        self._resume_windows_gateways_after_update = TaggedFn(
            "m._resume_windows_gateways_after_update",
            "resume_windows_gateways_after_update", lambda token: {"token": token})
        self._pause_windows_gateways_for_update = TaggedFn(
            "m._pause_windows_gateways_for_update",
            "pause_windows_gateways_for_update", lambda: {})

    @property
    def PROJECT_ROOT(self):
        return PathDouble(seam("project_root", {}))


class EnvironDouble:
    def get(self, name, default=None):
        r = seam("environ_get", {"name": name})
        return r if r is not None else default

    def __setitem__(self, name, value):
        seam("environ_set", {"name": name, "value": value})


class OsDouble:
    def __init__(self):
        self.environ = EnvironDouble()


class SysDouble:
    def __init__(self, platform):
        self.platform = platform

    def exit(self, code=0):
        raise SystemExit(code)


class AtexitDouble:
    def unregister(self, fn):
        return seam("atexit_unregister", {"fn": getattr(fn, "__uc_marker__", None)})


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return o.posix
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, subprocess.CompletedProcess):
        return {"args": norm(o.args), "returncode": o.returncode,
                "stdout": norm(o.stdout), "stderr": norm(o.stderr)}
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
    import time
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
        "__name__": "hermes_cli.update_cmd_extracted",
        "json": json, "os": os, "sys": sys, "subprocess": subprocess,
        "shlex": shlex, "_time": time, "logging": logging,
        "Path": Path, "dataclass": dataclass, "NoReturn": NoReturn,
        "suppress": contextlib.suppress, "contextmanager": contextlib.contextmanager,
        "logger": RecLogger(),
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
    # the module's `logger = logging.getLogger(__name__)` Assign overwrites the
    # seeded recorder — re-set it AFTER the constant loop.
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # _best_effort (unclaimed helper) from update_cmd_common.py — its
    # @contextmanager decorator is part of the FunctionDef node, so compiling
    # the node applies it; its logger global resolves to the silent RecLogger.
    common_tree = ast.parse(src_common)
    for node in common_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_best_effort":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_COMMON, "exec"), ns)
    ns["logger"] = RecLogger()
    missing = (WANTED | {"_best_effort"}) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("NETWORK_GIT_TIMEOUT_SECONDS",):
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


def run_git_stub(git_cmd, args, cwd=None, capture_output=False, text=False,
                 encoding=None, errors=None, check=False, **kw):
    r = seam("run_git", {"git_cmd": git_cmd, "args": args, "cwd": posix(cwd),
                         "capture_output": capture_output, "text": text,
                         "encoding": encoding, "errors": errors, "check": check, **kw})
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def print_stub(*a, file=None, **k):
    text = a[0] if a else ""
    seam("print", {"text": text})
    return None


def install_stubs(ns):
    # claimed siblings + foreign collaborators (bare globals)
    ns["_no_prompt_git_kwargs"] = lambda: seam("no_prompt_git_kwargs", {})
    ns["_m"] = lambda: MainDouble()
    ns["_record_stop"] = lambda reason, without_receipt=None: seam(
        "record_stop", {"reason": reason, "without_receipt": without_receipt})
    ns["_resolve_pre_update_backup_mode"] = lambda args: seam(
        "resolve_pre_update_backup_mode",
        {"args": vars(args) if isinstance(args, types.SimpleNamespace) else args})
    ns["_completion_receipt"] = types.SimpleNamespace(
        record_stage=lambda stage, status, mode=None: seam(
            "record_stage", {"stage": stage, "status": status, "mode": mode}))
    ns["stop_historical_updater"] = lambda hook, error=None: seam(
        "stop_historical_updater", {"hook": hook})
    ns["_capture_head_sha"] = lambda git_cmd, cwd: seam(
        "capture_head_sha", {"git_cmd": git_cmd, "cwd": posix(cwd)})
    ns["_commit"] = types.SimpleNamespace(
        disarm_commit_obligations=lambda: seam("disarm_commit_obligations", {}))
    ns["_posix_pause"] = types.SimpleNamespace(
        arm_pause=lambda no_gateway_restart=False, force=False: seam(
            "arm_pause", {"no_gateway_restart": no_gateway_restart, "force": force}))
    ns["print"] = print_stub

    # stub packages (every intermediate package registered)
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = []
    sys.modules["hermes_cli"] = pkg

    stub_mod("hermes_cli.config",
             load_config=lambda: seam("load_config", {}),
             get_config_path=lambda: seam("get_config_path", {}),
             require_readable_config_before_write=lambda path: seam(
                 "require_readable_config_before_write", {"path": path}))
    stub_mod("hermes_cli.update_receipt",
             record_step=lambda step, ok, detail="": seam(
                 "record_step", {"step": step, "ok": ok, "detail": detail}),
             record_skip=lambda step, reason: seam(
                 "record_skip", {"step": step, "reason": reason}),
             finalize_update_receipt=lambda status: seam(
                 "finalize_update_receipt", {"status": status}),
             owe_followup=lambda update_id, kind, message, retry=None: seam(
                 "owe_followup", {"update_id": update_id, "kind": kind,
                                  "message": message, "retry": retry}))
    stub_mod("hermes_cli._subprocess_compat",
             windows_hide_flags=lambda: seam("windows_hide_flags", {}))
    stub_mod("hermes_cli.update_custody", run_git=run_git_stub)
    stub_mod("hermes_cli.gitlock",
             heal_shallow_history=lambda repo_root, branch, **kw: seam(
                 "heal_shallow_history", {"repo_root": posix(repo_root), "branch": branch, **kw}))
    stub_mod("hermes_cli.release_channels",
             validate_name=lambda name: seam("validate_name", {"name": name}))
    stub_mod("hermes_cli.update_channel",
             resolve_update_channel=lambda config, root: seam(
                 "resolve_update_channel", {"config": config, "root": posix(root)}))
    stub_mod("hermes_cli.update_cmd_windows",
             _resume_windows_gateways_after_update=TaggedFn(
                 "update_cmd_windows._resume_windows_gateways_after_update",
                 "resume_windows_gateways_after_update", lambda token: {"token": token}))
    sys.modules["atexit"] = AtexitDouble()


# ── materialization ──

def mat_args(inp):
    a = inp.get("args")
    return types.SimpleNamespace(**a) if isinstance(a, dict) else None


def mat_opts(inp):
    return types.SimpleNamespace(**(inp.get("opts") or {}))


def mat_broken(inp):
    b = inp.get("broken")
    return types.SimpleNamespace(**b) if isinstance(b, dict) else None


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

    if func == "get_default_hermes_root":
        return f()
    if func == "ensure_venv_pip":
        return f(inp.get("pip_cmd"), inp.get("python_exe", ""))
    if func == "pip_install_prefix":
        return f(inp.get("uv_bin"))
    if func == "updates_config":
        return norm(f())
    if func == "record_update_step":
        f(inp.get("step", ""), inp.get("ok", False), inp.get("detail", ""))
        return None
    if func == "record_update_skip":
        f(inp.get("step", ""), inp.get("reason", ""))
        return None
    if func == "record_pre_update_backup_outcome":
        f(mat_args(inp), inp.get("snapshot_id"))
        return None
    if func == "record_snapshot_stage":
        f(mat_args(inp), inp.get("snapshot_id"))
        return None
    if func == "finalize_receipt":
        f(inp.get("status", ""), inp.get("debug_message", ""))
        return None
    if func == "git_run":
        r = f(git_cmd, inp.get("args"), inp.get("cwd"),
              check=inp.get("check", False), network=inp.get("network", False))
        return norm(r)
    if func == "base_git_cmd":
        return f()
    if func == "is_shallow_checkout":
        return f(git_cmd)
    if func == "tip_shas":
        return norm(f(git_cmd, inp.get("target_ref", ""), inp.get("base", "HEAD")))
    if func == "resolved_commit":
        return f(git_cmd, inp.get("ref", ""))
    if func == "current_branch_name":
        return f(git_cmd, check=inp.get("check", False))
    if func == "landed_off_target":
        return f(git_cmd, inp.get("target_sha", ""), merged=inp.get("merged", True))
    if func == "update_movement_baseline":
        return f(git_cmd, inp.get("pre_pull_sha"), inp.get("pre_sync_sha"),
                 inp.get("rollback_branch"), inp.get("target_sha"))
    if func == "missing_branch_stop":
        return f(git_cmd, inp.get("branch", ""))
    if func == "refuse_unselected_head":
        f(git_cmd, inp.get("target_sha", ""), inp.get("unmoved"))
        return None
    if func == "verify_head_after_pull":
        return f(git_cmd, inp.get("branch", ""), inp.get("pre_pull_sha"),
                 in_place_update=inp.get("in_place_update", False),
                 _windows_gateway_resume=inp.get("resume_token"))
    if func == "map_ssl_cert_file_for_git":
        f(git_cmd)
        return None
    if func == "heal_stale_shallow_checkout":
        f(PathDouble(inp.get("repo_root", "")), inp.get("branch", ""))
        return None
    if func == "source_update_channel":
        return f(mat_args(inp), channel=inp.get("channel"),
                 branch_explicit=inp.get("branch_explicit", False))
    if func == "update_run_channel":
        return f(mat_args(inp))
    if func == "settle_windows_resume":
        req = copy.deepcopy(inp.get("request"))
        CUR["statewin"] = req
        f(req)
        return None
    if func == "resume_paused_gateways_at_exit":
        f(copy.deepcopy(inp.get("token")), copy.deepcopy(inp.get("request")))
        return None
    if func == "pause_gateways_for_update":
        return norm(f(mat_opts(inp)))
    if func == "upstream_broken_header":
        return f(mat_broken(inp))
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
    "print", "record_stop", "project_root", "stop_historical_updater",
    "load_config", "record_step", "record_skip", "resolve_pre_update_backup_mode",
    "record_stage", "finalize_update_receipt", "no_prompt_git_kwargs",
    "windows_hide_flags", "run_git", "capture_head_sha", "disarm_commit_obligations",
    "resume_windows_gateways_after_update", "pause_windows_gateways_for_update",
    "arm_pause", "environ_get", "environ_set", "heal_shallow_history",
    "validate_name", "get_config_path", "require_readable_config_before_write",
    "resolve_update_channel", "atexit_unregister", "owe_followup",
]


def self_test(ns):
    """Drive every fixture case live (no C): surface binding bugs / script gaps."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "update_cmd_flow.jsonl"
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
        print("ORACLE update_cmd_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE update_cmd_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()