#!/usr/bin/env python3
"""Differential oracle for upstream Hermes tools/approval_ansi_c.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
NAME = "w8_ansi_c_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/approval_ansi_c.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "expand":
        return live.expand_ansi_c_quotes(r.get("command", ""))
    if f == "span_end":
        return live._ansi_c_span_end(r.get("text", ""), int(r.get("start", 0)))
    if f == "comment_start":
        return live._is_shell_comment_start(r.get("text", ""), int(r.get("index", 0)))
    if f == "skip_cont":
        return live._skip_continuations(r.get("text", ""), int(r.get("i", 0)))
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
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE ansi_c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)