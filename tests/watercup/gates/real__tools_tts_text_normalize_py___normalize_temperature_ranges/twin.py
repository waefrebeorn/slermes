"""twin for tools/tts_text_normalize.py:_normalize_temperature_ranges (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_text_normalize.py"
    PY_FN = "_normalize_temperature_ranges"
    C_FN = "_normalize_temperature_ranges"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
