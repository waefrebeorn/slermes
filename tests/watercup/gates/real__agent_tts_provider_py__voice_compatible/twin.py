"""twin for agent/tts_provider.py:voice_compatible (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tts_provider.py"
    PY_FN = "voice_compatible"
    C_FN = "ttp_voice_compatible"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
