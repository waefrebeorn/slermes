"""twin for hermes_state.py:_persistent_repair_attempts_exhausted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_persistent_repair_attempts_exhausted"
    C_FN = "_persistent_repair_attempts_exhausted"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
