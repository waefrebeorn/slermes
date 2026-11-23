#!/usr/bin/env python3
"""Differential oracle for tools/code_execution_rpc.py (w11_codeexecut_b29647 gate).

Loads the LIVE module via spec_from_file_location with a parent-package
bootstrap and stubs for its two module-level quarry imports. env.execute() is
a per-case data seam; no subprocess/socket is ever touched. Exceptions are
projected as {"__raise__":cls,"message":exc.args[0]}.
"""
import importlib.util
import json
import sys
import types
from contextlib import contextmanager

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# parent-package bootstrap + sys.modules registration.
agent = types.ModuleType("agent"); agent.__path__ = []
tso = types.ModuleType("agent.thread_scoped_output")
@contextmanager
def _silence():
    yield
tso.thread_scoped_silence = _silence
sys.modules["agent"] = agent
sys.modules["agent.thread_scoped_output"] = tso
agent.thread_scoped_output = tso

tools = types.ModuleType("tools"); tools.__path__ = []
reg = types.ModuleType("tools.registry")
reg.tool_error = lambda msg: msg
sys.modules["tools"] = tools
sys.modules["tools.registry"] = reg
tools.registry = reg

SRC = Q + "/tools/code_execution_rpc.py"
spec = importlib.util.spec_from_file_location("rpc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["rpc_live"] = live
spec.loader.exec_module(live)


def proj(exc):
    msg = exc.args[0] if exc.args else str(exc)
    return {"__raise__": type(exc).__name__, "message": msg if isinstance(msg, str) else str(msg)}


class Env:
    def __init__(self, r):
        self.result = r.get("execute_result") or {}

    def execute(self, cmd, **kwargs):
        return self.result


def drive(r):
    func = r.get("func")
    try:
        if func == "private_dirs_cmd":
            return live._private_dirs_cmd(r.get("root", ""), *r.get("subdirs", []))
        if func == "rpc_token_ok":
            return live._rpc_token_ok(r.get("request") or {}, r.get("rpc_token", ""))
        if func == "result_error":
            return live._result_error(r.get("result", ""))
        if func == "tool_errors_since":
            return live.tool_errors_since(r.get("tool_call_log") or [], int(r.get("start", 0)))
        if func == "execute_checked":
            return live._execute_checked(Env(r), r.get("cmd", ""), r.get("what", ""),
                                         timeout=int(r.get("timeout", 0)))
        if func == "remote_write":
            return live._remote_write(Env(r), r.get("remote_path", ""), r.get("content", ""),
                                      atomic=bool(r.get("atomic", False)),
                                      timeout=int(r.get("timeout", 30)), check=bool(r.get("check", False)))
    except BaseException as exc:
        return proj(exc)
    return "UNKNOWN_FUNC"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    py = drive(r)
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w11_codeexecut_b29647: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
