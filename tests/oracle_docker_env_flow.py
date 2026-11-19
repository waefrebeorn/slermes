#!/usr/bin/env python3
"""Oracle for the Docker container-lifecycle FLOW cores
(src/tools/environments/docker_env_flow.c).

Drives the LIVE tools/environments/docker.py functions/methods with SCRIPTED
seams: the targets are AST-extracted from the quarry (no heavy import), module
constants are seeded (Assign + AnnAssign, plain-Name targets only), and every
docker CLI capture / subprocess.run / uuid / os.path probe / makedirs /
sandbox-dir discovery / foreign-module helper / logger emission is replaced by
a recording stub returning the same scripted results the C harness feeds its
universal seam. Compares the DECISION SEQUENCE (ordered {ev,args,ret} trace)
plus the return value, propagated exception and mutated self state.

Input: stdin JSON lines from t_port_docker_env_flow:
    {"func": ..., "in": {...}, "out": {"ret","exc","trace"[,"self"]}}
Summary: ORACLE docker_env_flow: N cases, M mismatches. Exit 1 on any mismatch,
and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import builtins
import copy
import json
import logging
import os
import re
import subprocess
import sys
import threading
import types
import uuid as _uuid_mod
from pathlib import PurePosixPath

for _stream in (sys.stdout, sys.stdin):
    try:
        if hasattr(_stream, "reconfigure"):
            _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

QUARRY = r"C:\Users\eman5\hermes-agent"
import __future__
ANN_FLAG = __future__.annotations.compiler_flag

CUR = {"func": None, "in": {}, "trace": [], "counts": {}, "state_out": {}}
LIVE = {}
REAL = {}


def rec(ev):
    CUR["trace"].append(ev)


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def mk_exc(kind, msg):
    """Real exception instance with str(e) == msg and the LIVE class identity
    for except-clause matching. subprocess kinds are real subclasses (so
    isinstance(e, subprocess.TimeoutExpired / SubprocessError) holds) with a
    controlled __str__; the BaseException trio are the real builtins so they
    pass through every `except Exception`-shaped site."""
    if kind == "TimeoutExpired":
        cls = type("TimeoutExpired", (subprocess.TimeoutExpired,),
                   {"__str__": lambda self: msg})
        return cls("cmd", 1)
    if kind == "CalledProcessError":
        cls = type("CalledProcessError", (subprocess.CalledProcessError,),
                   {"__str__": lambda self: msg})
        return cls(1, "cmd")
    if kind == "SubprocessError":
        return subprocess.SubprocessError(msg)
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


def consume(name):
    """Script for a seam: single value reused, bare array consumed per call,
    or {"__script__":true,"ret":X} constant (list) result."""
    io = CUR["in"].get("io", {}) or {}
    script = io.get(name)
    if isinstance(script, dict) and script.get("__script__") is True:
        return script.get("ret")
    if isinstance(script, list):
        i = CUR["counts"].get(name, 0)
        CUR["counts"][name] = i + 1
        if not script:
            return None
        return script[min(i, len(script) - 1)]
    return script


def record(name, args, script):
    rec({"ev": name, "args": args, "ret": script})


def maybe_raise(script):
    if is_raise(script):
        raise mk_exc(script["__raise__"], script.get("msg", ""))


def exc_norm(a):
    return {"__exc__": type(a).__name__, "msg": str(a)}


# ── AST extraction ──
def extract_consts(rel, ns):
    p = os.path.join(QUARRY, rel)
    src = open(p, encoding="utf-8").read()
    tree = ast.parse(src)
    for node in tree.body:
        tg = []
        if isinstance(node, ast.Assign):
            tg = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            tg = [node.target.id]
        if tg:
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), p, "exec", ANN_FLAG), ns)
            except Exception:
                pass
    return tree, src, p


def extract_funcs(tree, p, wanted, ns):
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted:
            mm = copy.copy(node)
            mm.decorator_list = []
            exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)


def extract_methods(tree, p, class_name, wanted, ns):
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == class_name:
            for m in node.body:
                if isinstance(m, (ast.FunctionDef, ast.AsyncFunctionDef)) and m.name in wanted:
                    mm = copy.copy(m)
                    mm.decorator_list = []
                    exec(compile(ast.Module(body=[mm], type_ignores=[]), p, "exec", ANN_FLAG), ns)


MODULE_TARGETS = [
    "_docker_query", "_is_idle_workspace_container", "_extra_args_network_mode",
    "_split_volume_spec", "_container_mount_taken", "_existing_host_mount",
    "_free_host_cwd_mount", "_abs_host_cwd", "_host_user_args",
    "_name_only_env_args",
]
METHOD_TARGETS = [
    "_attach_existing_container", "_image_available_locally", "_start_container",
    "_docker_run", "_run_command", "_resource_args", "_mount_args",
    "_egress_and_env_args", "_container_image", "_docker_client_env",
]


def build_ns():
    mod = types.ModuleType("docker_env_flow_extracted")
    sys.modules["docker_env_flow_extracted"] = mod
    ns = mod.__dict__
    ns.update({"__name__": "docker_env_flow_extracted", "json": json, "os": os,
               "re": re, "sys": sys, "subprocess": subprocess, "threading": threading,
               "logging": logging, "uuid": _uuid_mod, "PurePosixPath": PurePosixPath})
    tree, src, p = extract_consts("tools/environments/docker.py", ns)
    extract_funcs(tree, p, set(MODULE_TARGETS), ns)
    extract_methods(tree, p, "DockerEnvironment", set(METHOD_TARGETS), ns)
    for nm in MODULE_TARGETS + METHOD_TARGETS:
        if nm not in ns:
            raise RuntimeError("extraction missing %s" % nm)
        LIVE[nm] = ns[nm]
        REAL[nm] = ns[nm]
    for nm in ("_NETWORK_FLAGS", "_VOLUME_SPEC_RE", "_HOST_CWD_FALLBACK_MOUNTS",
               "_DEFAULT_PIDS_LIMIT", "_WORKSPACE_LABEL_KEY"):
        if nm not in ns:
            raise RuntimeError("const seeding missing %s" % nm)
    return ns


# ── recording stubs ──
def rec_fn(seam_name, argmap):
    def f(*args, **kwargs):
        a = argmap(*args, **kwargs)
        sc = consume(seam_name)
        record(seam_name, drop_none(a), sc)
        maybe_raise(sc)
        return sc
    return f


class RecLogger:
    """logger.<level>(msg, *args) — args recorded VERBATIM (format specifiers
    apply only at emission); exception args normalized like the C side."""
    def _emit(self, level, msg, *args, **kwargs):
        norm = [exc_norm(a) if isinstance(a, BaseException) else a for a in args]
        sc = consume("log_" + level)
        record("log_" + level, {"msg": msg, "args": norm}, sc)
        maybe_raise(sc)

    def debug(self, msg, *a, **k): self._emit("debug", msg, *a, **k)
    def info(self, msg, *a, **k): self._emit("info", msg, *a, **k)
    def warning(self, msg, *a, **k): self._emit("warning", msg, *a, **k)
    def error(self, msg, *a, **k): self._emit("error", msg, *a, **k)


def stub_run_capture(cmd, *, timeout, check=False, env=None):
    sc = consume("run_capture")
    record("run_capture", drop_none({"argv": cmd, "timeout": timeout, "check": check, "env": env}), sc)
    maybe_raise(sc)
    if isinstance(sc, dict):
        return subprocess.CompletedProcess(args=cmd, returncode=sc.get("returncode"),
                                           stdout=sc.get("stdout"), stderr=sc.get("stderr"))
    return sc


def stub_subprocess_run(argv, capture_output=False, timeout=None, stdin=None, check=False):
    sc = consume("subprocess_run")
    record("subprocess_run", drop_none({"argv": argv, "capture_output": capture_output,
                                        "timeout": timeout, "stdin": stdin, "check": check}), sc)
    maybe_raise(sc)
    return sc


def stub_uuid4():
    sc = consume("uuid4_hex")
    record("uuid4_hex", {}, sc)
    maybe_raise(sc)
    return types.SimpleNamespace(hex=sc)


def stub_get_sandbox_dir():
    sc = consume("get_sandbox_dir")
    record("get_sandbox_dir", {}, sc)
    maybe_raise(sc)
    # PurePosixPath keeps str() forward-slash on the Windows oracle host
    # (Path.as_posix normalization); fixture roots are clean absolute paths.
    return PurePosixPath(sc) if isinstance(sc, str) else sc


def stub_egress_proxy():
    sc = consume("egress_proxy_args_for_docker")
    record("egress_proxy_args_for_docker", {}, sc)
    maybe_raise(sc)
    return tuple(sc) if isinstance(sc, list) else sc


def stub_find_reusable(task_label, profile_label, egress_label):
    sc = consume("find_reusable_container")
    record("find_reusable_container",
           drop_none({"task_label": task_label, "profile_label": profile_label,
                      "egress_label": egress_label}), sc)
    maybe_raise(sc)
    return tuple(sc) if isinstance(sc, list) else sc


def mk_os(inp):
    return types.SimpleNamespace(
        path=types.SimpleNamespace(
            isdir=rec_fn("isdir", lambda path: {"path": path}),
            expanduser=rec_fn("expanduser", lambda path: {"path": path}),
            abspath=rec_fn("abspath", lambda path: {"path": path})),
        makedirs=rec_fn("makedirs", lambda path, exist_ok=False: {"path": path, "exist_ok": exist_ok}),
        name=inp.get("os_name", "posix"))


def install_stubs(ns, inp):
    # restore every target to REAL first (a prior flow case may have left a
    # recorder bound in a shared slot)
    for nm, fn in REAL.items():
        ns[nm] = fn
    ns["os"] = mk_os(inp)
    ns["sys"] = types.SimpleNamespace(platform=inp.get("platform", "linux"))
    ns["logger"] = RecLogger()
    ns["uuid"] = types.SimpleNamespace(uuid4=stub_uuid4)
    ns["subprocess"] = types.SimpleNamespace(
        run=stub_subprocess_run, DEVNULL=subprocess.DEVNULL,
        TimeoutExpired=subprocess.TimeoutExpired,
        CalledProcessError=subprocess.CalledProcessError,
        SubprocessError=subprocess.SubprocessError,
        CompletedProcess=subprocess.CompletedProcess)
    ns["run_capture"] = stub_run_capture
    ns["client_env_with"] = rec_fn("client_env_with", lambda values: {"values": values})
    ns["_is_windows_drive_path"] = rec_fn("is_windows_drive_path", lambda path: {"path": path})
    ns["_host_path_key"] = rec_fn("host_path_key", lambda path: {"path": path})
    ns["get_sandbox_dir"] = stub_get_sandbox_dir
    ns["_sandbox_dir_name"] = rec_fn("sanitize_task_id_for_path", lambda task_id: {"task_id": task_id})
    ns["_cgroup_limits_available"] = rec_fn("cgroup_limits_available", lambda image: {"image": image})
    ns["_extra_args_set_shm_size"] = rec_fn("extra_args_set_shm_size", lambda extra_args: {"extra_args": extra_args})
    ns["_resolve_host_user_spec"] = rec_fn("resolve_host_user_spec", lambda: {})
    ns["_egress_proxy_args_for_docker"] = stub_egress_proxy
    ns["_egress_reuse_fingerprint"] = rec_fn(
        "egress_reuse_fingerprint",
        lambda volume_args, env_overrides, host_args:
            {"volume_args": volume_args, "env_overrides": env_overrides, "host_args": host_args})
    ns["_egress_enforce_on_docker"] = rec_fn("egress_enforce_on_docker", lambda: {})
    ns["_critical_egress_env_names"] = rec_fn(
        "critical_egress_env_names", lambda env_overrides: {"env_overrides": env_overrides})
    ns["check_forward_env_collisions"] = rec_fn(
        "check_forward_env_collisions",
        lambda forward_env, critical, enforce:
            {"forward_env": forward_env, "critical": critical, "enforce": enforce})
    ns["check_docker_env_collisions"] = rec_fn(
        "check_docker_env_collisions",
        lambda user_env, egress_env, enforce:
            {"user_env": user_env, "egress_env": egress_env, "enforce": enforce})
    ns["check_extra_args_collisions"] = rec_fn(
        "check_extra_args_collisions",
        lambda extra_args, critical, enforce:
            {"extra_args": extra_args, "critical": critical, "enforce": enforce})
    ns["merge_egress_env"] = rec_fn(
        "merge_egress_env",
        lambda user_env, egress_env, enforce:
            {"user_env": user_env, "egress_env": egress_env, "enforce": enforce})


def mk_self(inp):
    self = types.SimpleNamespace(**(inp.get("self") or {}))
    # bind every extracted method (the case target AND its real collaborators)
    for nm in METHOD_TARGETS:
        setattr(self, nm, types.MethodType(REAL[nm], self))
    # claimed siblings stay seamed (plain recorders: instance attrs don't bind)
    self._find_reusable_container = stub_find_reusable
    self._container_network_mode = rec_fn("container_network_mode",
                                          lambda container_id: {"container_id": container_id})
    self._storage_opt_supported = rec_fn("storage_opt_supported", lambda: {})
    return self


def project_self(self):
    return {k: v for k, v in vars(self).items() if not callable(v)}


def project_ret(r):
    if isinstance(r, subprocess.CompletedProcess):
        return {"returncode": r.returncode, "stdout": r.stdout, "stderr": r.stderr}
    if isinstance(r, BaseException):
        return exc_norm(r)
    if isinstance(r, tuple):
        return list(r)
    return r


# ── drive the LIVE target for one case ──
def drive(ns, func, inp):
    CUR["state_out"] = {}
    if func == "docker_query":
        exc_types = ((subprocess.SubprocessError, OSError)
                     if inp.get("exc_types") == "subprocess"
                     else (subprocess.TimeoutExpired, OSError))
        r = ns["_docker_query"](inp.get("argv", []), timeout=inp.get("timeout", 10),
                                fail=inp.get("fail", ""),
                                fail_args=tuple(inp.get("fail_args") or ()),
                                nonzero=inp.get("nonzero"), exc_types=exc_types)
        return {"ret": project_ret(r)}
    if func == "is_idle_workspace_container":
        r = ns["_is_idle_workspace_container"](inp.get("docker_exe", "docker"),
                                               inp.get("container_id", ""))
        return {"ret": r}
    if func == "extra_args_network_mode":
        return {"ret": ns["_extra_args_network_mode"](inp.get("extra_args"))}
    if func == "split_volume_spec":
        return {"ret": project_ret(ns["_split_volume_spec"](inp.get("spec")))}
    if func == "container_mount_taken":
        return {"ret": ns["_container_mount_taken"](inp.get("volume_args", []),
                                                   inp.get("mount", ""))}
    if func == "existing_host_mount":
        return {"ret": ns["_existing_host_mount"](inp.get("volume_args", []),
                                                   inp.get("host_cwd_abs", ""))}
    if func == "free_host_cwd_mount":
        return {"ret": ns["_free_host_cwd_mount"](inp.get("volume_args", []))}
    if func == "abs_host_cwd":
        return {"ret": ns["_abs_host_cwd"](inp.get("host_cwd", ""))}
    if func == "host_user_args":
        return {"ret": ns["_host_user_args"](inp.get("run_as_host_user", False))}
    if func == "attach_existing_container":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        r = self._attach_existing_container(inp.get("task_label", ""),
                                            inp.get("profile_name", ""),
                                            inp.get("egress_label", "off"),
                                            inp.get("network", True))
        return {"ret": r}
    if func == "image_available_locally":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        return {"ret": self._image_available_locally()}
    if func == "start_container":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        return {"ret": project_ret(self._start_container(inp.get("container_id", "")))}
    if func == "docker_run":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        return {"ret": self._docker_run(inp.get("cwd", "/root"))}
    if func == "run_command":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        return {"ret": self._run_command(inp.get("name", ""), inp.get("workdir", "/root"))}
    if func == "resource_args":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        r = self._resource_args(inp.get("image", ""), float(inp.get("cpu", 0)),
                                int(inp.get("memory", 0)), int(inp.get("disk", 0)),
                                inp.get("network", True), inp.get("shm_size"),
                                inp.get("extra_args"))
        return {"ret": r}
    if func == "mount_args":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        r = self._mount_args(inp.get("volumes"), inp.get("host_cwd"),
                             inp.get("auto_mount_cwd", False), inp.get("task_id", "default"))
        return {"ret": [r[0], r[1]]}
    if func == "egress_and_env_args":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        return {"ret": list(self._egress_and_env_args(inp.get("extra_args")))}
    if func == "container_image":
        self = mk_self(inp)
        CUR["state_out"]["self"] = self
        return {"ret": self._container_image(inp.get("container_id", ""))}
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["counts"] = {}
    CUR["state_out"] = {}
    try:
        out = drive(ns, func, inp)
        out["exc"] = None
    except BaseException as e:
        out = {"ret": None, "exc": {"__raise__": type(e).__name__, "msg": str(e)}}
    for k, v in CUR["state_out"].items():
        out[k] = project_self(v) if isinstance(v, types.SimpleNamespace) else v
    out["trace"] = CUR["trace"]
    return out


# ── comparison ──
def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def compare(want, got):
    if canon(want.get("trace", [])) != canon(got.get("trace", [])):
        return False
    for k in set(want) | set(got):
        if k == "trace":
            continue
        if canon(want.get(k)) != canon(got.get(k)):
            return False
    return True


def main():
    ns = build_ns()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record_in = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record_in.get("func")
        inp = record_in.get("in") or {}
        got = record_in.get("out") or {}
        cases += 1
        install_stubs(ns, inp)
        try:
            want = run_case(ns, func, inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if not compare(want, got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE docker_env_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
