"""sta_oracle_gateway_cgroup_cleanup.py — oracle for the _own_cgroup_path
parser rule (^0::(.+)$). Replays each harness input through the equivalent
Python extraction and compares the extracted path (or None)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    rec = json.loads(line)
    buf = rec["in"]
    exp = None
    if buf:
        for ln in buf.split("\n"):
            if ln.startswith("0::"):
                exp = ln[3:].strip()
                break
    got = rec["out"]
    if got != exp:
        mism += 1
        print(f"MISMATCH in={buf!r} PY={exp!r} C={got!r}")
    n += 1
print(f"CGROUP_CLEANUP oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
