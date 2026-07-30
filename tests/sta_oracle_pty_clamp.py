"""
sta_oracle_pty_clamp.py — oracle for pty bridge clamps.
Mirrors the Python int() coercion: non-int -> _MIN_DIMENSION(1), else clamp.
"""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))

lines = [l for l in sys.stdin if l.strip().startswith("{")]
mism = 0
n = 0
for ln in lines:
    rec = json.loads(ln)
    fn, value, maximum, got = rec["fn"], rec["value"], rec["maximum"], rec["out"]
    # Replicate int(value) with TypeError/ValueError/OverflowError -> 1, then clamp.
    try:
        v = int(value)
    except (TypeError, ValueError, OverflowError):
        v = 1
    if v < 1: v = 1
    if v > maximum: v = maximum
    if got != v:
        mism += 1
        print(f"MISMATCH fn={fn} value={value} max={maximum} got={got} exp={v}")
    n += 1
print(f"PTY_CLAMP oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
