#!/usr/bin/env python3
"""Differential oracle for agent/redact_connstr.py (w7_redactconns_217bf3 gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/redact_connstr.py"
spec = importlib.util.spec_from_file_location("live_redact_connstr_w7", SRC)
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
        if func == "closes":
            py = bool(live._closes_wrapper(r.get("value", ""), int(r.get("j", 0))))
        elif func == "value_end":
            py = live._kv_value_end(r.get("value", ""), int(r.get("i", 0)), bool(r.get("semicolon", False)))
        elif func == "line":
            py = live._redact_kv_connection_line(r.get("value", ""), bool(r.get("code_file", False)))
        elif func == "strings":
            py = live._redact_kv_connection_strings(r.get("value", ""), bool(r.get("code_file", False)))
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
print(f"ORACLE w7_redactconns_217bf3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
