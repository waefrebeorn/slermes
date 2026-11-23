#!/usr/bin/env python3
"""Differential oracle for live Hermes gateway/session_local.py (port w8_sessionloca_b67e17).

External lookups (route_supported_efforts / STATIC_TIERS / resolve_fast_mode_overrides)
are injected through seams; the LIVE effort_fields body and real clamp_effort decide.
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
import gateway  # noqa: F401  parent-package bootstrap

SRC = QUARRY + "/gateway/session_local.py"
NAME = "session_local_live_w8"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

import agent.fast_mode as fm
import agent.reasoning_effort as re
import hermes_cli.models as models


def compute(r):
    func = r.get("func")
    if func == "effort_fields":
        supported = tuple(r.get("supported_efforts", []))
        static = frozenset(r.get("static_tiers", ["priority", "ultrafast"]))
        override = r.get("fast_override")
        re.route_supported_efforts = lambda *a, **k: supported
        fm.STATIC_TIERS = static
        models.resolve_fast_mode_overrides = lambda *a, **k: override
        return live.effort_fields(
            r.get("config"), r.get("tier"), r.get("model"), r.get("provider"),
            r.get("base_url"), r.get("api_mode"))
    if func == "identity":
        src = types.SimpleNamespace(**r.get("source", {}))
        return list(live.LocalSessionAdapter._identity(src))
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
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_sessionloca_b67e17: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
