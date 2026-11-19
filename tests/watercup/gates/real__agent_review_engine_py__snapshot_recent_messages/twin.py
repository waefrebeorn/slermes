"""twin for agent/review_engine.py:snapshot_recent_messages (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_engine.py"
    PY_FN = "snapshot_recent_messages"
    C_FN = "snapshot_recent_messages"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
