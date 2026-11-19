"""twin for tools/voice_mode.py:stop_thinking_sound (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "stop_thinking_sound"
    C_FN = "voice_stop_thinking_sound"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
