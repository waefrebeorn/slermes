"""twin for gateway/delivery_ledger.py:record_obligation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery_ledger.py"
    PY_FN = "record_obligation"
    C_FN = "gateway_delivery_ledger_record_obligation"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
