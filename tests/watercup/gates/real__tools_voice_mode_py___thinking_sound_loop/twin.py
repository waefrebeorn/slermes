"""twin for tools/voice_mode.py:_thinking_sound_loop (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "_thinking_sound_loop"
    C_FN = "_thinking_sound_loop"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
