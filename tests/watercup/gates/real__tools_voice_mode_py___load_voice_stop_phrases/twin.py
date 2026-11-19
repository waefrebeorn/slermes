"""twin for tools/voice_mode.py:_load_voice_stop_phrases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "_load_voice_stop_phrases"
    C_FN = "_load_voice_stop_phrases"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
