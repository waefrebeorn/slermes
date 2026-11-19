"""twin for agent/empty_response_guard.py:deterministic_empty (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/empty_response_guard.py"
    PY_FN = "deterministic_empty"
    C_FN = "deterministic_empty"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
