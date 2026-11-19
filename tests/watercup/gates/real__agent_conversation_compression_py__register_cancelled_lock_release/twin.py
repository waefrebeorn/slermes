"""twin for agent/conversation_compression.py:register_cancelled_lock_release (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "register_cancelled_lock_release"
    C_FN = "register_cancelled_lock_release"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
