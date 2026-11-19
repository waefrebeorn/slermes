"""twin for agent/moa_loop.py:peel_reference_guidance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moa_loop.py"
    PY_FN = "peel_reference_guidance"
    C_FN = "peel_reference_guidance"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
