#!/usr/bin/env python3
"""Oracle for the terminal-tool execution-lifecycle FLOW cores
(src/tools/terminal_tool_lifecycle_flow.c).

Drives the LIVE functions with SCRIPTED process/config seams: the functions are
AST-extracted from the quarry (tools/terminal_tool.py + terminal_tool_background/
result/config splits, no heavy import), their module constants and @dataclass
verdict/plan types are seeded, and every env.execute / registry-spawn / approval /
cwd / redaction / truncation / logging / sleep dependency is replaced by a
recording monkeypatch stub returning the same scripted results the C harness
feeds its seams. Compares the DECISION SEQUENCE (ordered trace of seam calls)
plus the return value / mutated session state, not just a final scalar.

Input: stdin JSON lines from t_port_terminal_lifecycle_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE terminal_lifecycle_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import json
import os
import sys
import types
import logging
import functools
import posixpath
import re
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any, Optional, overload

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": []}
LIVE = {}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


class RecLogger:
    LEVELS = {10: "debug", 20: "info", 30: "warning", 40: "error"}

    def _log(self, level, fmt, *args, **kw):
        if isinstance(level, int):
            level = self.LEVELS.get(level, str(level))
        rec({"ev": "log", "level": level, "msg": fmt % args if args else fmt})

    def debug(self, fmt, *args, **kw): self._log("debug", fmt, *args, **kw)
    def info(self, fmt, *args, **kw): self._log("info", fmt, *args, **kw)
    def warning(self, fmt, *args, **kw): self._log("warning", fmt, *args, **kw)
    def error(self, fmt, *args, **kw): self._log("error", fmt, *args, **kw)
    def log(self, level, fmt, *args, **kw): self._log(level, fmt, *args, **kw)


def extract(rel, wanted, ns, consts=True):
    p = os.path.join(QUARRY, rel)
    tree = ast.parse(open(p, encoding="utf-8").read())
    if consts:
        for node in tree.body:
            tg = []
            if isinstance(node, ast.Assign):
                tg = [t.id for t in node.targets if isinstance(t, ast.Name)]
            elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
                tg = [node.target.id]
            if tg:
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec"), ns)
                except Exception:
                    pass
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef)) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec"), ns)
    return ns


def build_ns():
    ns = {"__name__": "terminal_lifecycle_extracted", "json": json, "logger": RecLogger(),
          "dataclass": dataclass, "Any": Any, "Optional": Optional, "overload": overload,
          "posixpath": posixpath, "re": re, "contextmanager": contextmanager,
          "functools": functools, "logging": logging, "sys": sys, "os": os}
    extract("tools/terminal_tool.py", {
        "_error_json", "_Rejected", "_ApprovalVerdict", "_ExecPlan",
        "_run_approval_guards", "_with_promoted_note", "_terminal_deadline_floor",
        "_degraded_result", "_handle_terminal", "_pre_exec_block", "_run_foreground",
        "_plan_execution", "_yield_kwargs", "_resolve_notification_flag_conflict",
    }, ns)
    extract("tools/terminal_tool_background.py", {
        "spawn_background_process", "_looks_like_homebrew_ci_poller", "_spawn",
    }, ns)
    extract("tools/terminal_tool_result.py", {
        "finalize_foreground_result", "_cwd_fallback_note", "_interpret_exit_code",
    }, ns)
    extract("tools/terminal_tool_config.py", {
        "_host_path_key", "translate_mounted_host_path", "coerce_ssh_remote_cwd", "_quiet",
    }, ns)
    extract("tools/registry.py", {"tool_error", "_bound_error_text"}, ns, consts=False)
    ns["_MAX_TOOL_ERROR_CHARS"] = 2048
    ns["_MAX_LOGGED_ERROR_CHARS"] = 8192
    ns["_TOOL_ERROR_TRUNCATION_MARKER"] = "\u2026 [truncated]"
    extract("tools/terminal_tool_guards.py", {"_safe_command_preview"}, ns)
    ns["FOREGROUND_MAX_TIMEOUT"] = 600
    ns["time"] = types.SimpleNamespace(
        sleep=lambda s: rec({"ev": "sleep", "seconds": s}), time=lambda: 0.0)
    return ns


def fake_module(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts) + 1):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            sys.modules[pn] = types.ModuleType(pn)
    m = sys.modules[name]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


def ns_obj(d):
    return None if d is None else types.SimpleNamespace(**d)


def env_data(env):
    if env is None:
        return None
    return {k: v for k, v in vars(env).items() if not callable(v)}


def mk_exc(name, msg):
    return type(name, (Exception,), {})(msg)


def install_stubs(ns):
    live_cwd_note = ns["_cwd_fallback_note"]
    live_flag_conflict = ns["_resolve_notification_flag_conflict"]
    for k in ("finalize_foreground_result", "_interpret_exit_code", "coerce_ssh_remote_cwd",
              "_run_approval_guards", "_error_json", "_with_promoted_note",
              "_terminal_deadline_floor", "_cwd_fallback_note", "_host_path_key",
              "translate_mounted_host_path", "_looks_like_homebrew_ci_poller",
              "_plan_execution", "_run_foreground", "_pre_exec_block", "_degraded_result",
              "_handle_terminal", "spawn_background_process"):
        LIVE[k] = ns[k]

    # ── fake modules for the targets' function-local imports ──
    fake_module("tools.terminal_tool",
                _redact_terminal_error_text=lambda t: t,
                _resolve_command_cwd=None,
                _resolve_notification_flag_conflict=live_flag_conflict,
                record_session_cwd=None)
    fake_module("tools.approval_provenance",
                preapproval_note=lambda pa: rec({"ev": "preapproval_note", "pre_approved": pa,
                                                  "ret": cin("preapproval_note_ret", "")}) or cin("preapproval_note_ret", ""))
    fake_module("tools.interrupt",
                clear_current_thread_interrupt=lambda: rec({"ev": "clear_current_thread_interrupt"}))

    def enforce():
        if cin("refusal_raises", False):
            rec({"ev": "enforce_no_refusal", "raised": True})
            raise RuntimeError(cin("refusal_msg", "refusal scope"))
        rec({"ev": "enforce_no_refusal", "raised": False})
    fake_module("tools.terminal_scope", enforce_no_refusal=enforce)

    fake_module("tools.environments.local",
                _msys_to_windows_path=lambda p: rec({"ev": "msys_to_windows_path", "path": p,
                                                      "ret": cin("msys_path_ret", p)}) or cin("msys_path_ret", p),
                LocalEnvironment=type("LocalEnvironment", (), {}))
    fake_module("tools.environments.sandbox_scope", hosted_scope=lambda: cin("hosted_scope", None))
    fake_module("tools.terminal_tool_result", _cwd_fallback_note=live_cwd_note)
    fake_module("tools.ansi_strip",
                strip_ansi=lambda o: rec({"ev": "strip_ansi", "output": o,
                                          "ret": cin("strip_ansi_ret", o)}) or cin("strip_ansi_ret", o))
    fake_module("agent.redact",
                redact_terminal_output=lambda o, c: rec({"ev": "redact_output", "output": o,
                                                          "command": c, "ret": cin("redact_output_ret", o)}) or cin("redact_output_ret", o))
    fake_module("agent.terminal_approval_batch",
                validate_prepared_terminal=lambda args: rec({"ev": "validate_prepared_terminal", "args": args}))
    fake_module("agent.delegation_context",
                is_delegated_child_context=lambda: rec({"ev": "is_delegated_child_context",
                                                         "ret": cin("delegated_child", False)}) or cin("delegated_child", False))
    fake_module("gateway.session_context",
                async_delivery_supported=lambda: cin("async_delivery", True),
                get_session_env=lambda var, default="": cin("session_env", {}).get(var, default))
    fake_module("hermes_cli.observability.shared_metrics_harness",
                record_terminal_outcome=lambda command, env_type, result:
                rec({"ev": "record_terminal_outcome", "command": command,
                     "env_type": env_type, "result": result}))
    fake_module("hermes_constants",
                get_subprocess_home=lambda: rec({"ev": "get_subprocess_home",
                                                  "ret": cin("subprocess_home")}) or cin("subprocess_home"),
                get_real_home=lambda: rec({"ev": "get_real_home",
                                            "ret": cin("real_home")}) or cin("real_home"))

    # ── process_registry double ──
    class FakeRegistry:
        def __init__(self):
            self.pending_watchers = []

        def _spawn_rec(self, branch, kwargs):
            sess = cin("proc_session")
            rec({"ev": "spawn", "branch": branch, "kwargs": kwargs,
                 "ret": sess, "exc_msg": cin("spawn_exc")})
            if cin("spawn_exc"):
                raise RuntimeError(cin("spawn_exc"))
            return types.SimpleNamespace(**{"watcher_platform": "", **sess})

        def spawn_local(self, **kwargs):
            return self._spawn_rec("local", kwargs)

        def spawn_via_env(self, **kwargs):
            return self._spawn_rec("env", kwargs)

        def arm_heartbeat(self, proc_session, seconds):
            r = cin("arm_heartbeat_ret", seconds)
            rec({"ev": "arm_heartbeat", "session_id": proc_session.id,
                 "heartbeat_seconds": seconds, "ret": r})
            return r
    fake_module("tools.process_registry", process_registry=FakeRegistry())

    # ── module-global seams in the extracted namespaces ──
    ns["logger"] = RecLogger()
    ns["_docker_has_host_access"] = lambda config: rec({"ev": "docker_has_host_access",
                                                         "config": config, "ret": cin("host_access", False)}) or cin("host_access", False)

    def check_guards(command, env_type, has_host_access=False):
        r = cin("approval", {"approved": True})
        rec({"ev": "check_all_guards", "command": command, "env_type": env_type,
             "has_host_access": has_host_access, "ret": r})
        return r
    ns["_check_all_guards"] = check_guards
    ns["_get_env_config"] = lambda: rec({"ev": "get_env_config", "ret": cin("config", {})}) or cin("config", {})
    ns["_resolve_container_task_id"] = lambda tid: rec({"ev": "resolve_container_task_id", "task_id": tid,
                                                         "ret": cin("container_task_id", tid or "default")}) or cin("container_task_id", tid or "default")
    ns["resolve_task_overrides"] = lambda tid: rec({"ev": "resolve_task_overrides", "task_id": tid,
                                                     "ret": cin("overrides", {})}) or cin("overrides", {})
    ns["_select_image"] = lambda et, ov, cfg: rec({"ev": "select_image", "env_type": et, "overrides": ov,
                                                    "config": cfg, "ret": cin("image", "")}) or cin("image", "")
    ns["get_session_cwd"] = lambda sk: rec({"ev": "get_session_cwd", "session_key": sk,
                                             "ret": cin("session_cwd")}) or cin("session_cwd")
    ns["coerce_ssh_remote_cwd"] = lambda cwd, et: rec({"ev": "coerce_ssh_remote_cwd", "cwd": cwd,
                                                        "env_type": et, "ret": cin("coerced_cwd", cwd)}) or cin("coerced_cwd", cwd)
    ns["_resolve_task_host_cwd"] = lambda cfg, tid: rec({"ev": "resolve_task_host_cwd", "config": cfg,
                                                          "task_id": tid, "ret": cin("host_cwd")}) or cin("host_cwd")
    ns["_is_container_backend"] = lambda et: rec({"ev": "is_container_backend", "env_type": et,
                                                   "ret": cin("is_container", False)}) or cin("is_container", False)
    ns["_is_unusable_container_cwd"] = lambda cwd, mounted_host=None: rec({"ev": "is_unusable_container_cwd",
                                                                            "cwd": cwd, "mounted_host": mounted_host,
                                                                            "ret": cin("unusable_cwd", False)}) or cin("unusable_cwd", False)
    ns["trailing_amp_background_command"] = lambda c: rec({"ev": "trailing_amp_background_command",
                                                            "command": c, "ret": cin("auto_bg")}) or cin("auto_bg")
    ns["_foreground_background_guidance"] = lambda c: rec({"ev": "foreground_background_guidance",
                                                            "command": c, "ret": cin("guidance")}) or cin("guidance")
    ns["_redact_terminal_error_text"] = lambda t: rec({"ev": "redact", "text": t,
                                                        "ret": cin("redact_ret", t)}) or cin("redact_ret", t)
    ns["_tenv"] = lambda name, default="": rec({"ev": "tenv", "name": name, "default": default,
                                                 "ret": cin("tenv_ret", default)}) or cin("tenv_ret", default)
    ns["_fatal_error_json"] = lambda e: rec({"ev": "fatal_error_json",
                                              "ret": cin("fatal_ret", "{}")}) or cin("fatal_ret", "{}")

    def evict(tid):
        raised = cin("evict_raises", False)
        rec({"ev": "evict_environment_for_task", "task_id": tid, "raised": raised})
        if raised:
            raise RuntimeError("eviction failed")
    ns["_evict_environment_for_task"] = evict
    ns["terminal_tool"] = lambda **kw: rec({"ev": "terminal_tool", "kwargs": kw,
                                             "ret": cin("terminal_tool_ret", "{}")}) or cin("terminal_tool_ret", "{}")

    def parse_env(name, default):
        if cin("parse_env_raises", False):
            rec({"ev": "parse_env_var", "name": name, "default": default, "ret": None, "raised": True})
            raise ValueError("bad env")
        r = cin("parse_env_ret", int(default))
        rec({"ev": "parse_env_var", "name": name, "default": default, "ret": r, "raised": False})
        return r
    ns["_parse_env_var"] = parse_env
    ns["_is_container_launch_dir"] = lambda p: rec({"ev": "is_container_launch_dir", "path": p,
                                                     "ret": cin("launch_dir", False)}) or cin("launch_dir", False)
    ns["_interpret_signal_exit"] = lambda ec: rec({"ev": "interpret_signal_exit", "exit_code": ec,
                                                    "ret": cin("signal_note")}) or cin("signal_note")

    # pre-exec seams
    def glb(*, command, env, env_type, cwd, workdir, session_key):
        r = cin("lifecycle_block")
        rec({"ev": "gateway_lifecycle_block", "command": command, "env_type": env_type, "cwd": cwd,
             "workdir": workdir, "session_key": session_key, "ret": r})
        return r
    ns["gateway_lifecycle_block"] = glb
    ns["_validate_workdir"] = lambda w: rec({"ev": "validate_workdir", "workdir": w,
                                              "ret": cin("workdir_error")}) or cin("workdir_error")

    def refuse_local(workdir, env):
        r = cin("refuse_local_reject")
        rec({"ev": "refuse_missing_local_workdir", "workdir": workdir, "env": env_data(env), "ret": r})
        if r:
            raise ns["_Rejected"](r)
    ns["_refuse_missing_local_workdir"] = refuse_local

    def srb(*, command, cwd, workdir, session_key):
        r = cin("self_repo_block")
        rec({"ev": "self_repo_block", "command": command, "cwd": cwd, "workdir": workdir,
             "session_key": session_key, "ret": r})
        return r
    ns["self_repo_block"] = srb

    def lsb(*, command, cwd, workdir, session_key):
        r = cin("locked_skill_block")
        rec({"ev": "locked_skill_block", "command": command, "cwd": cwd, "workdir": workdir,
             "session_key": session_key, "ret": r})
        return r
    ns["locked_skill_block"] = lsb

    # foreground seams
    def rcc(*, workdir, default_cwd, session_key=None, env_type=None, mounted_host=None, env=None):
        r = cin("command_cwd", default_cwd)
        rec({"ev": "resolve_command_cwd", "workdir": workdir, "default_cwd": default_cwd,
             "session_key": session_key, "env_type": env_type, "mounted_host": mounted_host,
             "env": env_data(env), "ret": r})
        return r
    ns["_resolve_command_cwd"] = rcc
    sys.modules["tools.terminal_tool"]._resolve_command_cwd = rcc

    def ybh(*, command, env_type, cwd, effective_task_id, task_id, session_key):
        r = cin("yield_handler", False)
        rec({"ev": "yield_to_background_handler", "command": command, "env_type": env_type, "cwd": cwd,
             "effective_task_id": effective_task_id, "task_id": task_id, "session_key": session_key, "ret": r})
        return (lambda proc, out: {}) if r else None
    ns["yield_to_background_handler"] = ybh

    def finalize(*, command, result, env, env_type, effective_task_id, task_id, session_id,
                 session_key, workdir, command_cwd, approval_note):
        r = cin("finalize_ret", '{"finalized":true}')
        rec({"ev": "finalize_foreground_result", "command": command, "result": result,
             "env": env_data(env), "env_type": env_type, "effective_task_id": effective_task_id,
             "task_id": task_id, "session_id": session_id, "session_key": session_key,
             "workdir": workdir, "command_cwd": command_cwd, "approval_note": approval_note, "ret": r})
        return r
    ns["finalize_foreground_result"] = finalize

    # background seams
    def aas(proc_session, result_data, notify_on_complete, watch_patterns):
        rn = cin("async_notify", notify_on_complete)
        rw = cin("async_watch", watch_patterns)
        rec({"ev": "apply_async_support", "session_id": proc_session.id,
             "notify_on_complete": notify_on_complete, "watch_patterns": watch_patterns,
             "ret_notify": rn, "ret_watch": rw})
        result_data.update(cin("async_result_patch", {}))
        for k, v in cin("async_session_patch", {}).items():
            setattr(proc_session, k, v)
        return rn, rw
    ns["_apply_async_support"] = aas

    def rcw(process_registry, proc_session, session_key):
        rec({"ev": "register_completion_watcher", "session_id": proc_session.id, "session_key": session_key})
        proc_session.watcher_interval = 5
    ns["_register_completion_watcher"] = rcw

    # finalize seams
    def rec_cwd(session_key, cwd):
        rec({"ev": "record_session_cwd", "session_key": session_key, "cwd": cwd})
    sys.modules["tools.terminal_tool"].record_session_cwd = rec_cwd
    ns["record_session_cwd"] = rec_cwd

    def sudo_ann(command, output, env_type):
        r = cin("sudo_ann", {"output": output, "auth_failed": False, "cache_cleared": False})
        rec({"ev": "sudo_annotations", "command": command, "output": output, "env_type": env_type,
             "ret_output": r["output"], "auth_failed": r["auth_failed"], "cache_cleared": r["cache_cleared"]})
        return r["output"], r["auth_failed"], r["cache_cleared"]
    ns["_sudo_annotations"] = sudo_ann

    def transform(command, output, returncode, task_id, env_type):
        raised = cin("transform_raises", False)
        r = cin("transform_ret", output)
        rec({"ev": "transform_hook", "command": command, "output": output, "returncode": returncode,
             "task_id": task_id, "env_type": env_type, "ret": r, "raised": raised})
        if raised:
            rec({"ev": "log", "level": "debug", "msg": "transform_terminal_output hook"})
            return output
        return r
    ns["_apply_output_transform_hook"] = transform
    ns["_truncate_head_tail"] = lambda o: rec({"ev": "truncate", "output": o,
                                                "ret": cin("truncate_ret", o)}) or cin("truncate_ret", o)

    def iec(command, exit_code):
        r = cin("exit_note")
        rec({"ev": "interpret_exit_code", "command": command, "exit_code": exit_code, "ret": r})
        return r
    ns["_interpret_exit_code"] = iec

    def fhint(command, returncode, output, exit_note):
        raised = cin("failure_hint_raises", False)
        r = cin("failure_hint")
        rec({"ev": "failure_hint", "command": command, "returncode": returncode, "output": output,
             "exit_note": exit_note, "ret": r, "raised": raised})
        if raised:
            rec({"ev": "log", "level": "debug", "msg": "failure hint"})
            return None
        return r
    ns["_failure_hint"] = fhint

    def realp(p):
        raised = cin("realpath_raises", False)
        r = cin("realpaths", {}).get(p, p)
        rec({"ev": "realpath", "path": p, "ret": r, "raised": raised})
        if raised:
            raise OSError("realpath failed")
        return r
    ns["os"] = types.SimpleNamespace(path=types.SimpleNamespace(realpath=realp))

    def spill(path, total_chars, command):
        r = cin("spill_pairs", [])
        rec({"ev": "redact_spill", "path": path, "total_chars": total_chars, "command": command, "ret": r})
        return [tuple(p) for p in r]
    ns["_redact_spill_file"] = spill

    def vevidence(command, cwd, session_id, returncode, output, pipeline_status=None):
        raised = cin("verification_raises", False)
        r = cin("verification")
        rec({"ev": "verification_evidence", "command": command, "cwd": cwd, "session_id": session_id,
             "returncode": returncode, "output": output, "pipeline_status": pipeline_status,
             "ret": r, "raised": raised})
        if raised:
            rec({"ev": "log", "level": "debug", "msg": "verification evidence recording"})
            return None
        return r
    ns["_verification_evidence"] = vevidence
    return ns


# ── drive the LIVE target for one case ──
def drive(ns, func, inp):
    if func == "error_json":
        r = LIVE["_error_json"](inp.get("error", ""), exit_code=inp.get("exit_code", -1),
                                status=inp.get("status"), **inp.get("extra", {}))
        return {"ret": r}
    if func == "run_approval_guards":
        try:
            v = LIVE["_run_approval_guards"](inp.get("command", "ls"), inp.get("env_type", "local"),
                                             inp.get("config", {}), force=inp.get("force", False))
            return {"ret": {"note": v.note, "approved_run": v.approved_run}, "rejected": None}
        except ns["_Rejected"] as r:
            return {"ret": None, "rejected": r.result_json}
    if func == "pre_exec_block":
        env = ns_obj(inp.get("env"))
        try:
            LIVE["_pre_exec_block"](inp.get("command", "ls"), env=env, env_type=inp.get("env_type", "local"),
                                    cwd=inp.get("cwd", "/w"), workdir=inp.get("workdir"),
                                    session_key=inp.get("session_key", "sk"))
            return {"ret": None, "rejected": None}
        except ns["_Rejected"] as r:
            return {"ret": None, "rejected": r.result_json}
    if func == "plan_execution":
        cmd = inp.get("command")
        pt = inp.get("command_py_type")
        if pt == "NoneType":
            cmd = None
        elif pt == "bool":
            cmd = bool(cmd)
        elif pt == "int":
            cmd = int(cmd)
        elif pt == "list":
            cmd = list(cmd)
        elif pt == "dict":
            cmd = dict(cmd)
        try:
            plan = LIVE["_plan_execution"](cmd, task_id=inp.get("task_id"), timeout=inp.get("timeout"),
                                           background=inp.get("background", False),
                                           _host_local=inp.get("host_local", False))
            return {"ret": vars(plan), "rejected": None, "exception": None}
        except ns["_Rejected"] as r:
            return {"ret": None, "rejected": r.result_json, "exception": None}
        except Exception as e:
            return {"ret": None, "rejected": None, "exception": str(e)}
    if func == "with_promoted_note":
        return {"ret": LIVE["_with_promoted_note"](inp.get("result_json", ""), inp.get("requested_timeout", 900))}
    if func == "run_foreground":
        env_attrs = dict(inp.get("env", {}))
        scripts = inp.get("execute_results", [])
        state = {"i": 0}

        def execute(command, **kw):
            i = state["i"]
            state["i"] += 1
            sc = scripts[i] if i < len(scripts) else {}
            rec({"ev": "env_execute", "attempt": i, "command": command,
                 "timeout": kw.get("timeout"), "cwd": kw.get("cwd"),
                 "bounded_capture": kw.get("bounded_capture"),
                 "allow_cwd_fallback": kw.get("allow_cwd_fallback"),
                 "yield_handler": "yield_handler" in kw,
                 "ret": sc.get("ret"), "exc_type": sc.get("exc_type"), "exc_msg": sc.get("exc_msg")})
            if sc.get("exc_type"):
                raise mk_exc(sc["exc_type"], sc.get("exc_msg", ""))
            return sc.get("ret", {})
        env = types.SimpleNamespace(execute=execute, **env_attrs)
        plan = types.SimpleNamespace(env_type=inp.get("env_type", "local"),
                                     effective_task_id=inp.get("effective_task_id", "t1"),
                                     effective_timeout=inp.get("effective_timeout", 180),
                                     cwd=inp.get("plan_cwd", "/w"), host_cwd=inp.get("plan_host_cwd"))
        r = LIVE["_run_foreground"](inp.get("command", "ls"), env, plan,
                                    task_id=inp.get("task_id"), session_id=inp.get("session_id"),
                                    session_key=inp.get("session_key", "sk"), cwd_key=inp.get("cwd_key", "sk"),
                                    workdir=inp.get("workdir"), approval_note=inp.get("approval_note"),
                                    clear_interrupt=inp.get("clear_interrupt", False),
                                    metered=inp.get("metered", True))
        return {"ret": r}
    if func == "terminal_deadline_floor":
        return {"ret": LIVE["_terminal_deadline_floor"](inp.get("args", {}))}
    if func == "degraded_result":
        e = types.SimpleNamespace(reason=inp.get("reason", "ssh down"), retry_hint=inp.get("retry_hint", "retry"))
        return {"ret": LIVE["_degraded_result"](e, inp.get("task_id"))}
    if func == "handle_terminal":
        args = dict(inp.get("args", {}))
        if "heartbeat" in args and inp.get("heartbeat_py_type") == "float":
            args["heartbeat"] = float(args["heartbeat"])
        r = LIVE["_handle_terminal"](args, task_id=inp.get("task_id"), session_id=inp.get("session_id"))
        return {"ret": r}
    if func == "spawn_background":
        r = LIVE["spawn_background_process"](
            command=inp.get("command", "sleep 1"), env=ns_obj(inp.get("env")),
            env_type=inp.get("env_type", "local"), effective_task_id=inp.get("effective_task_id", "t1"),
            task_id=inp.get("task_id"), session_key=inp.get("session_key", "sk"),
            workdir=inp.get("workdir"), cwd=inp.get("cwd", "/w"),
            effective_pty=inp.get("effective_pty", False), cwd_key=inp.get("cwd_key"),
            notify_on_complete=inp.get("notify_on_complete", False),
            watch_patterns=inp.get("watch_patterns"), approval_note=inp.get("approval_note"),
            completion_output_chars=inp.get("completion_output_chars", 0),
            pty_disabled_reason=inp.get("pty_disabled_reason"),
            heartbeat_seconds=inp.get("heartbeat_seconds", 0),
            persist_on_release=inp.get("persist_on_release", False),
            mounted_host=inp.get("mounted_host"), auto_backgrounded=inp.get("auto_backgrounded", False))
        return {"ret": r, "session": None}
    if func == "finalize_foreground":
        env = ns_obj(inp.get("env"))
        r = LIVE["finalize_foreground_result"](
            command=inp.get("command", "ls"), result=inp.get("result", {}), env=env,
            env_type=inp.get("env_type", "local"), effective_task_id=inp.get("effective_task_id", "t1"),
            task_id=inp.get("task_id"), session_id=inp.get("session_id"),
            session_key=inp.get("session_key", "sk"), workdir=inp.get("workdir"),
            command_cwd=inp.get("command_cwd"), approval_note=inp.get("approval_note"))
        return {"ret": r}
    if func == "cwd_fallback_note":
        return {"ret": LIVE["_cwd_fallback_note"](inp.get("fallback"))}
    if func == "interpret_exit_code":
        return {"ret": LIVE["_interpret_exit_code"](inp.get("command", ""), inp.get("exit_code", 0))}
    if func == "host_path_key":
        return {"ret": LIVE["_host_path_key"](inp.get("path"))}
    if func == "translate_mounted_host_path":
        return {"ret": LIVE["translate_mounted_host_path"](inp.get("path"), inp.get("host_root"),
                                                           inp.get("container_root"))}
    if func == "coerce_ssh_remote_cwd":
        return {"ret": LIVE["coerce_ssh_remote_cwd"](inp.get("cwd"), inp.get("env_type"))}
    if func == "looks_like_homebrew_ci_poller":
        return {"ret": LIVE["_looks_like_homebrew_ci_poller"](inp.get("command", ""))}
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    if func == "spawn_background":
        reg = sys.modules["tools.process_registry"].process_registry
        orig = reg._spawn_rec
        holder = {"obj": None}

        def cap(branch, kwargs):
            s = orig(branch, kwargs)
            holder["obj"] = s
            return s
        reg._spawn_rec = cap
        try:
            out = drive(ns, func, inp)
        finally:
            reg._spawn_rec = orig
        out["session"] = vars(holder["obj"]) if holder["obj"] is not None else None
    else:
        out = drive(ns, func, inp)
    out["trace"] = CUR["trace"]
    return out


# ── comparison ──
def canon(x, ordered):
    if isinstance(x, dict):
        items = [(k, canon(v, ordered)) for k, v in x.items()]
        if not ordered:
            items = sorted(items, key=lambda kv: kv[0])
        return ("obj", items)
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v, ordered) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def json_ret_equal(wv, gv):
    """Envelope rets are JSON strings: parse both, compare key-ORDERED (insertion
    order is semantic). Non-JSON strings (promotion pass-through) compare raw."""
    try:
        wp, gp = json.loads(wv), json.loads(gv)
    except (ValueError, TypeError):
        return wv == gv
    return canon(wp, True) == canon(gp, True)


def compare(func, want, got):
    if canon(want.get("trace", []), False) != canon(got.get("trace", []), False):
        return False
    for k in set(want) | set(got):
        if k == "trace":
            continue
        wv, gv = want.get(k), got.get(k)
        if k in ("ret", "rejected") and isinstance(wv, str) and isinstance(gv, str):
            if not json_ret_equal(wv, gv):
                return False
            continue
        if k == "ret" and func in ("plan_execution", "run_approval_guards") \
                and isinstance(wv, dict) and isinstance(gv, dict):
            if canon(wv, True) != canon(gv, True):
                return False
            continue
        if canon(wv, False) != canon(gv, False):
            return False
    return True


def main():
    ns = build_ns()
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
        try:
            want = run_case(ns, func, inp)
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if not compare(func, want, got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE terminal_lifecycle_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
