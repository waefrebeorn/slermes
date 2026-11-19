"""twin for agent/thread_scoped_output.py:_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/thread_scoped_output.py"
    PY_FN = "_target"
    C_FN = "_target"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
