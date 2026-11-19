"""twin for agent/reasoning_summaries.py:separate_glued_reasoning_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_summaries.py"
    PY_FN = "separate_glued_reasoning_blocks"
    C_FN = "separate_glued_reasoning_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
