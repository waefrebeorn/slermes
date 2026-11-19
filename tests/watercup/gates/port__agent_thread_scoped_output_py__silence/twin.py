"""twin for agent/thread_scoped_output.py:silence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/thread_scoped_output.py"
    PY_FN = "silence"
    C_FN = "continuous_on_silence"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
