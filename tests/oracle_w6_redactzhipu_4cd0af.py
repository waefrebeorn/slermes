#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/redact_zhipu.py (gate w6_redactzhipu_4cd0af).

Compares our C re.finditer port against the LIVE compiled regex by reading the
same fixture lines the C harness consumes.
"""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

spec = importlib.util.spec_from_file_location(
    "live_redact_zhipu", QUARRY + "/agent/redact_zhipu.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_redact_zhipu"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func != "find":
        continue
    text = r.get("text", "")
    py = [m.group(1) for m in live._ZHIPU_API_KEY_RE.finditer(text)]
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH case=%s text=%r py=%r c=%r" % (r.get("case"), text, py, got), file=sys.stderr)

print("ORACLE w6_redactzhipu_4cd0af: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
