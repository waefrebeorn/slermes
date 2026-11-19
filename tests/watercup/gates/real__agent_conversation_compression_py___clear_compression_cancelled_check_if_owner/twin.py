"""twin for agent/conversation_compression.py:_clear_compression_cancelled_check_if_owner (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_clear_compression_cancelled_check_if_owner"
    C_FN = "_clear_compression_cancelled_check_if_owner"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
