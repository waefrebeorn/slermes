"""twin for agent/gemini_native_adapter.py:_advance_stream_iterator (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/gemini_native_adapter.py"
    PY_FN = "_advance_stream_iterator"
    C_FN = "_advance_stream_iterator"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
