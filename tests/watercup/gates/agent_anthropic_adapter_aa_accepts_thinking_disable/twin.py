"""twin for agent/anthropic_adapter.py:_accepts_thinking_disable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_accepts_thinking_disable"
    C_FN = "_accepts_thinking_disable"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
