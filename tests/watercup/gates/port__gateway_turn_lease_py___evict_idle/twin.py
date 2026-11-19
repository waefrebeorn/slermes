"""twin for gateway/turn_lease.py:_evict_idle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/turn_lease.py"
    PY_FN = "_evict_idle"
    C_FN = "gateway_turn_lease_u_evict_idle"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
