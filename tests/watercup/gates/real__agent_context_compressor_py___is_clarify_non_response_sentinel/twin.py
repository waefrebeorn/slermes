"""twin for agent/context_compressor.py:_is_clarify_non_response_sentinel (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_is_clarify_non_response_sentinel"
    C_FN = "_is_clarify_non_response_sentinel"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
