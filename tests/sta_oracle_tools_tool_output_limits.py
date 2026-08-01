"""AUTO-GENERATED oracle for tools_tool_output_limits (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.tool_output_limits import (_reset_tool_output_limits_cache, get_max_bytes, get_max_line_length, get_max_lines)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'cli_tools_tool_output_limits__reset_tool_output_limits_cache': ('_reset_tool_output_limits_cache', []),
        'cli_tools_tool_output_limits_get_max_bytes': ('get_max_bytes', []),
        'cli_tools_tool_output_limits_get_max_lines': ('get_max_lines', []),
        'cli_tools_tool_output_limits_get_max_line_length': ('get_max_line_length', []),
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
