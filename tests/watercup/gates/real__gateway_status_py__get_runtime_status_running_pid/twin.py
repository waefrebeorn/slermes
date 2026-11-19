"""twin for gateway/status.py:get_runtime_status_running_pid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "get_runtime_status_running_pid"
    C_FN = "gwstatus_get_runtime_status_running_pid"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
