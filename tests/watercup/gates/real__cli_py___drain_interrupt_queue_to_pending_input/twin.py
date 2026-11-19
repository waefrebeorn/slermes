"""twin for cli.py:_drain_interrupt_queue_to_pending_input (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_drain_interrupt_queue_to_pending_input"
    C_FN = "cli__drain_interrupt_queue_to_pending_input"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
