"""twin for tools/transcription_tools.py:_is_hallucinated_segment (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_is_hallucinated_segment"
    C_FN = "_is_hallucinated_segment"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
