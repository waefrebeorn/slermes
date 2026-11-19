"""twin for agent/relay_llm.py:_recover_successful_callback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_llm.py"
    PY_FN = "_recover_successful_callback"
    C_FN = "_recover_successful_callback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
