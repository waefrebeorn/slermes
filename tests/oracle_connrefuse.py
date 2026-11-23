#!/usr/bin/env python3
"""Differential oracle for connector_refusals.py (gate connrefuse)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"; sys.path.insert(0, Q)

def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m

RUN_CONST = {"UNKNOWN_TARGET":"unknown_target","MIXED_KINDS":"mixed_kinds",
             "LINK_STILL_VALID":"link_still_valid","SETTLED":"settled",
             "NOT_ALLOWED":"not_allowed","REFUSED":"refused"}
stub("tools.connectors.run", **RUN_CONST)
REASONS = {
 "invalid_params":"INVALID_PARAMS","not_owner":"NOT_OWNER","unsupported_runtime":"UNSUPPORTED_RUNTIME",
 "connector_request_failed":"CONNECTOR_REQUEST_FAILED","invalid_connector_response":"INVALID_CONNECTOR_RESPONSE",
 "unknown_target":"UNKNOWN_TARGET","link_still_valid":"LINK_STILL_VALID","reissue_refused":"REISSUE_REFUSED",
 "unknown_operation":"UNKNOWN_OPERATION","invalid_answer":"INVALID_ANSWER","needs_nous_auth":"NEEDS_NOUS_AUTH",
 "connector_not_found":"CONNECTOR_NOT_FOUND","tools_unavailable":"TOOLS_UNAVAILABLE",
 "connectors_unavailable":"CONNECTORS_UNAVAILABLE","catalog_unavailable":"CATALOG_UNAVAILABLE",
 "accounts_unavailable":"ACCOUNTS_UNAVAILABLE","connection_not_found":"CONNECTION_NOT_FOUND",
 "policy_unavailable":"POLICY_UNAVAILABLE","policy_conflict":"POLICY_CONFLICT",
 "forbidden_scope":"FORBIDDEN_SCOPE","org_required":"ORG_REQUIRED","org_access_denied":"ORG_ACCESS_DENIED",
 "invalid_policy":"INVALID_POLICY","alias_taken":"ALIAS_TAKEN",
}
stub("tui_gateway.contracts.connectors", ConnectorErrorReason=types.SimpleNamespace(**REASONS))
name = "connector_refusals_live"
sp = importlib.util.spec_from_file_location(name, os.path.join(Q, "tui_gateway/connector_refusals.py"))
live = importlib.util.module_from_spec(sp); sys.modules[name] = live; sp.loader.exec_module(live)

def compat(t):
    if t is None or isinstance(t, (int, float, str, bool)): return t
    if isinstance(t, (list, tuple)): return [compat(x) for x in t]
    return t

def compute(r):
    f = r.get("func")
    if f == "constant":
        v = getattr(live, r.get("name", ""), None)
        return compat(v)
    if f == "reissue_refusal":
        v = live.REISSUE_REFUSALS.get(r.get("key", ""))
        return compat(v)
    if f == "auth_refusal":
        exc = types.SimpleNamespace(code=r.get("code", ""), status=r.get("status", 0))
        return compat(live.auth_refusal(exc))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") is None: continue
    cases += 1
    py, got = compute(r), r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH connrefuse %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE connrefuse: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
