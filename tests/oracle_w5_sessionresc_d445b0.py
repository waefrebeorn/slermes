#!/usr/bin/env python3
"""Oracle for upstream Hermes session_rescue.py (w5_sessionresc_d445b0 gate).

The live function's observable is the JSONL file it appends (or None). We run
it into a fresh temp sessions_dir each case and compare its parsed records to
the harness's filesystem-independent plan.
"""
import importlib.util, json, sys, tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/session_rescue.py"
spec = importlib.util.spec_from_file_location("sessionresc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sessionresc_live"] = live
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
    if r.get("func") != "divert":
        continue
    sessions_dir = Path(tempfile.mkdtemp(prefix="sr_oracle_")) / "sessions"
    p = live.divert_session_transcript_jsonl(
        r.get("session_id", ""), r.get("messages", []), sessions_dir=sessions_dir
    )
    if p is None:
        py = None
    else:
        records = [json.loads(x) for x in Path(p).read_text(encoding="utf-8").splitlines() if x.strip()]
        py = {"name": Path(p).name, "records": records}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_sessionresc_d445b0: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
