#!/usr/bin/env python3
"""Differential oracle for tools/voice_live.py (gate w6_voicelive_605a89)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/voice_live.py"
spec = importlib.util.spec_from_file_location("w6_voicelive_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_voicelive_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "voice_live_turn_note":
        return live.voice_live_turn_note(r.get("context", ""))
    if f == "voice_chat_mode":
        return live.voice_chat_mode(r.get("voice", {}))
    if f == "live_instructions":
        return live.live_instructions(r.get("live", {}))
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
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_voicelive_605a89: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
