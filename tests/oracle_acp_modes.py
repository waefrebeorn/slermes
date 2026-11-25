#!/usr/bin/env python3
"""Oracle for upstream Hermes acp_adapter/modes.py tables and lookups."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/acp_adapter/modes.py"
spec = importlib.util.spec_from_file_location("acp_modes_live", SRC)
live = importlib.util.module_from_spec(spec)
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
    if func == "constants":
        py = {
            "DEFAULT_MODE": live.DEFAULT_MODE,
            "DEFAULT_EDIT_APPROVAL_POLICY": live.DEFAULT_EDIT_APPROVAL_POLICY,
            "EDIT_APPROVAL_POLICY_CONFIG_ID": live.EDIT_APPROVAL_POLICY_CONFIG_ID,
        }
    elif func == "modes":
        py = {k: list(v) for k, v in live.MODES.items()}
    elif func == "mode_to_policy":
        py = {k: v[0] for k, v in live.MODES.items()}
    elif func == "policy_to_mode":
        py = {v[0]: k for k, v in live.MODES.items()}
    elif func == "policy_for_mode":
        py = live.MODE_TO_EDIT_APPROVAL_POLICY.get(r.get("mode"), r.get("fallback"))
    elif func == "mode_for_policy":
        py = live.EDIT_APPROVAL_POLICY_TO_MODE.get(r.get("policy"))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE acp_modes: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
