#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/jobs_normalize.py (jobsnorm gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)  # resolve quarry modules, not the slermes shadow copy
SRC = PYROOT + "/cron/jobs_normalize.py"
spec = importlib.util.spec_from_file_location("jobsnorm_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

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
    if func == "opt":
        py = live._normalize_job_optional_text(
            r.get("value"), strip_trailing_slash=bool(r.get("strip", False)))
    elif func == "base":
        py = live._normalize_base_url(r.get("value"))
    elif func == "str_list":
        py = live._normalize_str_list(r.get("items"))
    elif func == "context":
        py = live._normalize_context_from(r.get("value"))
    elif func == "failure":
        py = live._normalize_failure_deliver(r.get("value"))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE jobsnorm: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
