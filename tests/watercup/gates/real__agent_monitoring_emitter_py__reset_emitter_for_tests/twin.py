"""twin for agent/monitoring/emitter.py:reset_emitter_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/emitter.py"
    PY_FN = "reset_emitter_for_tests"
    C_FN = "monitoring_emitter_reset_for_tests"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
