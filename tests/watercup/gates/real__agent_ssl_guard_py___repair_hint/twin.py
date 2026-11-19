"""twin for agent/ssl_guard.py:_repair_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/ssl_guard.py"
    PY_FN = "_repair_hint"
    C_FN = "_repair_hint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
