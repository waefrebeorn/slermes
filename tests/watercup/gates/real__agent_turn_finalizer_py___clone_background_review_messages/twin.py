"""twin for agent/turn_finalizer.py:_clone_background_review_messages (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_finalizer.py"
    PY_FN = "_clone_background_review_messages"
    C_FN = "_clone_background_review_messages"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
