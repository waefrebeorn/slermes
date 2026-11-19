"""twin for hermes_state.py:set_compression_fallback_streak (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "set_compression_fallback_streak"
    C_FN = "set_compression_fallback_streak"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
