"""twin for tools/write_approval.py:skill_pending_diff (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/write_approval.py"
    PY_FN = "skill_pending_diff"
    C_FN = "skill_pending_diff"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
