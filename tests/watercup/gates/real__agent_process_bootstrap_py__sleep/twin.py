"""twin for agent/process_bootstrap.py:sleep (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/process_bootstrap.py"
    PY_FN = "sleep"
    C_FN = "sleep"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
