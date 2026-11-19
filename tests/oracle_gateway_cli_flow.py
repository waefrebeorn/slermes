#!/usr/bin/env python3
"""Oracle for gateway_cli_flow.c — hermes_cli/gateway.py FLOW cores + leaves
(LIVE authority: the quarry hermes_cli/gateway.py).

Drives the LIVE functions with SCRIPTED I/O: the 25 targets are AST-extracted
from the quarry (no heavy hermes_cli.gateway import), module constants seeded
(Assign AND AnnAssign, plain-Name targets only; GATEWAY_* exit codes seeded from
gateway/restart.py), and every systemctl run, unit-file read, host-record probe,
foreign-module import, config read, print/log emission and process signal
replaced by a recording stub returning the SAME scripted results the C harness
feeds its universal seam. Internal composition (_parse_kv_pairs inside
_systemctl_show; _current_profile_name / host_multiplexer_serving /
_served_by_another_host_gateway / _named_profile_refused_under_multiplexer /
_host_decision_exit_code inside their callers) runs REAL on both sides.
Compares the ordered DECISION SEQUENCE + return value with ordered-dict
comparison (insertion order is semantic). Per-case BaseException catch
(SystemExit escapes except Exception and is compared as a raise spec; scripted
raises round-trip bare).

Input: stdin JSON lines from t_port_gateway_cli_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE gateway_cli_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.

Script semantics (identical on both sides): a seams[name] ARRAY is a per-call
queue (last value repeats); a single value is a constant; a MISSING script
returns None (NULL in C). {"__raise__": cls, "msg": ...} scripts an exception.

Fixture domain (kept identical on both sides): ASCII text; POSIX lexical paths
(no symlinks/resolve, no trailing slashes/backslashes); str.strip()/splitlines()
model the ASCII whitespace set; integral JSON numbers read as int; gateway /
decision doubles are JSON objects {home,pid,profile_label,...} /
{outcome,message,transient}; scripted CalledProcessError/TimeoutExpired carries
returncode/cmd (msg == the real str(exc)).
"""
import ast
import builtins
import json
import os
import subprocess
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# Scrubbed-env runner: derive home from __file__ (repo is parent.parent).
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "gc_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway.py")

PY = {
    "parse_kv_pairs": "_parse_kv_pairs",
    "systemd_cli_bits": "_systemd_cli_bits",
    "runtime_state_pid": "_runtime_state_pid",
    "host_decision_exit_code": "_host_decision_exit_code",
    "print_info_lines": "_print_info_lines",
    "print_indented": "_print_indented",
    "current_profile_name": "_current_profile_name",
    "systemd_unit_is_active": "_systemd_unit_is_active",
    "systemctl_show": "_systemctl_show",
    "systemd_unit_installed": "_systemd_unit_installed",
    "agent_timeout_setting": "_agent_timeout_setting",
    "systemd_scope_preamble": "_systemd_scope_preamble",
    "retire_hermes_replace_dropin": "_retire_hermes_replace_dropin",
    "host_multiplexer_serving": "host_multiplexer_serving",
    "served_by_another_host_gateway": "_served_by_another_host_gateway",
    "host_multiplexer_for_all_verb": "_host_multiplexer_for_all_verb",
    "host_multiplexer_is_ours": "_host_multiplexer_is_ours",
    "print_unfolded_gateway_note": "_print_unfolded_gateway_note",
    "stop_host_multiplexer": "_stop_host_multiplexer",
    "print_multiplex_standalone_reason": "_print_multiplex_standalone_reason",
    "print_served_ingress_urls": "_print_served_ingress_urls",
    "served_profile_needs_no_service": "_served_profile_needs_no_service",
    "named_profile_refused_under_multiplexer": "_named_profile_refused_under_multiplexer",
    "attach_to_host_gateway_or_guard": "_attach_to_host_gateway_or_guard",
    "systemd_reset_and_run": "_systemd_reset_and_run",
}
WANTED = set(PY.values())

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}
NOTICE_VALUE = None  # extracted from gateway_multiplex_mode.py


# ── exception construction ──

def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    if cls == "CalledProcessError":
        return subprocess.CalledProcessError(spec.get("returncode", 1), spec.get("cmd", "systemctl"))
    if cls == "TimeoutExpired":
        return subprocess.TimeoutExpired(spec.get("cmd", "systemctl"), spec.get("timeout", 90))
    if cls == "SystemctlUnavailableError":
        return type("SystemctlUnavailableError", (RuntimeError,), {})(msg)
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    sc = CUR["scripts"].get(name)
    if sc is None:
        r = None
    elif isinstance(sc, list):
        i = CUR["idx"].get(name, 0)
        CUR["idx"][name] = i + 1
        r = sc[min(i, len(sc) - 1)] if sc else None
    else:
        r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


