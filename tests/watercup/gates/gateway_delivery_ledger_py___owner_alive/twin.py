"""twin for gateway/delivery_ledger.py:_owner_alive (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery_ledger.py"
    PY_FN = "_owner_alive"
    C_FN = "gateway_delivery_ledger_u_owner_alive"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
