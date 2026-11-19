"""twin for tools/interrupt.py:clear_current_thread_interrupt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/interrupt.py"
    PY_FN = "clear_current_thread_interrupt"
    C_FN = "tools_interrupt_clear_current_thread_interrupt"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
