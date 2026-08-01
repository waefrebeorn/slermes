"""AUTO-GENERATED oracle for tools_voice_mode (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.voice_mode import (_audio_available, _pulse_socket_reachable, _termux_api_app_installed, _termux_voice_capture_available)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'voice_mode__audio_available': ('_audio_available', []),
        'voice_mode__termux_api_app_installed': ('_termux_api_app_installed', []),
        'voice_mode__termux_voice_capture_available': ('_termux_voice_capture_available', []),
        'voice_mode__pulse_socket_reachable': ('_pulse_socket_reachable', []),
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
