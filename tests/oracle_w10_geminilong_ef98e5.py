#!/usr/bin/env python3
"""Differential oracle for live agent/gemini_long_decode.py (w10_geminilong_ef98e5)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/agent/gemini_long_decode.py"
spec = importlib.util.spec_from_file_location("gld_ef98e5_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["gld_ef98e5_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "max_output_tokens":
        return live.long_decode_max_output_tokens(r.get("model"), r.get("config"))
    if f == "hops":
        return live.long_decode_hops(r.get("value"))
    if f == "stale_multiplier":
        return live.long_decode_stale_multiplier(r.get("payload"))
    if f == "continuation_token":
        return live.continuation_token(r.get("payload"))
    if f == "merge_hop_usage":
        return live.merge_hop_usage(r.get("hops", []))
    if f == "merge_hop_responses":
        return live.merge_hop_responses(r.get("hops", []))
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
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w10_geminilong_ef98e5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
