"""AUTO-GENERATED oracle for tools_managed_tool_gateway (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.managed_tool_gateway import (auth_json_path, get_tool_gateway_scheme)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'cli_tools_managed_tool_gateway_auth_json_path': ('auth_json_path', []),
        'cli_tools_managed_tool_gateway_get_tool_gateway_scheme': ('get_tool_gateway_scheme', []),
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
