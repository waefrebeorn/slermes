#!/usr/bin/env python3
"""Differential oracle for agent/memory_context.py (w7_memoryconte_c58d5c gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/memory_context.py"
spec = importlib.util.spec_from_file_location("live_memory_context_w7", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
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
    try:
        if func == "fence":
            py = live.fence_recalled_context(r.get("value", ""))
        elif func == "dedupe":
            py = live._drop_repeated_recall_lines(r.get("value", ""))
        elif func == "stream":
            scrub = live.StreamingContextScrubber()
            py = "".join([scrub.feed(c) for c in r.get("chunks", [])] + [scrub.flush()])
        elif func == "block":
            py = live.build_memory_context_block(r.get("value", ""))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_memoryconte_c58d5c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
