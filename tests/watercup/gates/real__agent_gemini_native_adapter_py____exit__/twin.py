"""twin for agent/gemini_native_adapter.py:__exit__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/gemini_native_adapter.py"
    PY_FN = "__exit__"
    C_FN = "gemini_native_adapter__exit"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
