"""twin for agent/billing_view.py:can_charge (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_view.py"
    PY_FN = "can_charge"
    C_FN = "can_charge"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
