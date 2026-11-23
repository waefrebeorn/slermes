#!/usr/bin/env python3
"""Differential oracle for tools/managed_gateway_auth.py (w14_managedgat_6ff729).

The gateway builder, token reader and user-agent helper are injected seams in
the live module; the fixture supplies their results through tool_url,
connector_url and token, and a stub tools.managed_gateway_identity fixes the
user-agent string to the same literal the C port emits.
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


tools = _mod("tools")
tools.__path__ = [Q + "/tools"]
_mod("tools.managed_tool_gateway",
     build_vendor_gateway_url=lambda label: "", read_nous_access_token=lambda: None)
_mod("tools.managed_gateway_identity", user_agent=lambda: "hermes-test/1.0")

SRC = Q + "/tools/managed_gateway_auth.py"
spec = importlib.util.spec_from_file_location("tools.managed_gateway_auth_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
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
    tool_url = r.get("tool_url", "")
    connector_url = r.get("connector_url", "")
    builder = lambda label: {"tool": tool_url, "connector": connector_url}[label]
    token_reader = lambda: r.get("token")
    # connector_gateway_origin() takes no args and reads the module global.
    live.build_vendor_gateway_url = builder
    try:
        if func == "connector_gateway_origin":
            py = live.connector_gateway_origin()
        elif func == "is_managed":
            py = live.is_managed_nous_gateway_url(r.get("url"), gateway_builder=builder)
        elif func == "auth_headers":
            py = live.managed_gateway_auth_headers(
                r.get("url"), gateway_builder=builder, token_reader=token_reader)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_managedgat_6ff729: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
