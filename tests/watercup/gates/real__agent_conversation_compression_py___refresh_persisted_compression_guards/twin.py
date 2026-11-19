"""twin for agent/conversation_compression.py:_refresh_persisted_compression_guards (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_refresh_persisted_compression_guards"
    C_FN = "_refresh_persisted_compression_guards"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
