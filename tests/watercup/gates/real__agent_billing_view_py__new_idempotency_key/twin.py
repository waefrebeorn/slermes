"""twin for agent/billing_view.py:new_idempotency_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_view.py"
    PY_FN = "new_idempotency_key"
    C_FN = "new_idempotency_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
