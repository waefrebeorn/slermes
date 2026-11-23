#!/usr/bin/env python3
"""Oracle for upstream Hermes tools/tool_output_truncate.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/tools/tool_output_truncate.py"
spec = importlib.util.spec_from_file_location("touttrunc_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func == "notice":
        py = live.truncation_notice(r["omitted"], r["total"], label=r.get("label","OUTPUT"), unit=r.get("unit","chars"))
    elif func == "split":
        py = list(live.head_tail_split(r["budget"]))
    elif func == "truncate":
        py = live.truncate_head_tail(r["text"], r["max_chars"], label=r.get("label","OUTPUT"))
    else: continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE touttrunc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
