"""twin for tools/voice_mode.py:voice_stop_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "voice_stop_hint"
    C_FN = "voice_stop_hint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
