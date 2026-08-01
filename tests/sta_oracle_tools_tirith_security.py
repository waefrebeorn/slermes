"""AUTO-GENERATED oracle for tools_tirith_security (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.tirith_security import (_clear_install_failed, _detect_target, _get_hermes_home, _is_install_failed_on_disk, _load_security_config, _read_failure_reason, _reset_spawn_warning_state, is_platform_supported)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        '_load_security_config': ('_load_security_config', []),
        '_reset_spawn_warning_state': ('_reset_spawn_warning_state', []),
        '_get_hermes_home': ('_get_hermes_home', []),
        '_read_failure_reason': ('_read_failure_reason', []),
        '_is_install_failed_on_disk': ('_is_install_failed_on_disk', []),
        '_clear_install_failed': ('_clear_install_failed', []),
        '_detect_target': ('_detect_target', []),
        'is_platform_supported': ('is_platform_supported', []),
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
