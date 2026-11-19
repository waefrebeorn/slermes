"""twin for gateway/platforms/base.py:_mark_streaming_tts_completed_turn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_mark_streaming_tts_completed_turn"
    C_FN = "stts_mark_streaming_tts_completed_turn"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
