#!/usr/bin/env python3
"""Differential oracle for agent/process_transport.py (gate w9_processtran_ea08b8).

The two foreign predicates the module composes (is_local_endpoint and
_is_external_process_provider) are seeded from the fixture via registered
stub modules.  The LIVE functions still run their own bool(...) / str(...)
coercion and composition; C reproduces exactly that over the same inputs.
"""
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


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)
cli_pkg = types.ModuleType("hermes_cli")
cli_pkg.__path__ = [QUARRY + "/hermes_cli"]
sys.modules.setdefault("hermes_cli", cli_pkg)

mm = types.ModuleType("agent.model_metadata")
sys.modules["agent.model_metadata"] = mm
rpb = types.ModuleType("hermes_cli.runtime_provider_backends")
sys.modules["hermes_cli.runtime_provider_backends"] = rpb

live = _load("agent.process_transport", QUARRY + "/agent/process_transport.py")


def compute(r):
    func = r.get("func")
    local = bool(r.get("local", False))
    external = bool(r.get("external", False))
    base = r.get("base_url", "")
    provider = r.get("provider", "")
    mm.is_local_endpoint = lambda s: local
    rpb._is_external_process_provider = lambda s: external
    if func == "is_local_server":
        return live.is_local_server(base, provider)
    if func == "waits_like_local":
        return live.waits_like_local(base, provider)
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
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_processtran_ea08b8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
