#!/usr/bin/env python3
"""Differential oracle for w11_automation_a6ffec (session_rules/automation.py)."""
import copy
import dataclasses
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/session_rules/automation.py"
spec = importlib.util.spec_from_file_location("w11_automation_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_automation_live"] = live
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
        if func == "descriptor":
            py = live.automation_descriptor(r.get("payload", {}))
        elif func == "folded_payload":
            py = live.folded_payload(copy.deepcopy(r.get("payload", {})))
        elif func == "goal_contract":
            py = live.GoalContract.from_dict(r.get("data")).to_dict()
        elif func == "goal_contract_empty":
            py = live.GoalContract.from_dict(r.get("data")).is_empty()
        elif func == "goal_gate":
            py = live.GoalGate.from_dict(r.get("data")).to_dict()
        elif func == "heartbeat_state":
            py = dataclasses.asdict(live.HeartbeatState.from_json(json.dumps(r.get("data", {}))))
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
print(f"ORACLE w11_automation_a6ffec: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
