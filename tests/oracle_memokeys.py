#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/model_metadata_memo_keys.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
SRC = Q + "/agent/model_metadata_memo_keys.py"
spec = importlib.util.spec_from_file_location("memokeys_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func not in ("memo_key", "disk_slot", "fingerprint", "minted"): continue
    normalized = r.get("normalized", "")
    if func == "memo_key":
        memo = live.endpoint_memo_key(normalized, r.get("api_key"))
        py = {"normalized": memo[0], "slot": memo[1]}
    elif func == "disk_slot":
        py = live.endpoint_disk_slot((normalized, r.get("slot")))
    elif func == "fingerprint":
        py = live.fingerprint_secret_value(r.get("value"))
    else:
        py = f"minted:{int(r.get('id', '0')):x}"
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE memokeys: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
