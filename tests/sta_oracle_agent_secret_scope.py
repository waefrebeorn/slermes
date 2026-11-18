"""AUTO-GENERATED oracle for agent_secret_scope (gen_oracle.py)."""
import os
import sys, json, os
for _quarry in ("~/hermes-agent", "~/hermes-agent-dev"):
    _p = os.path.expanduser(_quarry)
    if os.path.isdir(os.path.join(_p, "agent")):
        sys.path.insert(0, _p)
        break
from agent.secret_scope import (is_multiplex_active)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'secret_scope_is_multiplex_active': ('is_multiplex_active', []),
    }
    if fn not in ARGS:
        continue
    pyf, pargs = ARGS[fn]
    try:
        exp = globals()[pyf](*pargs)
    except Exception as e:
        print('PYERR', fn, e); continue
    got = rec['ret']
    if isinstance(exp, str): exp = exp
    if got != exp:
        # loose compare for floats / bool/int
        ok = False
        try:
            if abs(float(got) - float(exp)) < 1e-6: ok = True
        except Exception: pass
        if not ok:
            mism += 1
            print('MISMATCH', fn, 'got', got, 'exp', exp)
print("AUTO oracle: %d cases, %d mismatches" % (n, mism))
sys.exit(1 if mism else 0)
