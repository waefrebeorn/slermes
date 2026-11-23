#!/usr/bin/env python3
"""Differential oracle for upstream Hermes agent/plan_limits.py (planlimits gate).

The module has no heavy top-level imports; functions under test are pure readers
of the injected pricing/limit data (cached data only, as in production).
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/agent/plan_limits.py"
spec = importlib.util.spec_from_file_location("planlimits_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["planlimits_live"] = live
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
        if func == "model_label":
            py = live.model_label(r.get("model", ""), r.get("pricing"))
        elif func == "credits_action":
            py = live.credits_action(r.get("kg"), r.get("pm"))
        elif func == "portal_wording":
            py = live.portal_wording(r.get("text", ""))
        elif func == "next_steps":
            py = live.limit_next_steps_from_payload(
                r.get("limit", {}),
                model_command=r.get("model_command", "/model"),
                topup_command=r.get("topup_command", "/topup"),
                portal=bool(r.get("portal", False)),
                markup=bool(r.get("markup", False)),
            )
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE planlimits: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
