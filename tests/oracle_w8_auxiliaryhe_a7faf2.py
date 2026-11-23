#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_health.py (gate w8_auxiliaryhe_a7faf2).

The LIVE module's top-level route_identity import is satisfied from the quarry;
its in-function agent.auxiliary_client / hermes_cli.runtime_provider lookups
are replaced per case with fakes whose values come straight from the fixture.
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


def pkg(name, path):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = [path]
        sys.modules[name] = m
    return sys.modules[name]


pkg("agent", QUARRY + "/agent")
pkg("hermes_cli", QUARRY + "/hermes_cli")


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w8_auxiliaryhe_a7faf2_live", QUARRY + "/agent/auxiliary_health.py")


def compute(r):
    func = r.get("func")
    if func == "quarantine_ttl":
        return live.fallback_candidate_quarantine_ttl(r.get("reason"))
    if func == "custom_health_base_url":
        label = r.get("label", "")
        current = r.get("current_custom", "")
        resolves = bool(r.get("resolves", False))
        entry = r.get("named_entry")

        fake_ac = types.ModuleType("agent.auxiliary_client")
        fake_ac._normalize_chain_label = lambda p: label
        fake_ac._current_custom_base_url = lambda: current
        fake_rp = types.ModuleType("hermes_cli.runtime_provider")
        fake_rp._resolves_to_custom = lambda lab: resolves
        fake_rp._get_named_custom_provider = lambda p: entry

        old_ac = sys.modules.get("agent.auxiliary_client")
        old_rp = sys.modules.get("hermes_cli.runtime_provider")
        sys.modules["agent.auxiliary_client"] = fake_ac
        sys.modules["hermes_cli.runtime_provider"] = fake_rp
        try:
            return live._custom_health_base_url(r.get("provider", ""), r.get("explicit"))
        finally:
            if old_ac is None:
                sys.modules.pop("agent.auxiliary_client", None)
            else:
                sys.modules["agent.auxiliary_client"] = old_ac
            if old_rp is None:
                sys.modules.pop("hermes_cli.runtime_provider", None)
            else:
                sys.modules["hermes_cli.runtime_provider"] = old_rp
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
        print("ORACLE w8_auxiliaryhe_a7faf2: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w8_auxiliaryhe_a7faf2 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w8_auxiliaryhe_a7faf2: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
