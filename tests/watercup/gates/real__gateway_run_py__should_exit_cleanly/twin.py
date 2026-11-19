"""twin for gateway/run.py:should_exit_cleanly (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "should_exit_cleanly"
    C_FN = "gateway_runner_should_exit_cleanly"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
