"""twin for agent/conversation_loop.py:_should_rearm_compression_budget (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_should_rearm_compression_budget"
    C_FN = "_should_rearm_compression_budget"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
