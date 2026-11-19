"""twin for gateway/session.py:_reconcile_recovered_routing_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_reconcile_recovered_routing_locked"
    C_FN = "_reconcile_recovered_routing_locked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
