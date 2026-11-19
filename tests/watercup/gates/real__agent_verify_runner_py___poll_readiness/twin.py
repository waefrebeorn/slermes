"""twin for agent/verify/runner.py:_poll_readiness (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify/runner.py"
    PY_FN = "_poll_readiness"
    C_FN = "_poll_readiness"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
