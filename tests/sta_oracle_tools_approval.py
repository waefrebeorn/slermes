"""AUTO-GENERATED oracle for tools_approval (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.approval import (_YOLO_MODE_FROZEN, _is_gateway_approval_context, _is_interactive_cli, disable_session_yolo, is_approval_bypass_active, is_current_session_yolo_enabled, load_permanent, reset_current_observability_context, reset_current_session_key, unregister_gateway_notify)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'is_yolo_mode_frozen': ('_YOLO_MODE_FROZEN', []),
        'is_approval_bypass_active': ('is_approval_bypass_active', []),
        '_is_interactive_cli': ('_is_interactive_cli', []),
        'approval_reset_current_session_key': ('reset_current_session_key', []),
        'approval_reset_current_observability_context': ('reset_current_observability_context', []),
        'approval__is_gateway_approval_context': ('_is_gateway_approval_context', []),
        'approval_disable_session_yolo': ('disable_session_yolo', []),
        'approval_is_current_session_yolo_enabled': ('is_current_session_yolo_enabled', []),
        'approval_load_permanent': ('load_permanent', []),
        'approval_unregister_gateway_notify': ('unregister_gateway_notify', []),
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
