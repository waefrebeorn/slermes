"""twin for agent/interrupt_compat.py:_accepts_keyword (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/interrupt_compat.py"
    PY_FN = "_accepts_keyword"
    C_FN = "agent_interrupt_compat_accepts_keyword"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
