"""twin for agent/conversation_loop.py:_provider_overflow_exhausted_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_provider_overflow_exhausted_result"
    C_FN = "_provider_overflow_exhausted_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
