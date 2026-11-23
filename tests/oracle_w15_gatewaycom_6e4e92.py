#!/usr/bin/env python3
"""Differential oracle for acp_adapter/gateway_commands.py (gate w15_gatewaycom_6e4e92)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("acp_adapter", QUARRY + "/acp_adapter")
_pkg("hermes_cli", QUARRY + "/hermes_cli")
gc = types.ModuleType("hermes_cli.gateway_client")


class GatewayClientError(Exception):
    pass


class GatewayRPCError(Exception):
    pass


gc.GatewayClientError = GatewayClientError
gc.GatewayRPCError = GatewayRPCError
sys.modules["hermes_cli.gateway_client"] = gc

spec = importlib.util.spec_from_file_location(
    "acp_adapter.gateway_commands_live", QUARRY + "/acp_adapter/gateway_commands.py")
live = importlib.util.module_from_spec(spec)
sys.modules["acp_adapter.gateway_commands_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "deferred_pick_text":
        return live.deferred_pick_text(r.get("result") or {})
    if f == "refused_pick_text":
        return live.refused_pick_text(r.get("update") or {})
    if f == "summary_text":
        return live._summary_text(r.get("result") or {})
    if f == "model_text":
        return live._model_text(r.get("label", ""), r.get("fields") or {})
    if f == "tools_text":
        return live._tools_text(r.get("info") or {})
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
    try:
        py = compute(r)
    except Exception as exc:
        print("PYERROR %r %r" % (r, exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE w15_gatewaycom_6e4e92: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
