"""twin for tools/tts_text_normalize.py:smooth_whitespace_for_tts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_text_normalize.py"
    PY_FN = "smooth_whitespace_for_tts"
    C_FN = "smooth_whitespace_for_tts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
