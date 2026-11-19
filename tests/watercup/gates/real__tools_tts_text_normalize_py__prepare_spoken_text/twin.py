"""twin for tools/tts_text_normalize.py:prepare_spoken_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_text_normalize.py"
    PY_FN = "prepare_spoken_text"
    C_FN = "prepare_spoken_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
