#!/usr/bin/env python3
"""Differential oracle for acp_adapter/permissions.py (gate w9_permissions_d89402).

acp.schema (AllowedOutcome / PermissionOption), the tools package and
tools.approval_context are stubbed in sys.modules so the pure helpers can be
called without the real SDK or config file. The two injected seams
(_get_approval_timeout, _permission_option_supports_kind) are driven per case.
"""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

acp = types.ModuleType("acp")
acp.__path__ = [Q + "/acp"]
sys.modules["acp"] = acp
schema = types.ModuleType("acp.schema")

class Obj:
    def __init__(self, **kw):
        self.__dict__.update(kw)

class AllowedOutcome(Obj):
    pass

class PermissionOption(Obj):
    pass

schema.AllowedOutcome = AllowedOutcome
schema.PermissionOption = PermissionOption
sys.modules["acp.schema"] = schema
acp.schema = schema

tools = types.ModuleType("tools")
tools.__path__ = [Q + "/tools"]
sys.modules["tools"] = tools
CFG = {"timeout": 0.0}
approv = types.ModuleType("tools.approval_context")
approv._get_approval_timeout = lambda: CFG["timeout"]
sys.modules["tools.approval_context"] = approv

SRC = Q + "/acp_adapter/permissions.py"
spec = importlib.util.spec_from_file_location("w9_permissions_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "resolve_timeout":
        CFG["timeout"] = r.get("config_timeout", 0)
        return live.resolve_permission_timeout(r.get("timeout"))
    if func == "map_outcome":
        allowed = set(r.get("allowed", []))
        if r.get("valid_outcome", True):
            outcome = AllowedOutcome(option_id=r.get("option_id", ""))
        else:
            outcome = object()
        return live._map_outcome_to_hermes(outcome, allowed_option_ids=allowed)
    if func == "build_options":
        live._permission_option_supports_kind = lambda k: bool(r.get("supports_reject_always", True))
        opts = live._build_permission_options(
            allow_permanent=bool(r.get("allow_permanent", True)),
            allow_session=bool(r.get("allow_session", True)),
            smart_denied=bool(r.get("smart_denied", False)))
        return [{"option_id": o.option_id, "kind": o.kind, "name": o.name} for o in opts]
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_permissions_d89402: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
