#!/usr/bin/env python3
"""Differential oracle for agent/provider_registry.py (gate w9_providerreg_49f364).

Drives the LIVE pure helpers with seeded collaborators: config reads and the
NOUS_MANAGED_PROVIDER constant come from the fixture via registered stub
modules, provider.is_available is a fake.  C reproduces each helper's own
normalization / mapping / safe-bool logic.
"""
import importlib.util
import json
import logging
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

hc = types.ModuleType("hermes_constants")
hc.hermes_home_key = lambda scope: scope
hc.normalize_scope = lambda s: s
sys.modules["hermes_constants"] = hc

cli_pkg = types.ModuleType("hermes_cli")
cli_pkg.__path__ = [QUARRY + "/hermes_cli"]
sys.modules.setdefault("hermes_cli", cli_pkg)
cfg = types.ModuleType("hermes_cli.config")
sys.modules["hermes_cli.config"] = cfg

tools_pkg = types.ModuleType("tools")
tools_pkg.__path__ = [QUARRY + "/tools"]
sys.modules.setdefault("tools", tools_pkg)
tbh = types.ModuleType("tools.tool_backend_helpers")
tbh.NOUS_MANAGED_PROVIDER = "nous"
sys.modules["tools.tool_backend_helpers"] = tbh

live = _load("agent.provider_registry", QUARRY + "/agent/provider_registry.py")
logger = logging.getLogger("port-test")


def compute(r):
    func = r.get("func")
    if func == "lower_key":
        return live.lower_key(r.get("name", ""))
    if func == "exact_key":
        return live.exact_key(r.get("name", ""))
    if func == "configured":
        section = r.get("section", "tts")
        raw = r.get("raw")
        cfg.load_config_readonly = lambda: ({section: {"provider": raw}} if raw is not None else {})
        return live.configured_provider_name(section, logger)
    if func == "is_available_safe":
        class P:
            name = "p"
            def is_available(self):
                if r.get("raises"):
                    raise RuntimeError("boom")
                return bool(r.get("available", False))
        return live.is_available_safe(P(), logger, "fmt %s")
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
print(f"ORACLE w9_providerreg_49f364: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
