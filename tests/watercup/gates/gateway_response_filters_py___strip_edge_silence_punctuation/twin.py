"""twin for gateway/response_filters.py:_strip_edge_silence_punctuation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/response_filters.py"
    PY_FN = "_strip_edge_silence_punctuation"
    C_FN = "gateway_response_filters_u_strip_edge_silence_punctuation"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
