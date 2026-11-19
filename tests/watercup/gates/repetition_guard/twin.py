"""twin for agent/repetition_guard.py:is_repetition_dominated (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/repetition_guard.py"
    PY_FN = "is_repetition_dominated"
    C_FN = "is_repetition_dominated"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
