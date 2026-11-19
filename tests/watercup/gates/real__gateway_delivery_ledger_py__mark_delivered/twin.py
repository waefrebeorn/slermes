"""twin for gateway/delivery_ledger.py:mark_delivered (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery_ledger.py"
    PY_FN = "mark_delivered"
    C_FN = "gateway_delivery_ledger_mark_delivered"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
