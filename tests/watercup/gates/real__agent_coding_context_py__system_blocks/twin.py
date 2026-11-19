"""twin for agent/coding_context.py:system_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "system_blocks"
    C_FN = "system_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
