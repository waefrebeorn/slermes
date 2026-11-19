"""twin for hermes_state.py:_repair_failure_consumes_attempt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_repair_failure_consumes_attempt"
    C_FN = "_repair_failure_consumes_attempt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
