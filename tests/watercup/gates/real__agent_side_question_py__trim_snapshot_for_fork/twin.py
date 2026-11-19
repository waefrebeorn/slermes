"""twin for agent/side_question.py:trim_snapshot_for_fork (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/side_question.py"
    PY_FN = "trim_snapshot_for_fork"
    C_FN = "trim_snapshot_for_fork"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
