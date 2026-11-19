"""twin for agent/review_idle_queue.py:note_turn_started (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_idle_queue.py"
    PY_FN = "note_turn_started"
    C_FN = "note_turn_started"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
