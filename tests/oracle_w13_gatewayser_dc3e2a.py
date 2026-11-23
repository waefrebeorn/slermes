#!/usr/bin/env python3
"""Differential oracle for the pure module-level helpers of
acp_adapter/gateway_server.py (gate w13_gatewayser_dc3e2a).

The live helpers are AST-extracted from the real quarry source (the surrounding
module is an async gateway class with heavy imports that are irrelevant here).
Env and command registry are injected per case so the extracted live code is
still what computes every expected value.
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/acp_adapter/gateway_server.py"
NAMES = {"_tools_changed_notice", "_bypass_route", "_slash_command"}


class _GWCError(Exception):
    pass


class _FakeOS:
    def __init__(self, env):
        self.environ = env


class _Cmd:
    def __init__(self, name):
        self.name = name


g = {"os": _FakeOS({}), "GatewayClientError": _GWCError}
tree = ast.parse(open(SRC, encoding="utf-8").read())
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in NAMES:
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), g)
for name in NAMES:
    if name not in g:
        print(f"ORACLE w13_gatewayser_dc3e2a: live {name} not extracted")
        sys.exit(1)


def _install_registry(cmds, routes):
    for pkg in ("hermes_cli", "acp_adapter"):
        if pkg not in sys.modules:
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m
    cm = types.ModuleType("hermes_cli.commands")
    cm.resolve_command = lambda n: (_Cmd(cmds[n]) if n in cmds else None)
    gc = types.ModuleType("acp_adapter.gateway_commands")
    gc.ROUTES = set(routes or [])
    sys.modules["hermes_cli.commands"] = cm
    sys.modules["acp_adapter.gateway_commands"] = gc
    sys.modules["hermes_cli"].commands = cm
    sys.modules["acp_adapter"].gateway_commands = gc


def compute(r):
    f = r.get("func")
    if f == "tools_changed":
        return g["_tools_changed_notice"](r.get("payload") or {})
    if f == "bypass":
        env = {
            "HERMES_MODEL": r.get("model", "") or "",
            "HERMES_INFERENCE_MODEL": r.get("imodel", "") or "",
            "HERMES_INFERENCE_PROVIDER": r.get("provider", "") or "",
        }
        g["os"] = _FakeOS(env)
        try:
            return g["_bypass_route"]()
        except _GWCError as exc:
            return {"error": str(exc)}
    if f == "slash":
        _install_registry(r.get("commands") or {}, r.get("routes") or [])
        return g["_slash_command"](r.get("text", ""))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_gatewayser_dc3e2a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
