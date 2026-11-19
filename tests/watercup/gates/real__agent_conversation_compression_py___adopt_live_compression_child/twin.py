"""twin for agent/conversation_compression.py:_adopt_live_compression_child (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_adopt_live_compression_child"
    C_FN = "_adopt_live_compression_child"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
