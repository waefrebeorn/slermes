"""twin for gateway/session.py:_has_active_processes_safe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_has_active_processes_safe"
    C_FN = "_has_active_processes_safe"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
