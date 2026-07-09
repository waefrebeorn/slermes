"""
sta_oracle_scale_to_zero.py — oracle for gateway/scale_to_zero._platform_name.
"""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from gateway.scale_to_zero import _platform_name

lines = [l for l in sys.stdin if l.strip().startswith("{")]
mism = 0
for ln in lines:
    rec = json.loads(ln)
    inp, got = rec["in"], rec["out"]
    # _platform_name accepts an object with .value OR a string
    exp = _platform_name(inp)
    if got != exp:
        mism += 1
        print(f"MISMATCH in={inp!r} got={got!r} exp={exp!r}")
print(f"SCALE_TO_ZERO oracle: {len(lines)} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
