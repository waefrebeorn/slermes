"""twin for agent/fast_mode.py:effective_request_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/fast_mode.py"
    PY_FN = "effective_request_overrides"
    C_FN = "effective_request_overrides"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
