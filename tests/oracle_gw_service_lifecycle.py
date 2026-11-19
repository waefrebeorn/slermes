#!/usr/bin/env python3
"""Oracle for the hermes_cli/gateway.py service-lifecycle FLOW cores
(src/cli/port_gw_service_lifecycle.c).

Drives the LIVE functions with SCRIPTED I/O: the functions are AST-extracted
from the quarry (no heavy gateway import), their module constants are seeded,
and every OS / service / registry / subprocess dependency is replaced by a
recording monkeypatch stub returning the same scripted results the C harness
feeds its seams. Compares the DECISION SEQUENCE (ordered trace of seam calls)
plus the return value / exit verdict / raised exception, not just a scalar.

Input: stdin JSON lines from t_port_gw_service_lifecycle:
    {"func": ..., "in": {...}, "out": {"ret": ..., "exit": ..., "raised": ...,
     "trace": [...]}}
Summary: ORACLE gw_service_lifecycle: N cases, M mismatches. Exit 1 on any
mismatch; fail-closed FATAL exit 2 on extraction failure or zero cases.
"""
import ast
import contextlib
import json
import os
import subprocess
import sys
import types
from dataclasses import dataclass, field
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway.py")

WANTED_FUNCS = {
    "_cmd_start", "_cmd_stop", "_cmd_restart", "_cmd_install", "_cmd_uninstall",
    "_cmd_status", "_restart_all", "_restart_all_as_host",
    "_systemd_graceful_restart_action", "_handle_no_backend", "_no_backend_exit",
    "_service_backend", "_service_call", "_installed_service_kind_for",
    "_installed_service_kind", "_stop_installed_service", "_profile_name_from_home",
    "_unit_environment_value", "_positive_pid", "_systemd_main_pid_from_props",
    "_print_lines", "_service_mgmt_blocked", "_status_host_kind", "_running_under_s6",
    "_refuse_from_inside_gateway", "_is_service_installed",
}
WANTED_CLASSES = {"GatewayRuntimeSnapshot"}

CUR = {"func": None, "in": {}, "trace": [], "gw_idx": 0, "systemd_idx": 0,
       "systemd_status_idx": 0, "launchd_idx": 0, "launchd_install_idx": 0,
       "launchd_status_idx": 0, "guard_idx": 0, "sessions_idx": 0, "dispatch_idx": 0,
       "dispatch_all_idx": 0, "install_cli_idx": 0, "refuse_restart_idx": 0,
       "start_repl_idx": 0, "reap_idx": 0}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


def fatal(msg):
    print("FATAL %s" % msg)
    sys.exit(2)


def script_obj(key, idxkey):
    v = CUR["in"].get(key)
    if v is None:
        return None
    if isinstance(v, dict):
        return v
    if isinstance(v, list) and v:
        i = CUR[idxkey]
        if i >= len(v):
            i = len(v) - 1
        CUR[idxkey] += 1
        it = v[i]
        return it if isinstance(it, dict) else None
    return None


def apply_exc(script):
    """Return (raised_name, exception_instance) for a script object."""
    if not script:
        return None, None
    ex = script.get("exit")
    if isinstance(ex, (int, float)):
        return "SystemExit", SystemExit(int(ex))
    t = script.get("exc")
    if t:
        msg = script.get("msg", "boom")
        if t == "CalledProcessError":
            return t, subprocess.CalledProcessError(1, "cmd")
        if t == "OSError":
            return t, OSError(msg)
        if t == "TimeoutExpired":
            return t, subprocess.TimeoutExpired("cmd", 10)
        if t == "SystemExit":
            return t, SystemExit(int(script.get("code", 1)))
        return t, RuntimeError(msg)
    return None, None


def rec_raised(e, raised):
    if raised:
        e["raised"] = raised
    else:
        e["ret"] = None


class RecLogger:
    def debug(self, fmt, *args, **kw):
        rec({"ev": "logger_debug", "msg": fmt % args if args else fmt})

    def warning(self, fmt, *args, **kw):
        rec({"ev": "logger_warning", "msg": fmt % args if args else fmt})

    def info(self, fmt, *args, **kw):
        rec({"ev": "logger_info", "msg": fmt % args if args else fmt})


