#!/usr/bin/env python3
"""Differential oracle for tools/hosted_tool_policy.py (w11_hostedtool_48c71a gate).

Loads the LIVE module via spec_from_file_location with a parent-package bootstrap
and a stubbed tools.environments.sandbox_scope. The _hosted() probe is replaced
per case by an explicit `hosted` flag from the fixture (Windows CRT setenv("")
would unset a variable Python keeps). Exceptions are projected as
{"__raise__":cls,"message":str(exc)} and compared to the C seam's object.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# parent-package bootstrap + sys.modules registration for in-function imports.
tools = types.ModuleType("tools")
tools.__path__ = []
envs = types.ModuleType("tools.environments")
envs.__path__ = []
scope = types.ModuleType("tools.environments.sandbox_scope")
STATE = {"hosted": False}
scope.hosted_scope = lambda: (object() if STATE["hosted"] else None)
sys.modules["tools"] = tools
sys.modules["tools.environments"] = envs
sys.modules["tools.environments.sandbox_scope"] = scope
tools.environments = envs
envs.sandbox_scope = scope

SRC = Q + "/tools/hosted_tool_policy.py"
spec = importlib.util.spec_from_file_location("hosted_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hosted_live"] = live
spec.loader.exec_module(live)


def proj(exc):
    # Use the raw first argument (Python wraps KeyError args in repr via str()).
    msg = exc.args[0] if exc.args else str(exc)
    return {"__raise__": type(exc).__name__, "message": msg if isinstance(msg, str) else str(msg)}


def drive(r):
    func = r.get("func")
    STATE["hosted"] = bool(r.get("hosted", False))
    live._hosted = lambda: STATE["hosted"]
    try:
        if func == "message":
            return live._message(r.get("what", ""), r.get("why", ""))
        if func == "tool_refusal":
            return live.hosted_tool_refusal(r.get("tool_name", ""))
        if func == "feature_enabled":
            return live.host_feature_enabled(r.get("feature", ""))
        if func == "feature_refusal":
            return live.host_feature_refusal(r.get("feature", ""), r.get("what", ""))
        if func == "require":
            live.require_host_feature(r.get("feature", ""), r.get("what", ""))
            return None
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
    try:
        py = drive(r)
    except BaseException as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w11_hostedtool_48c71a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
