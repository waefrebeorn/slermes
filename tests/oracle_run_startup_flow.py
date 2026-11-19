#!/usr/bin/env python3
"""Oracle for the gateway/run_startup.py startup-lifecycle FLOW cores
(run_startup_flow.c).

AST-extracts the live GatewayStartupMixin methods (no heavy gateway import),
seeds the module constants they read (GATEWAY_FATAL_CONFIG_EXIT_CODE, the
imported is_global_startup_conflict), and replaces every live I/O (allowlist
probe, runtime-status writes, conflict classification, clean-exit request, the
fatal-config helper on the no-connections path, logging) with recording stubs
returning the same scripted results the C harness feeds its seams. Compares the
ordered seam-call trace plus the return value / mutated runner state.

Input: stdin JSON lines from t_port_run_startup_flow.
Summary: ORACLE run_startup_flow: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import json
import os
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_startup.py")

WANTED = {
    "_serving_state", "_startup_should_abort", "_resume_owner_authorized",
    "_park_last_fatal_platform", "_start_handle_no_connections",
    "_startup_fail_fatal_config",
}

CUR = {"func": None, "in": {}, "trace": []}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


class RecLogger:
    def _rec(self, level, fmt, args):
        rec({"ev": "log", "level": level, "fmt": fmt,
             "args": [str(a) if isinstance(a, BaseException) else a for a in args]})

    def debug(self, fmt, *a, **k):
        self._rec(10, fmt, a)

    def info(self, fmt, *a, **k):
        self._rec(20, fmt, a)

    def warning(self, fmt, *a, **k):
        self._rec(30, fmt, a)

    def error(self, fmt, *a, **k):
        self._rec(40, fmt, a)

    def log(self, level, fmt, *a, **k):
        self._rec(level, fmt, a)


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"__name__": "gateway_run_startup_extracted", "logger": RecLogger(),
          "GATEWAY_FATAL_CONFIG_EXIT_CODE": 78}
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in WANTED:
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
    return ns


def install_stubs(ns):
    ns["logger"] = RecLogger()
    ns["GATEWAY_FATAL_CONFIG_EXIT_CODE"] = 78

    def conflict_stub(code):
        r = cin("conflict", False)
        rec({"ev": "is_global_startup_conflict", "code": code, "ret": r})
        return r
    ns["is_global_startup_conflict"] = conflict_stub

    def write_status_stub(gateway_state=None, exit_reason=None):
        rec({"ev": "write_runtime_status_quiet", "gateway_state": gateway_state,
             "exit_reason": exit_reason})
    run_mod = types.ModuleType("gateway.run")
    run_mod._write_runtime_status_quiet = write_status_stub
    gateway_pkg = types.ModuleType("gateway")
    gateway_pkg.run = run_mod
    sys.modules["gateway"] = gateway_pkg
    sys.modules["gateway.run"] = run_mod


def make_runner(inp, ns):
    st = inp.get("state", {})
    runner = types.SimpleNamespace()
    runner._startup_parked_platforms = st.get("_startup_parked_platforms", False)
    runner._restart_requested = st.get("_restart_requested", False)
    runner._draining = st.get("_draining", False)
    runner._external_drain_active = st.get("_external_drain_active", False)
    runner._running = st.get("_running", True)
    runner._startup_ownership_conflict = st.get("_startup_ownership_conflict", False)
    runner._exit_code = st.get("_exit_code", None)
    runner._startup_restore_in_progress = st.get("_startup_restore_in_progress", True)

    def shutdown_stub():
        r = cin("shutdown_event_set", False)
        rec({"ev": "shutdown_event_is_set", "ret": r})
        return r
    runner._shutdown_event = types.SimpleNamespace(is_set=shutdown_stub)

    def authorized_stub(source):
        if cin("authorize_raises", False):
            rec({"ev": "is_user_authorized", "raised": True})
            raise RuntimeError(cin("authorize_error", ""))
        r = cin("authorized", False)
        rec({"ev": "is_user_authorized", "ret": r})
        return r
    runner._is_user_authorized_for_source = authorized_stub

    def update_status_stub(state):
        rec({"ev": "update_runtime_status", "state": state})
    runner._update_runtime_status = update_status_stub

    def clean_exit_stub(reason):
        rec({"ev": "request_clean_exit", "reason": reason})
    runner._request_clean_exit = clean_exit_stub

    runner._serving_state = lambda: ns["_serving_state"](runner)
    # Scripted helper on the no-connections path (records one event); the live
    # core runs directly when it is itself the case target.
    runner._startup_fail_fatal_config = lambda reason: rec(
        {"ev": "startup_fail_fatal_config", "reason": reason})
    return runner


def adapter_double(inp):
    a = inp.get("adapter", {})
    plat = a.get("platform", {})
    return types.SimpleNamespace(
        fatal_error_retryable=a.get("fatal_error_retryable", False),
        fatal_error_code=a.get("fatal_error_code"),
        fatal_error_message=a.get("fatal_error_message"),
        platform=types.SimpleNamespace(value=plat.get("value", "")))


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


def drive(ns, func, inp):
    runner = make_runner(inp, ns)
    st = inp.get("state", {})
    if func == "serving_state":
        return {"ret": ns["_serving_state"](runner)}
    if func == "startup_should_abort":
        return {"ret": ns["_startup_should_abort"](runner)}
    if func == "resume_owner_authorized":
        return {"ret": ns["_resume_owner_authorized"](
            runner, inp.get("session_key", ""), inp.get("source"))}
    if func == "park_last_fatal_platform":
        ret = ns["_park_last_fatal_platform"](runner, adapter_double(inp))
        out_state = dict(st)
        # The C state copy only carries _startup_parked_platforms when the input
        # had it or the core parked (set it true); mirror that key set exactly.
        if "_startup_parked_platforms" in st or runner._startup_parked_platforms:
            out_state["_startup_parked_platforms"] = runner._startup_parked_platforms
        return {"ret": ret, "state": out_state}
    if func == "start_handle_no_connections":
        ret = ns["_start_handle_no_connections"](
            runner, inp.get("connected_count", 0), inp.get("enabled_platform_count", 0),
            inp.get("retryable", []), inp.get("nonretryable", []))
        out_state = dict(st)
        if "_startup_parked_platforms" in st or runner._startup_parked_platforms:
            out_state["_startup_parked_platforms"] = runner._startup_parked_platforms
        return {"ret": ret, "state": out_state}
    if func == "startup_fail_fatal_config":
        ns["_startup_fail_fatal_config"](runner, inp.get("reason", ""))
        out_state = dict(st)
        out_state["_exit_code"] = runner._exit_code
        out_state["_startup_restore_in_progress"] = runner._startup_restore_in_progress
        return {"ret": None, "state": out_state}
    raise ValueError("unknown func: %s" % func)


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
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        try:
            want = drive(ns, func, inp)
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE run_startup_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
