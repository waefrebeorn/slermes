#!/usr/bin/env python3
"""Differential oracle for agent/redact_db_url.py (w6_redactdbur_875c0b gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/redact_db_url.py"
spec = importlib.util.spec_from_file_location("live_redact_db_url", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_redact_db_url"] = live
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
    v = r.get("value", "")
    try:
        if func == "is_userinfo_placeholder":
            py = 1 if live._is_userinfo_placeholder(v) else 0
        elif func == "is_template_value":
            py = 1 if live._is_template_value(v) else 0
        elif func == "mask_query_params":
            py = live._mask_query_params(v)
        elif func == "redact":
            py = live.redact_db_url_query_passwords(v)
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
print(f"ORACLE w6_redactdbur_875c0b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
