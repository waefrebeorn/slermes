"""twin for agent/model_metadata.py:_warn_context_length_fallback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_warn_context_length_fallback"
    C_FN = "mm_warn_context_length_fallback"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
