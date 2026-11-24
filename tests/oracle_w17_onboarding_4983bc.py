#!/usr/bin/env python3
"""Oracle for gate w17_onboarding_4983bc (session_rules/onboarding_flow.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("session_rules.onboarding_flow", "session_rules/onboarding_flow.py")

def _facts(r):
    data = r.get("facts") or {}
    return live.Facts.from_stored(data if isinstance(data, dict) else {})

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
    if func == "next_beat":
        py = live.next_beat(r.get("record"))
    elif func == "is_active":
        py = live.is_active(r.get("record"))
    elif func == "applies_to":
        py = live.applies_to(r.get("tool", ""))
    elif func == "has_suggestions":
        py = live._has_suggestions(r.get("suggest"))
    elif func == "is_names":
        py = live._is_names(r.get("value"))
    elif func == "valid_answer":
        picks = r.get("picks") or {}
        py = live.valid_answer(r.get("beat", ""), r.get("answer") or {}, _facts(r), picks if isinstance(picks, dict) else {})
    elif func == "build_values":
        picks = r.get("picks") or {}
        py = list(live.build_values(_facts(r), picks if isinstance(picks, dict) else {}))
    elif func == "constants":
        py = {
            "PENDING": live.PENDING, "SHOWN": live.SHOWN, "DONE": live.DONE,
            "SKIPPED": live.SKIPPED, "ONBOARDING": live.ONBOARDING,
            "COMPLETE": live.COMPLETE, "PHASE_SKIPPED": live.PHASE_SKIPPED,
            "CARD_TIMEOUT_S": live.CARD_TIMEOUT_S, "MAX_PLUGINS": live.MAX_PLUGINS,
            "FIXED_ID": live.FIXED_ID, "OWN_ID": live.OWN_ID,
            "BEAT_IDS": list(live.BEAT_IDS),
            "LAYOUT_IDS": [o["id"] for o in live.LAYOUT_OPTIONS],
            "TOUR_IDS": [o["id"] for o in live.TOUR_OPTIONS],
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_onboarding_4983bc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
