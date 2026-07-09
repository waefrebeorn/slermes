"""
sta_oracle_whatsapp_identity.py — oracle for gateway/whatsapp_identity.to_whatsapp_jid.
"""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from gateway.whatsapp_identity import to_whatsapp_jid

lines = [l for l in sys.stdin if l.strip().startswith("{")]
mism = 0
for ln in lines:
    rec = json.loads(ln)
    inp, got = rec["in"], rec["out"]
    exp = to_whatsapp_jid(inp)
    if got != exp:
        mism += 1
        print(f"MISMATCH in={inp!r} got={got!r} exp={exp!r}")
print(f"WHATSAPP oracle: {len(lines)} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
