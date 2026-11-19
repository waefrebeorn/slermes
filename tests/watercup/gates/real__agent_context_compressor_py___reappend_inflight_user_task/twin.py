"""twin for agent/context_compressor.py:_reappend_inflight_user_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_reappend_inflight_user_task"
    C_FN = "_reappend_inflight_user_task"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
