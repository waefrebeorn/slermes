"""twin for agent/conversation_compression.py:_should_suppress (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_should_suppress"
    C_FN = "_should_suppress"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
