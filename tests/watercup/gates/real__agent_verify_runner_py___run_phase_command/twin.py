"""twin for agent/verify/runner.py:_run_phase_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify/runner.py"
    PY_FN = "_run_phase_command"
    C_FN = "_run_phase_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
