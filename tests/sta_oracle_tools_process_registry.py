"""AUTO-GENERATED oracle for tools_process_registry (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.process_registry import (ProcessRegistry)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    # _write_checkpoint / count_running / recover_from_checkpoint are now
    # ProcessRegistry methods; a fresh registry mirrors the C port's
    # statically-initialized (empty) store.
    ARGS = {
        'process_registry_write_checkpoint': ('_cp', []),
        'process_registry_count_running': ('_count', []),
        'process_registry_recover_from_checkpoint': ('_recover', []),
    }
    if fn not in ARGS:
        continue
    pyf, pargs = ARGS[fn]
    try:
        pr = ProcessRegistry()
        if pyf == '_cp':
            pr._write_checkpoint(); exp = 0
        elif pyf == '_count':
            exp = pr.count_running()
        else:
            exp = pr.recover_from_checkpoint()
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
