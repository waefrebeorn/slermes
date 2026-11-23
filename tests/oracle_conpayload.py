#!/usr/bin/env python3
"""Oracle for tui_gateway/connector_payload.py (connector_ui_payload)."""
import importlib.util, json, sys
sys.path.insert(0, r"C:\Users\eman5\hermes-agent")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/tui_gateway/connector_payload.py"
spec = importlib.util.spec_from_file_location("conpayload_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

def canon(o):
    return json.dumps(o, sort_keys=True, ensure_ascii=False, separators=(",", ":"))

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "payload":
        continue
    py = live.connector_ui_payload(r.get("value"))
    got = r.get("out")
    cases += 1
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH conpayload case={r.get('case')} py={canon(py)} c={canon(got)}", file=sys.stderr)
print(f"ORACLE conpayload: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
