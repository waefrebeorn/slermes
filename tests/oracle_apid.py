#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/platforms/api_server_response_identity.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/api_server_response_identity.py"
spec = importlib.util.spec_from_file_location("api_server_response_identity_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["api_server_response_identity_live"] = live
spec.loader.exec_module(live)

def compute(r):
    func = r.get("func")
    if func == "durable_response_id":
        return live.durable_response_id(r.get("request_key", ""))
    if func == "item_id_sequence":
        rid = r.get("response_id", "")
        counters = {}
        ids = []
        for c in r.get("calls", []):
            ids.append(live.next_item_id(
                rid, counters, c["type"],
                call_id=c.get("call_id"),
                commentary=bool(c.get("commentary", False))))
        return ids
    if func == "identify_output":
        return live.identify_output(r.get("items", []), r.get("response_id", ""))
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
print(f"ORACLE apid: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
