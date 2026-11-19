"""twin for agent/verify/runner.py:run_verify (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify/runner.py"
    PY_FN = "run_verify"
    C_FN = "run_verify"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
