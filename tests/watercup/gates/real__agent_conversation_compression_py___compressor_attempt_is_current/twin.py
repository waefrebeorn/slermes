"""twin for agent/conversation_compression.py:_compressor_attempt_is_current (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_compressor_attempt_is_current"
    C_FN = "_compressor_attempt_is_current"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
