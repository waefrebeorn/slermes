"""twin for tools/async_delegation.py:complete_event_delivery (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/async_delegation.py"
    PY_FN = "complete_event_delivery"
    C_FN = "adel_complete_event_delivery"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
