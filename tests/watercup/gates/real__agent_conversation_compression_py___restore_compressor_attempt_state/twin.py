"""twin for agent/conversation_compression.py:_restore_compressor_attempt_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_restore_compressor_attempt_state"
    C_FN = "_restore_compressor_attempt_state"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
