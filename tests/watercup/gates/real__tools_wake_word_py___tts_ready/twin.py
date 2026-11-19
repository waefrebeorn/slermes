"""twin for tools/wake_word.py:_tts_ready (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/wake_word.py"
    PY_FN = "_tts_ready"
    C_FN = "ww_tts_ready"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
