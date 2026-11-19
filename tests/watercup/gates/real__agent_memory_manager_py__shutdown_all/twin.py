"""twin for agent/memory_manager.py:shutdown_all (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/memory_manager.py"
    PY_FN = "shutdown_all"
    C_FN = "gw_platform_shutdown_all"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
