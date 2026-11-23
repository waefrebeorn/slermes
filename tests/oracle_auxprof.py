#!/usr/bin/env python3
"""Oracle for agent/auxiliary_profile_gates.py (gate auxprof).

contains_profile_reasoning_fields is compared against the live recursive
predicate.  profile_forwards_max_tokens is resolved by seeding a fake
`providers` module: the C side receives the already-resolved profile facts
(profile_known / forwards), and the live function is driven with the same
facts through a stub get_provider_profile.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/auxiliary_profile_gates.py"
spec = importlib.util.spec_from_file_location("auxprof_live", SRC)
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
    if r.get("func") == "contains":
        py = bool(live.contains_profile_reasoning_fields(r.get("value")))
    elif r.get("func") == "forwards":
        known = bool(r.get("profile_known", False))
        fwd = bool(r.get("forwards", False))
        fake = types.ModuleType("providers")
        fake.get_provider_profile = (
            (lambda name: types.SimpleNamespace(forwards_max_tokens=fwd))
            if known else (lambda name: None))
        saved = sys.modules.get("providers")
        sys.modules["providers"] = fake
        try:
            py = bool(live.profile_forwards_max_tokens(r.get("provider", "")))
        finally:
            if saved is None:
                sys.modules.pop("providers", None)
            else:
                sys.modules["providers"] = saved
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != bool(got):
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE auxprof: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
