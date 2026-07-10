#!/usr/bin/env python3
"""Oracle for v555 browser_redact extraction: C == LIVE agent.redact.

Reads JSON lines from tests/t_port_browser_redact.c and asserts C output
equals what LIVE agent/redact.py returns (redact_sensitive_text /
redact_cdp_url). Behavior-contract + exact-match; any divergence fails.

Run:  gcc -O2 -g -I include -I src/tools -I lib/libregex \
        tests/t_port_browser_redact.c src/tools/browser_redact.o -o /tmp/t_br \
      && /tmp/t_br | python3 tests/sta_oracle_browser_redact.py
"""
import json, sys

# import LIVE Python source of truth
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import agent.redact as redact


def redact_secret(value: str) -> str:
    return redact.redact_sensitive_text(value, force=True)


def redact_cdp(value: str) -> str:
    return redact.redact_cdp_url(value)


EXP = {"sensitive": redact_secret, "cdp": redact_cdp}

passed = 0
failed = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    rec = json.loads(line)
    fn, in_s, out_s = rec["fn"], rec.get("in"), rec.get("out")
    try:
        exp = EXP[fn](in_s)
    except Exception as e:  # noqa: BLE001
        print(f"PYTHON ERROR {fn} in={in_s!r}: {e}")
        failed += 1
        continue
    if out_s == exp:
        passed += 1
    else:
        failed += 1
        print(f"MISMATCH {fn}")
        print(f"  in : {in_s!r}")
        print(f"  C  : {out_s!r}")
        print(f"  PY : {exp!r}")

print(f"\n{passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
