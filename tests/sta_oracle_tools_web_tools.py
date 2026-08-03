"""AUTO-GENERATED oracle for tools_web_tools (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.web_tools import (_ddgs_package_importable, _ensure_web_plugins_loaded, _get_extract_char_limit, check_web_api_key)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'web_ensure_web_plugins_loaded': ('_ensure_web_plugins_loaded', []),
        'web_ddgs_package_importable': ('_ddgs_package_importable', []),
        'web_get_extract_char_limit': ('_get_extract_char_limit', []),
        'web_check_web_api_key': ('check_web_api_key', []),
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
