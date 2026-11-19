"""twin for gateway/platforms/base.py:_streaming_tts_turn_completed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_streaming_tts_turn_completed"
    C_FN = "stts_streaming_tts_turn_completed"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
