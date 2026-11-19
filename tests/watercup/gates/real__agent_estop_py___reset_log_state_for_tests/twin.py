"""twin for agent/estop.py:_reset_log_state_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/estop.py"
    PY_FN = "_reset_log_state_for_tests"
    C_FN = "estop_reset_log_state_for_tests"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