def rec(name):
    return lambda *a, **k: seam(name, k if k else ({"value": a[0]} if len(a) == 1 else {}))


def posix(p):
    if p is None:
        return None
    if isinstance(p, (PathDouble, Path)):
        return p.as_posix()
    return str(p).replace("\\", "/")


def _pathify(r):
    return Path(r) if isinstance(r, str) else r


def _exc_proj(exc):
    d = {"__raise__": type(exc).__name__, "msg": str(exc)}
    rc = getattr(exc, "returncode", None)
    cmd = getattr(exc, "cmd", None)
    if rc is not None:
        d["returncode"] = rc
    if cmd is not None:
        d["cmd"] = cmd
    return d


# ── normalization / canonical comparison ──

def norm(o):
    if isinstance(o, (GatewayDouble, DecisionDouble)):
        return norm(o._data)
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (Path, PathDouble)):
        return o.as_posix()
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── doubles ──

def _parent_lex(s):
    while len(s) > 1 and s.endswith("/"):
        s = s[:-1]
    idx = s.rfind("/")
    if idx < 0:
        return "."
    if idx == 0:
        return "/"
    return s[:idx]


def _name_lex(s):
    while len(s) > 1 and s.endswith("/"):
        s = s[:-1]
    idx = s.rfind("/")
    return s[idx + 1:] if idx >= 0 else s


def _join_lex(a, b):
    if not b:
        return a
    if b.startswith("/"):
        return b
    if not a:
        return b
    if a.endswith("/"):
        return a + b
    return a + "/" + b


class PathDouble:
    def __init__(self, p):
        if isinstance(p, PathDouble):
            s = p.s
        elif p is None:
            raise TypeError("expected str, bytes or os.PathLike object, not NoneType")
        else:
            s = str(p)
        s = s.replace("\\", "/")
        while len(s) > 1 and s.endswith("/"):
            s = s[:-1]
        self.s = s

    def exists(self):
        return seam("path_exists", {"path": self.s})

    def read_text(self, encoding=None):
        return seam("read_text", {"path": self.s, "encoding": encoding})

    def unlink(self):
        return seam("unlink", {"path": self.s})

    @property
    def parent(self):
        return PathDouble(_parent_lex(self.s))

    @property
    def name(self):
        return _name_lex(self.s)

    def __truediv__(self, other):
        return PathDouble(_join_lex(self.s, str(other)))

    def as_posix(self):
        return self.s

    def __fspath__(self):
        return self.s

    def __str__(self):
        return self.s


class GatewayDouble:
    def __init__(self, data):
        self._data = data if isinstance(data, dict) else {}
        self.home = self._data.get("home")
        self.pid = self._data.get("pid")
        self.profile_label = self._data.get("profile_label")

    def describe(self):
        return seam("gateway.describe", {})


class DecisionDouble:
    def __init__(self, data):
        self._data = data if isinstance(data, dict) else {}
        self.outcome = self._data.get("outcome")
        self.message = self._data.get("message", "")
        self.transient = self._data.get("transient", False)


class RecLogger:
    def debug(self, msg, *a, **kw):
        args = {"msg": msg}
        if a:
            args["args"] = list(a)
        if kw.get("exc_info"):
            args["exc_info"] = True
        return seam("logger.debug", args)

    def warning(self, msg, *a, **kw):
        args = {"msg": msg}
        if a:
            args["args"] = list(a)
        return seam("logger.warning", args)


def _gateway(r):
    return GatewayDouble(r) if isinstance(r, dict) else r


def _decision(r):
    return DecisionDouble(r) if isinstance(r, dict) else r


def _cp(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


# ── extraction ──

def _extract_notice():
    global NOTICE_VALUE
    p = os.path.join(QUARRY, "hermes_cli", "gateway_multiplex_mode.py")
    src = open(p, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == "STANDALONE_DEPRECATION_NOTICE":
                    ns = {}
                    exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec"), ns)
                    NOTICE_VALUE = ns["STANDALONE_DEPRECATION_NOTICE"]
                    return
    print("FATAL: STANDALONE_DEPRECATION_NOTICE not found")
    sys.exit(2)


def extract():
    import contextlib
    import logging
    import shutil
    import signal
    import socket
    import time
    import __future__

    _extract_notice()
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("gateway_cli_extracted")
    sys.modules["gateway_cli_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "gateway_cli_extracted",
        "json": json, "os": os, "sys": sys, "subprocess": subprocess,
        "contextlib": contextlib, "time": time, "socket": socket,
        "shutil": shutil, "signal": signal, "logging": logging, "Path": Path,
    })
    tree = ast.parse(src)
    # Pass 1: module constants (plain-Name Assign/AnnAssign targets only).
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)):
                continue
        else:
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # GATEWAY_* exit codes are ImportFrom (gateway/restart.py), not Assign.
    ns["GATEWAY_SERVICE_RESTART_EXIT_CODE"] = 75
    ns["GATEWAY_FATAL_CONFIG_EXIT_CODE"] = 78
    # Pass 2: target defs (future-annotations keeps annotations as strings).
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for name in ("_CAPTURE_TEXT", "GATEWAY_SERVICE_RESTART_EXIT_CODE", "GATEWAY_FATAL_CONFIG_EXIT_CODE"):
        if name not in ns:
            print("FATAL missing constant: %s" % name)
            sys.exit(2)
    return ns


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _submod(pkg, name):
    m = types.ModuleType(name)
    sys.modules[name] = m
    setattr(pkg, name.rsplit(".", 1)[-1], m)
    return m