def rec_print(*args, sep=" ", end="\n", file=None, flush=False):
    rec({"ev": "print", "text": sep.join(str(a) for a in args)})


def rec_print_error(text):
    rec({"ev": "print_error", "text": text})


def rec_exit(code=0):
    raise SystemExit(code)


def bool_stub(evname, key):
    def stub(*a, **kw):
        r = bool(cin(key, False))
        rec({"ev": evname, "ret": r})
        return r
    return stub


def owner_desc(owner):
    if owner is None:
        return None
    return {"pid": owner.pid, "profile_label": owner.profile_label,
            "profiles": list(owner.profiles), "describe": owner._describe}


def make_owner(d):
    if d is None:
        return None
    o = types.SimpleNamespace()
    o.pid = d.get("pid", 0)
    o.profile_label = d.get("profile_label", "")
    o.profiles = list(d.get("profiles", []))
    o._describe = d.get("describe", "")

    def describe():
        rec({"ev": "owner_describe", "ret": o._describe})
        return o._describe
    o.describe = describe
    return o


def snap_desc(s):
    return {"manager": s.manager, "service_installed": s.service_installed,
            "service_running": s.service_running, "gateway_pids": list(s.gateway_pids),
            "service_scope": s.service_scope}


class PathDouble:
    def __init__(self, path):
        self.path = str(path)

    def read_text(self, encoding=None):
        raised = bool(cin("unit_read_raises", False))
        text = cin("unit_text")
        if isinstance(text, str) and text.startswith("\ufeff"):
            text = text[1:]
        rec({"ev": "unit_read_text", "path": self.path,
             "ret": None if raised else text, "raised": raised})
        if raised:
            raise OSError("unreadable")
        return text


def extract():
    src = open(SRC, encoding="utf-8").read()
    import re
    import hashlib
    import shutil
    import getpass as getpass_real
    sys_double = types.SimpleNamespace(exit=rec_exit, platform="win32", argv=[],
                                       stdin=types.SimpleNamespace(isatty=lambda: False))
    ns = {"__name__": "gateway_extracted", "os": os, "re": re, "json": json,
          "subprocess": subprocess, "contextlib": contextlib, "shutil": shutil,
          "hashlib": hashlib, "Path": Path, "dataclass": dataclass, "field": field,
          "sys": sys_double, "logger": RecLogger(), "print": rec_print,
          "print_error": rec_print_error,
          "GATEWAY_FATAL_CONFIG_EXIT_CODE": 78, "GATEWAY_SERVICE_RESTART_EXIT_CODE": 75,
          "EXTERNAL_GATEWAY_SUPERVISOR_ENV": "HERMES_GATEWAY_EXTERNAL_SUPERVISOR"}
    tree = ast.parse(src)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec_node(node)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec_node(node)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in WANTED_CLASSES:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_FUNCS:
            exec_node(node)
    missing = WANTED_FUNCS - set(ns)
    if missing:
        fatal("extraction missed functions: %s" % sorted(missing))
    for cls in WANTED_CLASSES:
        if cls not in ns:
            fatal("extraction missed class %s" % cls)
    return ns


