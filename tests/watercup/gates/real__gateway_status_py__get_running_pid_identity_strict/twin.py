"""twin for gateway/status.py:get_running_pid_identity_strict (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "get_running_pid_identity_strict"
    C_FN = "get_running_pid_identity_strict"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
