"""twin for agent/think_scrubber.py:reset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/think_scrubber.py"
    PY_FN = "reset"
    C_FN = "reset"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