def install_stubs(ns):
    Snap = ns["GatewayRuntimeSnapshot"]

    # ── platform predicates ──
    ns["supports_systemd_services"] = bool_stub("supports_systemd", "supports_systemd")
    ns["is_macos"] = bool_stub("is_macos", "is_macos")
    ns["is_windows"] = bool_stub("is_windows", "is_windows")
    ns["is_wsl"] = bool_stub("is_wsl", "is_wsl")
    ns["is_container"] = bool_stub("is_container", "is_container")
    ns["is_termux"] = bool_stub("is_termux", "is_termux")
    ns["_is_apt_termux_install"] = bool_stub("is_apt_termux_install", "is_apt_termux")
    ns["_systemd_unit_installed"] = bool_stub("systemd_unit_installed", "systemd_unit_installed")

    def get_launchd_plist_path():
        class P:
            def exists(self):
                r = bool(cin("launchd_plist_exists", False))
                rec({"ev": "launchd_plist_exists", "ret": r})
                return r
        return P()
    ns["get_launchd_plist_path"] = get_launchd_plist_path

    ns["is_managed"] = bool_stub("is_managed", "is_managed")
    ns["named_profile_served_by_running_multiplexer"] = bool_stub(
        "named_profile_served_by_running_multiplexer", "named_served")
    ns["stop_profile_gateway"] = bool_stub("stop_profile_gateway", "stop_profile")
    ns["_spawn_detached_gateway"] = bool_stub("spawn_detached_gateway", "spawn_detached")
    ns["print_parked_status"] = bool_stub("print_parked_status", "print_parked_status")

    def detect_service_manager():
        r = cin("detect_service_mgr", "")
        rec({"ev": "detect_service_manager", "ret": r})
        return r

    def is_supervised():
        r = bool(cin("is_supervised", False))
        rec({"ev": "is_supervised_gateway_process", "ret": r})
        return r

    def get_service_name():
        r = cin("service_name", "hermes-gateway")
        rec({"ev": "get_service_name", "ret": r})
        return r

    def service_scope_label(system=False):
        r = cin("scope_label", "system" if system else "user")
        rec({"ev": "service_scope_label", "system": system, "ret": r})
        return r

    def current_profile_name():
        r = cin("current_profile_name", "default")
        rec({"ev": "current_profile_name", "ret": r})
        return r

    def get_hermes_home():
        r = cin("get_hermes_home", "C:/h")
        rec({"ev": "get_hermes_home", "ret": r})
        return r

    def active_profile_name():
        r = cin("active_profile_name")
        rec({"ev": "get_active_profile_name", "ret": r})
        return r

    def profile_is_standalone(home):
        r = bool(cin("profile_is_standalone", False))
        rec({"ev": "profile_is_standalone", "home": str(home), "ret": r})
        return r

    def routing_home():
        r = cin("routing_home", "C:/h")
        rec({"ev": "get_routing_process_hermes_home", "ret": r})
        return r

    def named_profile_home(home):
        r = cin("named_profile_home")
        rec({"ev": "named_profile_home", "home": str(home), "ret": r})
        return r

    def refuses_container(system):
        r = bool(cin("refuses_container", False))
        rec({"ev": "refuses_container_user_scope_install", "system": system, "ret": r})
        return r

    def managed_error(action):
        rec({"ev": "managed_error", "action": action})

    def record_service_choice(action):
        rec({"ev": "record_service_choice", "action": action})

    def profile_lifecycle(action, args):
        r = bool(cin("profile_lifecycle", False))
        rec({"ev": "profile_lifecycle", "action": action, "ret": r})
        return r

    def host_is_ours(owner):
        r = bool(cin("host_is_ours", False))
        rec({"ev": "host_multiplexer_is_ours", "ret": r})
        return r

    def find_pids(exclude_pids=None, all_profiles=False):
        r = list(cin("find_pids", []))
        rec({"ev": "find_gateway_pids", "ret": r})
        return r

    def kill_processes(force=False, exclude_pids=None, all_profiles=False):
        r = int(cin("kill_processes", 0))
        rec({"ev": "kill_gateway_processes", "all_profiles": all_profiles, "ret": r})
        return r

    def stop_host_multiplexer(owner):
        r = int(cin("stop_host_multiplexer", 0))
        rec({"ev": "stop_host_multiplexer", "owner_pid": owner.pid, "ret": r})
        return r

    def report_stop_failure():
        rec({"ev": "report_profile_stop_failure"})

    def wait_gateway_exit(timeout=10.0, force_after=5.0):
        rec({"ev": "wait_for_gateway_exit", "timeout": timeout, "force_after": force_after})

    def wait_port_free(*, timeout=10.0):
        rec({"ev": "wait_for_api_server_port_free"})

    def discard_host_record():
        rec({"ev": "discard_dead_host_record"})

    def forget_host_probe():
        rec({"ev": "forget_host_gateway_probe"})

    def run_gateway(verbose=0, quiet=False, replace=False, force=False, idle_exit=False):
        rec({"ev": "run_gateway", "verbose": verbose, "replace": replace})

    def get_snapshot(system=False):
        d = cin("snapshot", {})
        rec({"ev": "get_gateway_runtime_snapshot", "system": system, "ret": d})
        return Snap(manager=d.get("manager", "manual"),
                    service_installed=d.get("service_installed", False),
                    service_running=d.get("service_running", False),
                    gateway_pids=tuple(d.get("gateway_pids", [])),
                    service_scope=d.get("service_scope"))

    def report_incompat_owner(pids):
        r = bool(cin("report_incompat_owner", False))
        rec({"ev": "report_incompatible_owner", "pids": list(pids), "ret": r})
        return r

    def print_standalone_cfg():
        rec({"ev": "print_standalone_by_config"})

    def print_served_urls(profile=None):
        rec({"ev": "print_served_ingress_urls", "profile": profile})

    def print_unserved_urls(profile):
        rec({"ev": "print_unserved_shared_ingress", "profile": profile})

    def print_multiplex_reason():
        rec({"ev": "print_multiplex_standalone_reason"})

    def print_runtime_health():
        rec({"ev": "print_runtime_health"})

    def print_dup_creds():
        rec({"ev": "print_duplicate_credential_warnings"})

    def print_other_profiles():
        rec({"ev": "print_other_profiles_gateway_status"})

    def print_unfolded_note(owner):
        rec({"ev": "print_unfolded_gateway_note", "owner_pid": owner.pid})

    def print_mismatch(snapshot):
        rec({"ev": "print_gateway_process_mismatch", "snapshot": snap_desc(snapshot)})

    def host_owner():
        d = cin("host_owner")
        o = make_owner(d)
        rec({"ev": "host_multiplexer_for_all_verb", "ret": owner_desc(o)})
        return o

    def served_owner(profile_name=None):
        d = cin("served_owner")
        o = make_owner(d)
        rec({"ev": "served_by_another_host_gateway", "ret": owner_desc(o)})
        return o

    def get_linger_status(username=None):
        ok = bool(cin("linger_ok", False))
        rec({"ev": "get_systemd_linger_status", "ok": ok})
        return ok, ""

    def restart_wait_budget():
        r = float(cin("restart_wait_budget", 30.0))
        rec({"ev": "get_restart_exit_wait_budget", "ret": r})
        return r

    def process_start_time(pid):
        r = cin("process_start_time")
        rec({"ev": "get_process_start_time", "pid": pid, "ret": r})
        return r

    def drain_reporter(budget_s=None):
        r = cin("drain_reporter", "reporter")
        rec({"ev": "drain_progress_reporter", "budget_s": budget_s, "ret": r})
        return r

    def graceful_sigusr1(pid, drain_timeout, *, on_progress=None):
        r = bool(cin("graceful_sigusr1", False))
        rec({"ev": "graceful_restart_via_sigusr1", "pid": pid, "budget": drain_timeout,
             "on_progress": on_progress, "ret": r})
        return r

    def leave_restart_pending(pid, label, start_time=None):
        r = bool(cin("leave_restart_pending", False))
        rec({"ev": "leave_restart_pending", "pid": pid, "label": label,
             "start_time": start_time, "ret": r})
        return r

    def wait_systemd_restart(*, system=False, previous_pid=None, replacement_observed=None):
        ro = list(cin("replacement_observed", []))
        if replacement_observed is not None:
            replacement_observed.extend(ro)
        r = bool(cin("wait_systemd_restart", False))
        rec({"ev": "wait_for_systemd_service_restart", "system": system,
             "previous_pid": previous_pid, "replacement_observed": ro, "ret": r})
        return r

    def systemd_start_limited(system=False):
        r = bool(cin("systemd_start_limited", False))
        rec({"ev": "systemd_service_is_start_limited", "system": system, "ret": r})
        return r

    def read_unit_props(system=False, properties=("ActiveState", "SubState", "Result",
                                                 "ExecMainStatus", "MainPID")):
        r = dict(cin("unit_props", {}))
        rec({"ev": "read_systemd_unit_properties", "system": system, "ret": r})
        return r

    def get_running_pid():
        r = cin("get_running_pid")
        rec({"ev": "get_running_pid", "ret": r})
        return r

    def declares_external(pid):
        r = bool(cin("declares_external_supervisor", False))
        rec({"ev": "gateway_declares_external_supervisor", "pid": pid, "ret": r})
        return r

    def restart_externally(pid):
        rec({"ev": "restart_externally_supervised_gateway", "pid": pid})

    def getuser():
        r = cin("getuser", "user")
        rec({"ev": "getuser", "ret": r})
        return r

    # ── raising dispatch seams ──
    def gw_call(method, kwargs, ret_default=None):
        script = script_obj("gw_windows_calls", "gw_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "gw_windows", "method": method, "kwargs": kwargs}
        if raised:
            e["raised"] = raised
            rec(e)
            raise exc
        ret = script.get("ret", ret_default) if script else ret_default
        e["ret"] = ret
        rec(e)
        return ret

    class WinDouble:
        def is_installed(self):
            return gw_call("is_installed", None)

        def install(self, **kw):
            return gw_call("install", kw)

        def status(self, deep=False):
            return gw_call("status", {"deep": deep})

        def start(self):
            return gw_call("start", None)

        def stop(self):
            return gw_call("stop", None)

        def restart(self):
            return gw_call("restart", None)

        def uninstall(self):
            return gw_call("uninstall", None)

    def gw_windows():
        return WinDouble()

    def systemd_stub(verb):
        def stub(*a, **kw):
            script = script_obj("systemd_calls", "systemd_idx")
            raised, exc = apply_exc(script)
            e = {"ev": "systemd_call", "verb": verb, "kwargs": kw}
            rec_raised(e, raised)
            rec(e)
            if exc:
                raise exc
            return None
        return stub

    def systemd_status_stub(deep=False, system=False, full=False):
        script = script_obj("systemd_status_calls", "systemd_status_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "systemd_status", "deep": deep, "system": system, "full": full}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    def launchd_stub(verb):
        def stub(*a, **kw):
            script = script_obj("launchd_calls", "launchd_idx")
            raised, exc = apply_exc(script)
            e = {"ev": "launchd_call", "verb": verb}
            rec_raised(e, raised)
            rec(e)
            if exc:
                raise exc
        return stub

    def launchd_install_stub(force=False, start_now=True, force_unit_path=False):
        script = script_obj("launchd_install_calls", "launchd_install_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "launchd_install", "force": force, "start_now": start_now,
             "force_unit_path": force_unit_path}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    def launchd_status_stub(deep=False):
        script = script_obj("launchd_status_calls", "launchd_status_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "launchd_status", "deep": deep}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    def install_systemd_cli(args, *, force, system, run_as_user):
        script = script_obj("install_systemd_cli_calls", "install_cli_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "install_systemd_from_cli", "force": force, "system": system,
             "run_as_user": run_as_user}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    def guard_named_profile(force=False):
        script = script_obj("guard_named_profile_calls", "guard_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "guard_named_profile", "force": force}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    def refuse_sessions_live(action, args):
        script = script_obj("refuse_sessions_live_calls", "sessions_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "refuse_sessions_live", "action": action}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    def dispatch_s6(action, profile=None):
        script = script_obj("dispatch_s6_calls", "dispatch_idx")
        raised, exc = apply_exc(script)
        r = bool(script.get("ret", False)) if script else False
        e = {"ev": "dispatch_s6", "action": action}
        if raised:
            e["raised"] = raised
        else:
            e["ret"] = r
        rec(e)
        if exc:
            raise exc
        return r

    def dispatch_all_s6(action):
        script = script_obj("dispatch_all_s6_calls", "dispatch_all_idx")
        raised, exc = apply_exc(script)
        r = bool(script.get("ret", False)) if script else False
        e = {"ev": "dispatch_all_s6", "action": action}
        if raised:
            e["raised"] = raised
        else:
            e["ret"] = r
        rec(e)
        if exc:
            raise exc
        return r

    def reap_orphans():
        script = script_obj("reap_orphans", "reap_idx")
        raised, exc = apply_exc(script)
        rec({"ev": "reap_orphans", "raised": raised})
        if raised == "SystemExit":
            raise exc
        if raised:
            raise RuntimeError(script.get("msg", "boom") if script else "boom")

    def refuse_restart_service_managed(pid):
        script = script_obj("refuse_restart_service_managed", "refuse_restart_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "refuse_restart_of_service_managed_gateway", "pid": pid}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    def start_restart_replacement(args, *, force):
        script = script_obj("start_restart_replacement", "start_repl_idx")
        raised, exc = apply_exc(script)
        e = {"ev": "start_restart_replacement", "force": force}
        rec_raised(e, raised)
        rec(e)
        if exc:
            raise exc

    @contextlib.contextmanager
    def host_scope(owner):
        rec({"ev": "host_scope_enter", "owner": owner_desc(owner)})
        try:
            yield
        finally:
            rec({"ev": "host_scope_exit"})

    @contextlib.contextmanager
    def restart_scope(owner):
        rec({"ev": "restart_all_scope_enter", "owner": owner_desc(owner)})

        def notice():
            rec({"ev": "owe_online_notice"})
        try:
            yield notice
        finally:
            rec({"ev": "restart_all_scope_exit"})

    # ── assign into ns ──
    ns.update({
        "detect_service_manager": detect_service_manager,
        "_is_supervised_gateway_process": is_supervised,
        "get_service_name": get_service_name, "_service_scope_label": service_scope_label,
        "_current_profile_name": current_profile_name, "get_hermes_home": get_hermes_home,
        "get_active_profile_name": active_profile_name, "profile_is_standalone": profile_is_standalone,
        "get_routing_process_hermes_home": routing_home, "named_profile_home": named_profile_home,
        "refuses_container_user_scope_install": refuses_container, "managed_error": managed_error,
        "record_service_choice": record_service_choice, "profile_lifecycle": profile_lifecycle,
        "_host_multiplexer_is_ours": host_is_ours, "find_gateway_pids": find_pids,
        "kill_gateway_processes": kill_processes, "_stop_host_multiplexer": stop_host_multiplexer,
        "report_profile_stop_failure": report_stop_failure, "_wait_for_gateway_exit": wait_gateway_exit,
        "_wait_for_api_server_port_free": wait_port_free, "_discard_dead_host_record": discard_host_record,
        "_forget_host_gateway_probe": forget_host_probe, "run_gateway": run_gateway,
        "get_gateway_runtime_snapshot": get_snapshot, "report_incompatible_owner": report_incompat_owner,
        "print_standalone_by_config": print_standalone_cfg, "_print_served_ingress_urls": print_served_urls,
        "_print_unserved_shared_ingress": print_unserved_urls,
        "_print_multiplex_standalone_reason": print_multiplex_reason,
        "_print_runtime_health": print_runtime_health,
        "_print_duplicate_credential_warnings": print_dup_creds,
        "_print_other_profiles_gateway_status": print_other_profiles,
        "_print_unfolded_gateway_note": print_unfolded_note,
        "_print_gateway_process_mismatch": print_mismatch,
        "_host_multiplexer_for_all_verb": host_owner, "_served_by_another_host_gateway": served_owner,
        "get_systemd_linger_status": get_linger_status,
        "_get_restart_exit_wait_budget": restart_wait_budget,
        "get_process_start_time": process_start_time, "drain_progress_reporter": drain_reporter,
        "_graceful_restart_via_sigusr1": graceful_sigusr1, "leave_restart_pending": leave_restart_pending,
        "_wait_for_systemd_service_restart": wait_systemd_restart,
        "_systemd_service_is_start_limited": systemd_start_limited,
        "_read_systemd_unit_properties": read_unit_props, "get_running_pid": get_running_pid,
        "gateway_declares_external_supervisor": declares_external,
        "restart_externally_supervised_gateway": restart_externally, "getuser": getuser,
        "_gw_windows": gw_windows, "systemd_status": systemd_status_stub,
        "launchd_install": launchd_install_stub, "launchd_status": launchd_status_stub,
        "_install_systemd_from_cli": install_systemd_cli,
        "_guard_named_profile_under_multiplexer": guard_named_profile,
        "refuse_while_sessions_live": refuse_sessions_live,
        "_dispatch_via_service_manager_if_s6": dispatch_s6,
        "_dispatch_all_via_service_manager_if_s6": dispatch_all_s6,
        "_reap_unsupervised_gateway_orphans": reap_orphans,
        "refuse_restart_of_service_managed_gateway": refuse_restart_service_managed,
        "start_restart_replacement": start_restart_replacement,
        "host_scope_for_all_verb": host_scope, "restart_all_verb_scope": restart_scope,
    })
    for verb in ("start", "stop", "restart", "uninstall", "install"):
        ns["systemd_" + verb] = systemd_stub(verb)
    # launchd_install keeps its DEDICATED stub (live _cmd_install calls
    # launchd_install(force, start_now=..., force_unit_path=...) directly);
    # the generic loop used to overwrite it with launchd_stub("install").
    for verb in ("start", "stop", "restart", "uninstall"):
        ns["launchd_" + verb] = launchd_stub(verb)

    # ── seed lazy-import modules ──
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    mod("hermes_cli.gateway_profile_lifecycle", profile_lifecycle=profile_lifecycle,
        host_scope_for_all_verb=host_scope, report_profile_stop_failure=report_stop_failure,
        print_parked_status=ns["print_parked_status"], restart_all_verb_scope=restart_scope)
    mod("hermes_cli.gateway_lifecycle_guards", refuse_while_sessions_live=refuse_sessions_live)
    mod("hermes_cli.gateway_supervised_restart",
        gateway_declares_external_supervisor=declares_external,
        refuse_restart_of_service_managed_gateway=refuse_restart_service_managed,
        restart_externally_supervised_gateway=restart_externally,
        start_restart_replacement=start_restart_replacement)
    mod("gateway.status", get_running_pid=get_running_pid, get_process_start_time=process_start_time,
        terminate_pid=lambda *a, **k: None, _get_process_hermes_home=lambda: "",
        _same_hermes_home=lambda a, b: False, read_runtime_status=lambda: None)
    mod("hermes_cli.gateway_setup_service", record_service_choice=record_service_choice)
    mod("hermes_cli.gateway_status_notes", print_standalone_by_config=print_standalone_cfg,
        report_incompatible_owner=report_incompat_owner)
    mod("hermes_cli.profiles", get_active_profile_name=active_profile_name,
        profile_is_standalone=profile_is_standalone)
    mod("hermes_cli.gateway_multiplex_mode",
        STANDALONE_DEPRECATION_NOTICE=cin("standalone_notice", "NOTICE"))
    mod("hermes_constants", get_routing_process_hermes_home=routing_home,
        named_profile_home=named_profile_home, is_container=ns["is_container"],
        is_termux=ns["is_termux"], is_wsl=ns["is_wsl"])
    mod("hermes_cli.service_manager", detect_service_manager=detect_service_manager)
    mod("tools.process_registry", _is_supervised_gateway_process=is_supervised)
    mod("hermes_cli.update_cmd_drain_report", drain_progress_reporter=drain_reporter,
        leave_restart_pending=leave_restart_pending)
    mod("getpass", getuser=getuser)
    # ensure parent packages exist for `from hermes_cli import ...` style imports
    for pkg in ("hermes_cli", "gateway", "tools"):
        if pkg not in sys.modules:
            sys.modules[pkg] = types.ModuleType(pkg)
    sys.modules["hermes_cli"].gateway_profile_lifecycle = sys.modules["hermes_cli.gateway_profile_lifecycle"]
    sys.modules["gateway"].status = sys.modules["gateway.status"]


def drive(ns, func, inp):
    args = types.SimpleNamespace(**(inp.get("args") or {}))
    if func == "service_backend":
        return {"ret": ns["_service_backend"](windows=inp.get("windows", True))}
    if func == "installed_service_kind_for":
        return {"ret": ns["_installed_service_kind_for"](lambda: inp.get("windows_thunk", False))}
    if func == "installed_service_kind":
        return {"ret": ns["_installed_service_kind"]()}
    if func == "service_call":
        ns["_service_call"](inp.get("backend", "systemd"), inp.get("verb", "start"),
                            inp.get("system"))
        return {"ret": None}
    if func == "stop_installed_service":
        return {"ret": ns["_stop_installed_service"](inp.get("system", False))}
    if func == "profile_name_from_home":
        return {"ret": ns["_profile_name_from_home"](Path(inp.get("home", "")),
                                                     Path(inp.get("default", "")))}
    if func == "unit_environment_value":
        return {"ret": ns["_unit_environment_value"](PathDouble(inp.get("unit_path", "unit.service")),
                                                     inp.get("name", "HERMES_HOME"))}
    if func == "positive_pid":
        return {"ret": ns["_positive_pid"](inp.get("value"))}
    if func == "no_backend_exit":
        ns["_no_backend_exit"](inp.get("subcommand", "install"), inp.get("reason", "unsupported"))
        return {"ret": None}
    if func == "handle_no_backend":
        ns["_handle_no_backend"](inp.get("subcommand", "install"), wsl=inp.get("wsl", False),
                                 s6=inp.get("s6", False))
        return {"ret": None}
    if func == "systemd_graceful_restart_action":
        return {"ret": ns["_systemd_graceful_restart_action"](inp.get("system", False),
                                                              inp.get("pid", 0))}
    if func == "cmd_install":
        ns["_cmd_install"](args)
        return {"ret": None}
    if func == "cmd_uninstall":
        ns["_cmd_uninstall"](args)
        return {"ret": None}
    if func == "cmd_start":
        ns["_cmd_start"](args)
        return {"ret": None}
    if func == "cmd_stop":
        ns["_cmd_stop"](args)
        return {"ret": None}
    if func == "cmd_restart":
        ns["_cmd_restart"](args)
        return {"ret": None}
    if func == "cmd_status":
        ns["_cmd_status"](args)
        return {"ret": None}
    if func == "restart_all":
        ns["_restart_all"](inp.get("system", False), inp.get("detach", False))
        return {"ret": None}
    if func == "restart_all_as_host":
        owner = make_owner(inp.get("owner"))

        def notice():
            rec({"ev": "owe_online_notice"})
        ns["_restart_all_as_host"](owner, inp.get("system", False), inp.get("detach", False),
                                   owe_online_notice=notice)
        return {"ret": None}
    raise ValueError("unknown func: %s" % func)


def main():
    try:
        ns = extract()
        install_stubs(ns)
    except SystemExit:
        raise
    except Exception as e:
        fatal("extraction/stub install failed: %r" % e)
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
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        for k in CUR:
            if k.endswith("_idx"):
                CUR[k] = 0
        # refresh the notice constant per case
        sys.modules["hermes_cli.gateway_multiplex_mode"].STANDALONE_DEPRECATION_NOTICE = \
            inp.get("standalone_notice", "NOTICE")
        raised = None
        exit_code = None
        try:
            want = drive(ns, func, inp)
        except SystemExit as e:
            raised = "SystemExit"
            exit_code = e.code
            want = {"ret": None}
        except subprocess.CalledProcessError:
            raised = "CalledProcessError"
            want = {"ret": None}
        except subprocess.TimeoutExpired:
            raised = "TimeoutExpired"
            want = {"ret": None}
        except KeyError:
            raised = "KeyError"
            want = {"ret": None}
        except OSError:
            raised = "OSError"
            want = {"ret": None}
        except RuntimeError:
            raised = "RuntimeError"
            want = {"ret": None}
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["exit"] = exit_code
        want["raised"] = raised
        want["trace"] = CUR["trace"]
        if want != got:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE gw_service_lifecycle: %d cases, %d mismatches" % (cases, mism))
    if cases == 0:
        fatal("zero cases compared")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())