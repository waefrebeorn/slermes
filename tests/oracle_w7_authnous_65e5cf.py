#!/usr/bin/env python3
"""Differential oracle for w7_authnous_65e5cf (hermes_cli/auth_nous.py)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/auth_nous.py"
spec = importlib.util.spec_from_file_location("w7_authnous_65e5cf_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_authnous_65e5cf_live"] = live
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
    try:
        if func == "scope_values":
            py = sorted(live._scope_values(r.get("raw")))
        elif func == "model_priority":
            py = list(live._model_priority(r.get("mid", "")))
        elif func == "validate_url":
            override = r.get("override") or None
            live._nous_inference_env_override = lambda: override
            py = live._validate_nous_inference_url_from_network(r.get("url"))
        elif func == "portal_host":
            py = live._portal_host(r.get("url"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} {r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_authnous_65e5cf: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
