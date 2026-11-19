"""twin for agent/anthropic_adapter.py:_is_stream_unavailable_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_is_stream_unavailable_error"
    C_FN = "antr_is_stream_unavailable_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
