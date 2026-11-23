#!/usr/bin/env python3
"""Differential oracle for agent/redact_control_split.py (w13_redactcont_3747d1).

Pattern seam: prefix_re is built as re.compile("(" + re.escape(prefix) +
"[A-Za-z0-9_+\\-./]+)"), mirroring the body-class family the C scanner implements;
sub_re=None and skip=None. Fixtures use only ASCII control chars so codepoint
indices match C byte indices.
"""
import importlib.util
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/agent/redact_control_split.py"
spec = importlib.util.spec_from_file_location("redact_control_split_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["redact_control_split_live"] = live
spec.loader.exec_module(live)


def pattern(prefix):
    return re.compile("(" + re.escape(prefix) + "[A-Za-z0-9_+\\-./]+)")


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
    text = r.get("text", "")
    try:
        if func == "strip_shadow":
            shadow, noise, orig = live._strip_shadow(text)
            py = {"shadow": shadow, "noise": list(noise), "orig_idx": list(orig)}
        elif func == "union":
            spans = [(int(s[0]), int(s[1]), bool(s[2])) for s in r.get("spans") or []]
            py = [list(x) for x in live._union(spans)]
        elif func == "mask_split":
            pat = pattern(r.get("prefix", ""))
            py = live.mask_control_split_tokens(text, lambda s: r.get("mask", ""), pat)
        elif func == "mask_prefix":
            pat = pattern(r.get("prefix", ""))
            py = live.mask_prefix_matches(text, lambda s: r.get("mask", ""), pat, lambda m: False)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_redactcont_3747d1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
