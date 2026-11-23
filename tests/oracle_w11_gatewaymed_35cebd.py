#!/usr/bin/env python3
"""Differential oracle for acp_adapter/gateway_media.py (gate w11_gatewaymed_35cebd)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

# Stub the one heavy top-level import; submit_fingerprint itself is pure.
hc = types.ModuleType("hermes_cli")
hc.__path__ = []
sys.modules["hermes_cli"] = hc
gc = types.ModuleType("hermes_cli.gateway_client")
class GatewayClientError(Exception):
    pass
gc.GatewayClientError = GatewayClientError
sys.modules["hermes_cli.gateway_client"] = gc

SRC = QUARRY + "/acp_adapter/gateway_media.py"
spec = importlib.util.spec_from_file_location("w11_gatewaymed_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_gatewaymed_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")
    if func != "fingerprint":
        continue
    cases += 1
    try:
        want = list(live.submit_fingerprint(r.get("session_id", ""), r.get("content", "")))
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        mism += 1
        continue
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_gatewaymed_35cebd: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
