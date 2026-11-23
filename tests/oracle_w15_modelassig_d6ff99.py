#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/model_assignment.py (gate w15_modelassig_d6ff99).

The module-level ModelSwitchResult import is stubbed: the three ported helpers
(_same_endpoint, is_explicit_custom_endpoint, entry_points_elsewhere) are pure
and do not touch config/provider resolution.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hcli = _mod("hermes_cli")
hcli.__path__ = [QUARRY + "/hermes_cli"]
_mod("hermes_cli.model_switch", ModelSwitchResult=type("ModelSwitchResult", (), {}))

SRC = QUARRY + "/hermes_cli/model_assignment.py"
spec = importlib.util.spec_from_file_location("w15_modelassig_d6ff99_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w15_modelassig_d6ff99_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "same_endpoint":
        return live._same_endpoint(r.get("a"), r.get("b"))
    if func == "explicit_endpoint":
        return live.is_explicit_custom_endpoint(r.get("provider", ""), r.get("base_url", ""))
    if func == "entry_elsewhere":
        return live.entry_points_elsewhere(r.get("entry"), r.get("base_url", ""))
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
        print(f"ORACLE w15_modelassig_d6ff99: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_modelassig_d6ff99: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
