"""twin for agent/relay_runtime.py:_run_bounded_on_exit_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "_run_bounded_on_exit_thread"
    C_FN = "_run_bounded_on_exit_thread"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
