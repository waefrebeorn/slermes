"""twin for agent/monitoring/emitter.py:_dispatch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/emitter.py"
    PY_FN = "_dispatch"
    C_FN = "_dispatch"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