def install_stubs(ns):
    # ── module-level names (claimed siblings + imported leaves) ──
    ns["get_hermes_home"] = lambda: _pathify(seam("get_hermes_home", {}))
    ns["get_service_name"] = lambda: seam("get_service_name", {})
    ns["get_systemd_unit_path"] = lambda system=False: PathDouble(seam("get_systemd_unit_path", {"system": system}))
    ns["_run_systemctl"] = lambda args, *, system=False, **kw: _cp(seam("run_systemctl", {"args": args, "system": system, **kw}))
    ns["_positive_pid"] = lambda value: seam("positive_pid", {"value": value})
    ns["_profile_suffix"] = lambda: seam("profile_suffix", {})
    ns["_select_systemd_scope"] = lambda system=False: seam("select_systemd_scope", {"system": system})
    ns["_require_root_for_system_service"] = lambda action: seam("require_root_for_system_service", {"action": action})
    ns["_preflight_user_systemd"] = lambda: seam("preflight_user_systemd", {})
    ns["_require_service_installed"] = lambda action, system=False: seam("require_service_installed", {"action": action, "system": system})
    ns["_systemd_error_indicates_start_limit"] = lambda exc: seam("systemd_error_indicates_start_limit", {"exc": _exc_proj(exc)})
    ns["_systemd_service_is_start_limited"] = lambda system=False: seam("systemd_service_is_start_limited", {"system": system})
    ns["_print_systemd_start_limit_wait"] = lambda system=False: seam("print_systemd_start_limit_wait", {"system": system})
    ns["_service_scope_label"] = lambda system=False: seam("service_scope_label", {"system": system})
    ns["_wait_for_systemd_service_restart"] = lambda *, system=False, previous_pid=None: seam("wait_for_systemd_service_restart", {"system": system, "previous_pid": previous_pid})
    ns["supports_systemd_services"] = lambda: seam("supports_systemd_services", {})
    ns["_is_service_installed"] = lambda: seam("is_service_installed", {})
    ns["named_profile_served_by_running_multiplexer"] = lambda profile_name=None: seam("named_profile_served", {"name": profile_name})
    ns["_guard_named_profile_under_multiplexer"] = lambda force=False: seam("guard_named_profile_under_multiplexer", {"force": force})
    ns["_running_under_gateway_supervisor"] = lambda: seam("running_under_gateway_supervisor", {})
    ns["find_gateway_pids"] = lambda all_profiles=False: seam("find_gateway_pids", {"all_profiles": all_profiles})
    ns["kill_gateway_processes"] = lambda: seam("kill_gateway_processes", {})
    ns["terminate_pid"] = lambda pid, *, force=False, expected_start_time=None: seam("terminate_pid", {"pid": pid, "force": force})
    ns["read_raw_config"] = lambda: seam("read_raw_config", {})
    ns["print_info"] = lambda text: seam("print_info", {"text": text})
    ns["print_success"] = lambda text: seam("print_success", {"text": text})
    ns["print_error"] = lambda text: seam("print_error", {"text": text})
    ns["print_warning"] = lambda text: seam("print_warning", {"text": text})

    def _print(*a):
        if len(a) == 1:
            return seam("print", {"text": a[0]})
        if not a:
            return seam("print", {})
        return seam("print", {"args": list(a)})
    ns["print"] = _print
    ns["logger"] = RecLogger()
    ns["os"] = types.SimpleNamespace(getenv=lambda name, default=None: seam("os_getenv", {"name": name}))
    ns["gateway_service_owner"] = types.SimpleNamespace(
        foreign_systemd_unit_home=lambda s: seam("foreign_systemd_unit_home", {"system": s}))

    # ── in-function imports via sys.modules stubs ──
    gateway = _pkg("gateway")
    ha = _submod(gateway, "gateway.host_attach")
    ha.ATTACH = "attach"
    ha.REFUSE = "refuse"
    ha.REPLACE_HOST = "replace-host"
    ha.host_gateway_serving = lambda profile, *, wait_for_channel=0.0: _gateway(seam("host_gateway_serving", {"name": profile}))
    ha.host_gateway = lambda *, wait_for_channel=0.0: _gateway(seam("host_gateway", {}))
    ha.launched_by_other_tenant = lambda owner_home, our_home: seam("launched_by_other_tenant", {"owner_home": posix(owner_home), "our_home": posix(our_home)})
    ha.decide = lambda home, *, replace=False: _decision(seam("decide", {"home": posix(home), "replace": replace}))
    ha.standalone_rescan_message = lambda profile: seam("standalone_rescan_message", {"profile": profile})

    st = _submod(gateway, "gateway.status")
    st._get_process_hermes_home = lambda: seam("get_process_hermes_home", {})
    st._same_hermes_home = lambda left, right: seam("same_hermes_home", {"left": posix(left), "right": posix(right)})

    hc = _pkg("hermes_constants")
    hc.profile_name_for_home = lambda home: seam("profile_name_for_home", {"home": posix(home)})
    hc.display_hermes_home = lambda home: seam("display_hermes_home", {"home": posix(home)})

    hcli = _pkg("hermes_cli")
    profiles = _submod(hcli, "hermes_cli.profiles")
    profiles.profile_is_standalone = lambda home: seam("profile_is_standalone", {"home": posix(home)})

    mm = _submod(hcli, "hermes_cli.gateway_multiplex_mode")
    mm.recorded_standalone_warning_lines = lambda: seam("recorded_standalone_warning_lines", {})
    mm.STANDALONE_DEPRECATION_NOTICE = NOTICE_VALUE

    ms = _submod(hcli, "hermes_cli.gateway_multiplex_served")
    ms.served_profile_ingress_urls = lambda profile=None: seam("served_profile_ingress_urls", {"profile": profile})
    ms.format_ingress_url_lines = lambda per_platform, *, indent="": seam("format_ingress_url_lines", {"per_platform": per_platform, "indent": indent})


