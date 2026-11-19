"""twin for gateway/run.py:_is_duplicate_voice_transcript (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_is_duplicate_voice_transcript"
    C_FN = "_is_duplicate_voice_transcript"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
