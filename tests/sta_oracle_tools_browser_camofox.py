"""AUTO-GENERATED oracle for tools_browser_camofox (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.browser_camofox import (_adopt_existing_tab_enabled, _loopback_rewrite_enabled, _managed_persistence_enabled, check_camofox_available)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'check_camofox_available': ('check_camofox_available', []),
        '_managed_persistence_enabled': ('_managed_persistence_enabled', []),
        '_adopt_existing_tab_enabled': ('_adopt_existing_tab_enabled', []),
        '_loopback_rewrite_enabled': ('_loopback_rewrite_enabled', []),
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