# ── drive ──

def drive(ns, func, inp):
    name = PY[func]
    try:
        if func == "parse_kv_pairs":
            ret = ns[name](inp.get("items"))
        elif func == "systemd_cli_bits":
            ret = ns[name](inp.get("system", False))
        elif func == "runtime_state_pid":
            ret = ns[name](inp.get("state"))
        elif func == "host_decision_exit_code":
            ret = ns[name](_decision(inp.get("decision")))
        elif func == "print_info_lines":
            ret = ns[name](*inp.get("lines", []))
        elif func == "print_indented":
            ret = ns[name](inp.get("text", ""), emit=lambda text: seam("emit", {"text": text}))
        elif func == "current_profile_name":
            ret = ns[name]()
        elif func == "systemd_unit_is_active":
            ret = ns[name](inp.get("system", False))
        elif func == "systemctl_show":
            ret = ns[name](tuple(inp.get("properties", [])), system=inp.get("system", False))
        elif func == "systemd_unit_installed":
            ret = ns[name]()
        elif func == "agent_timeout_setting":
            ret = ns[name](inp["env_var"], inp["key"], lambda value: seam("parse", {"value": value}))
        elif func == "systemd_scope_preamble":
            ret = ns[name](inp["action"], inp.get("system", False),
                           require_installed=inp.get("require_installed", True),
                           preflight_user=inp.get("preflight_user", False))
        elif func == "retire_hermes_replace_dropin":
            ret = ns[name](system=inp.get("system", False))
        elif func == "host_multiplexer_serving":
            ret = ns[name](inp.get("profile_name"))
        elif func == "served_by_another_host_gateway":
            ret = ns[name](inp.get("profile_name"))
        elif func == "host_multiplexer_for_all_verb":
            ret = ns[name]()
        elif func == "host_multiplexer_is_ours":
            ret = ns[name](_gateway(inp.get("owner")))
        elif func == "print_unfolded_gateway_note":
            ret = ns[name](_gateway(inp.get("owner")))
        elif func == "stop_host_multiplexer":
            ret = ns[name](_gateway(inp.get("owner")))
        elif func == "print_multiplex_standalone_reason":
            ret = ns[name]()
        elif func == "print_served_ingress_urls":
            ret = ns[name](inp.get("profile"))
        elif func == "served_profile_needs_no_service":
            ret = ns[name]()
        elif func == "named_profile_refused_under_multiplexer":
            ret = ns[name](force=inp.get("force", False))
        elif func == "attach_to_host_gateway_or_guard":
            ret = ns[name](force=inp.get("force", False), replace=inp.get("replace", False))
        elif func == "systemd_reset_and_run":
            ret = ns[name](inp["action"], system=inp.get("system", False), previous_pid=inp.get("previous_pid"))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec  # scripted raise round-trips bare
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def main():
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
        CUR["raised"] = []
        if func not in PY:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE gateway_cli_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_cli_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
