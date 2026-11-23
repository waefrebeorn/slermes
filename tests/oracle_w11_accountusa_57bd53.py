#!/usr/bin/env python3
"""Differential oracle for agent/account_usage_cache.py (gate w11_accountusa_57bd53)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SN = types.SimpleNamespace

# Stub modules imported lazily inside the ported functions.
hc = types.ModuleType("hermes_constants")
hc.hermes_home_key = lambda: ""
sys.modules["hermes_constants"] = hc
agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg
cp = types.ModuleType("agent.credential_pool")
cp._codex_principal_identity = lambda token: None
sys.modules["agent.credential_pool"] = cp
class Snap:
    pass
au = types.ModuleType("agent.account_usage")
au.AccountUsageSnapshot = Snap
sys.modules["agent.account_usage"] = au

SRC = QUARRY + "/agent/account_usage_cache.py"
spec = importlib.util.spec_from_file_location("w11_accountusa_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_accountusa_live"] = live
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
    try:
        if func == "key":
            hc.hermes_home_key = lambda: r.get("home_key", "")
            want = list(live._key(r.get("provider", ""), r.get("identity_id", ""),
                                  r.get("base_url", "")))
        elif func == "identity":
            principal = r.get("principal", None)
            cp._codex_principal_identity = (
                (lambda token: tuple(principal)) if principal else (lambda token: None))
            entry = SN(runtime_api_key=r.get("runtime_api_key", ""), id=r.get("entry_id", ""))
            want = live._identity_id_for(r.get("provider", ""), entry)
        elif func == "stale":
            fetched_at = float(r.get("fetched_at", 0.0))
            live.time = SN(time=lambda: float(r.get("now", 0.0)))
            if r.get("valid", False):
                snap = Snap()
                def timestamp():
                    if r.get("bad_timestamp", False):
                        raise ValueError("bad")
                    return fetched_at
                snap.fetched_at = SN(timestamp=timestamp)
            else:
                snap = object()
            want = live.snapshot_is_stale(snap)
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_accountusa_57bd53: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
