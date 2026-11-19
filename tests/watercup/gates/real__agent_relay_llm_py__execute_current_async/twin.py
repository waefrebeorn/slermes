"""twin for agent/relay_llm.py:execute_current_async (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_llm.py"
    PY_FN = "execute_current_async"
    C_FN = "execute_current_async"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
