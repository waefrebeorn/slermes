#!/usr/bin/env python3
"""Differential oracle for agent/redact_vault_values.py (gate w11_redactvaul_87f037).

Imports the LIVE module with a stub hermes_constants seam (profile home is
injected, never read from the real environment).  Registry state is shared
across fixture lines in both processes, so register/clear/redact sequences
are compared faithfully.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

hc = types.ModuleType("hermes_constants")
hc.get_hermes_home = lambda: "/probe/home"
hc.hermes_home_key = lambda home=None: "scope"
sys.modules["hermes_constants"] = hc

SRC = QUARRY + "/agent/redact_vault_values.py"
spec = importlib.util.spec_from_file_location("w11_redactvaul_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_redactvaul_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "register":
        return live.register_vault_redaction_value(r.get("value"))
    if func == "clear":
        return live.clear_vault_redaction_values()
    if func == "redact":
        return live.redact_registered_vault_values(r.get("text"))
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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w11_redactvaul_87f037: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w11_redactvaul_87f037: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
