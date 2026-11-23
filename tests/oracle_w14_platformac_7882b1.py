#!/usr/bin/env python3
"""Differential oracle for gateway/platform_activation.py (gate w14_platformac_7882b1).

Env / .env / credential-pool / gateway.json scans are injected per case by
patching the LIVE module's internal readers; the LIVE activation_predicate and
its inner has_evidence then run against those fakes.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/gateway/platform_activation.py"
spec = importlib.util.spec_from_file_location("w14_pa_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


class Reg:
    def __init__(self, declared):
        self.declared = declared

    def activation_env(self, name):
        return self.declared


def want_for(r):
    func = r.get("func")
    if func == "yaml_block_names":
        return sorted(live._yaml_block_names(r.get("yaml_cfg")))
    if func == "matches":
        return live._matches(r.get("name", ""), r.get("candidates") or [])
    if func == "has_evidence":
        live._yaml_block_names = lambda cfg: set(r.get("blocks") or [])
        live._legacy_gateway_json_names = lambda home: set(r.get("legacy") or [])
        live._env_names = lambda home: set(r.get("env") or [])
        live._credential_pool_names = lambda home: set(r.get("pools") or [])
        pred = live.activation_predicate(
            yaml_cfg={}, platform_names=r.get("platform_names") or [],
            home=Path("."), registry=Reg(r.get("declared") or []))
        return bool(pred(r.get("name", "")))
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
    want = want_for(r)
    if want is None:
        continue
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w14_platformac_7882b1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
