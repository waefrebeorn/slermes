#!/usr/bin/env python3
"""Differential oracle for tools/approval_wmic.py (gate w7_approvalwmi_9d4dd1).

Loads the LIVE module by path and replays its own logic: the case-insensitive
"wmic" guard, the static _carrier_script switch/title/host stripping, and the
compiled shadowcopy/delete segment pattern (with the prepended space from
search()).
"""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/approval_wmic.py"
spec = importlib.util.spec_from_file_location("w7_approvalwmi_9d4dd1_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_approvalwmi_9d4dd1_live"] = live
spec.loader.exec_module(live)

MATCHER = live.WmicShadowDeleteMatcher


def compute(r):
    func = r.get("func")
    if func == "has_wmic":
        return "wmic" in (r.get("text", "") or "").lower()
    if func == "carrier_script":
        return list(MATCHER._carrier_script(r.get("name", "") or "", r.get("rest", "") or ""))
    if func == "wmic_delete":
        seg = r.get("segment", "") or ""
        return live._WMIC_DELETE_IN_SEGMENT_RE.search(" " + seg) is not None
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
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_approvalwmi_9d4dd1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
