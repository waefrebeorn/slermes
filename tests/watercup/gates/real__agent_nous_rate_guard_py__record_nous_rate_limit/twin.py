"""twin for agent/nous_rate_guard.py:record_nous_rate_limit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/nous_rate_guard.py"
    PY_FN = "record_nous_rate_limit"
    C_FN = "nrg_record_nous_rate_limit"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
