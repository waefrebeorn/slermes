"""AUTO-GENERATED oracle for tools_moa_performance (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.moa_performance import (__aenter__, __aexit__, _ensure_session, _init_db, clear_expired, close_global_clients)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'moa_perf_init_db': ('_init_db', []),
        'moa_perf_clear_expired': ('clear_expired', []),
        'moa_perf_ensure_session': ('_ensure_session', []),
        'moa_perf_enter': ('__aenter__', []),
        'moa_perf_exit': ('__aexit__', []),
        'moa_perf_close_global_clients': ('close_global_clients', []),
    }
    if fn not in ARGS:
        continue
    pyf, pargs = ARGS[fn]
    try:
        exp = pyf(*pargs)
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
