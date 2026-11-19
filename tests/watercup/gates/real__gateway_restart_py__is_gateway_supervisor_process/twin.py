"""twin for gateway/restart.py:is_gateway_supervisor_process (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/restart.py"
    PY_FN = "is_gateway_supervisor_process"
    C_FN = "gateway_restart_is_gateway_supervisor_process"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
