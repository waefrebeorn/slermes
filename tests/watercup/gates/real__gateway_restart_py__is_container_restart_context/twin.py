"""twin for gateway/restart.py:is_container_restart_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/restart.py"
    PY_FN = "is_container_restart_context"
    C_FN = "gw_is_container_restart_context"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
