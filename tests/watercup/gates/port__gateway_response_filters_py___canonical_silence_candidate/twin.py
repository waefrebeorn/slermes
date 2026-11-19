"""twin for gateway/response_filters.py:_canonical_silence_candidate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/response_filters.py"
    PY_FN = "_canonical_silence_candidate"
    C_FN = "_canonical_silence_candidate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
