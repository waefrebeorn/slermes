"""twin for agent/monitoring/emitter.py:unsubscribe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/emitter.py"
    PY_FN = "unsubscribe"
    C_FN = "unsubscribe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
