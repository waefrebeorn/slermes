#!/usr/bin/env python3
"""Oracle for the gateway/run_shutdown.py shutdown-lifecycle FLOW cores
(run_shutdown_flow.c).

AST-extracts the live module functions and GatewayShutdownMixin methods (no
heavy gateway import), seeds GATEWAY_SERVICE_RESTART_EXIT_CODE and the
interrupt-reason constants, and replaces every live I/O (the failure-verdict
helper on the resolve path, runtime-status writes, the quiet-step callable,
logging) with recording stubs returning the same scripted results the C harness
feeds its seams. Compares the ordered seam-call trace plus the return value /
SystemExit code.

Input: stdin JSON lines from t_port_run_shutdown_flow.
Summary: ORACLE run_shutdown_flow: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import json
import os
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_shutdown.py")

WANTED = {
    "_exit_with_failure_verdict", "_resolve_gateway_exit_verdict",
    "_notice_allowed", "_restart_notification_allowed",
    "_shutdown_interrupt_reason", "_quiet_step", "_scale_to_zero_status",
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


class Plat:
    """Hashable Platform double keyed by its .value (config.platforms lookup)."""
    def __init__(self, value):
        self.value = value

    def __hash__(self):
        return hash(self.value)

    def __eq__(self, other):
        return isinstance(other, Plat) and self.value == other.value


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"__name__": "gateway_run_shutdown_extracted", "logger": RecLogger(),
          "GATEWAY_SERVICE_RESTART_EXIT_CODE": 75}
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
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        if isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in WANTED:
                    sub.decorator_list = []   # call as plain functions (self explicit)
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
    return ns


def install_stubs(ns):
    ns["logger"] = RecLogger()
    ns["GATEWAY_SERVICE_RESTART_EXIT_CODE"] = 75

    live_failure = ns["_exit_with_failure_verdict"]

    def failure_wrapper(runner):
        if CUR["func"] == "resolve_gateway_exit_verdict":
            r = cin("failure_verdict", False)
            rec({"ev": "exit_with_failure_verdict", "ret": r})
            return r
        return live_failure(runner)
    ns["_exit_with_failure_verdict"] = failure_wrapper

    run_mod = types.ModuleType("gateway.run")
    run_mod._INTERRUPT_REASON_GATEWAY_RESTART = "Gateway restarting"
    run_mod._INTERRUPT_REASON_GATEWAY_SHUTDOWN = "Gateway shutting down"
    gateway_pkg = types.ModuleType("gateway")
    gateway_pkg.run = run_mod
    sys.modules["gateway"] = gateway_pkg
    sys.modules["gateway.run"] = run_mod


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
    if func in ("exit_with_failure_verdict", "resolve_gateway_exit_verdict"):
        rj = inp.get("runner", {})
        runner = types.SimpleNamespace(
            should_exit_with_failure=rj.get("should_exit_with_failure", False),
            exit_reason=rj.get("exit_reason"),
            exit_code=rj.get("exit_code"),
            _restart_requested=rj.get("_restart_requested", False),
            _restart_via_service=rj.get("_restart_via_service", False))
        if func == "exit_with_failure_verdict":
            return {"ret": ns["_exit_with_failure_verdict"](runner)}
        try:
            ret = ns["_resolve_gateway_exit_verdict"](
                runner, inp.get("signal_initiated_shutdown", False))
            return {"ret": ret}
        except SystemExit as e:
            code = int(e.code)
            rec({"ev": "system_exit", "code": code})
            return {"ret": None, "exit": code}

    if func == "notice_allowed":
        platforms = {Plat(k): types.SimpleNamespace(
            gateway_restart_notification=v.get("gateway_restart_notification"))
            for k, v in (inp.get("config_platforms") or {}).items()}
        runner = types.SimpleNamespace(config=types.SimpleNamespace(platforms=platforms))
        runner._restart_notification_allowed = lambda platform: ns["_restart_notification_allowed"](runner, platform)
        pcfg = inp.get("platform_cfg")
        pcfg_double = types.SimpleNamespace(
            gateway_restart_notification=pcfg.get("gateway_restart_notification")) if pcfg is not None else None
        platform = Plat(inp.get("platform_value", ""))
        return {"ret": ns["_notice_allowed"](runner, platform, inp.get("what", ""), pcfg_double)}

    if func == "shutdown_interrupt_reason":
        runner = types.SimpleNamespace(_restart_requested=inp.get("restart_requested", False))
        return {"ret": ns["_shutdown_interrupt_reason"](runner)}

    if func == "quiet_step":
        def fn_stub():
            if cin("step_raises", False):
                rec({"ev": "fn", "raised": True})
                raise RuntimeError(cin("step_error", ""))
            r = cin("step_ret")
            rec({"ev": "fn", "ret": r})
            return r
        return {"ret": ns["_quiet_step"](inp.get("label", ""), fn_stub)}

    if func == "scale_to_zero_status":
        def update_stub(state):
            raised = cin("status_raises", False)
            rec({"ev": "update_runtime_status", "state": state, "raised": raised})
            if raised:
                raise RuntimeError("status write failed")
        runner = types.SimpleNamespace(_update_runtime_status=update_stub)
        ns["_scale_to_zero_status"](runner, inp.get("state", ""), inp.get("fail_msg", ""))
        return {"ret": None}

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
    print("ORACLE run_shutdown_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
