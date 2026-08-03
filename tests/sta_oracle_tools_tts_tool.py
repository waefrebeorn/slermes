"""AUTO-GENERATED oracle for tools_tts_tool (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.tts_tool import (_has_openai_audio_backend, _import_edge_tts, _import_elevenlabs, _import_kittentts, _import_mistral_client, _import_openai_client, _import_piper, _import_sounddevice)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'tts_tool_import_edge_tts': ('_import_edge_tts', []),
        'tts_tool_import_elevenlabs': ('_import_elevenlabs', []),
        'tts_tool_import_openai_client': ('_import_openai_client', []),
        'tts_tool_import_mistral_client': ('_import_mistral_client', []),
        'tts_tool_import_sounddevice': ('_import_sounddevice', []),
        'tts_tool_import_kittentts': ('_import_kittentts', []),
        'tts_tool_import_piper': ('_import_piper', []),
        'tts_tool_has_openai_audio_backend': ('_has_openai_audio_backend', []),
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
