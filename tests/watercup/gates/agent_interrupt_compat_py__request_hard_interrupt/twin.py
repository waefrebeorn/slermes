"""twin for agent/interrupt_compat.py:request_hard_interrupt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/interrupt_compat.py"
    PY_FN = "request_hard_interrupt"
    C_FN = "agent_interrupt_compat_request_hard_interrupt"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
