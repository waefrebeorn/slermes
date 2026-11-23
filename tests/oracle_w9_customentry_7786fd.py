#!/usr/bin/env python3
"""Differential oracle for agent/custom_entry_pool.py (gate w9_customentry_7786fd).

Imports the LIVE module via importlib and stubs the lazy config dependencies
(credential_pool prefix, OPENROUTER_BASE_URL, PROVIDER_REGISTRY) from injected
fixture values. Compares each harness "out" to the live helper result.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

agent_pkg = _pkg("agent")
hcli_pkg = _pkg("hermes_cli")

SRC = QUARRY + "/agent/custom_entry_pool.py"
spec = importlib.util.spec_from_file_location("w9_customentry_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9_customentry_live"] = live
spec.loader.exec_module(live)


def set_deps(r):
    cp = types.ModuleType("agent.credential_pool")
    cp.__path__ = []
    cp.CUSTOM_POOL_PREFIX = "custom:"
    agent_pkg.credential_pool = cp
    sys.modules["agent.credential_pool"] = cp

    hc = types.ModuleType("hermes_constants")
    hc.OPENROUTER_BASE_URL = r.get("openrouter_url")
    sys.modules["hermes_constants"] = hc

    hauth = types.ModuleType("hermes_cli.auth")
    reg = {}
    for k, v in (r.get("registry") or {}).items():
        reg[k] = SimpleNamespace(inference_base_url=v)
    hauth.PROVIDER_REGISTRY = reg
    hcli_pkg.auth = hauth
    sys.modules["hermes_cli.auth"] = hauth


def compute(r):
    func = r.get("func")
    if func == "entry_declares_credential":
        return live.entry_declares_credential(r.get("entry"))
    if func == "own_seeded_sources":
        return sorted(live._own_seeded_sources(r.get("entry")))
    if func in ("builtin_pool_address", "recorded_address"):
        set_deps(r)
        if func == "builtin_pool_address":
            return live.builtin_pool_address(r.get("pool_key"))
        row = r.get("row")
        obj = SimpleNamespace(**row) if isinstance(row, dict) else row
        return live.recorded_address(obj, r.get("pool_key"))
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
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w9_customentry_7786fd: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w9_customentry_7786fd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
