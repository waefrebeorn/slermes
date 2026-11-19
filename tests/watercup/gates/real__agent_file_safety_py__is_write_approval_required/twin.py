"""twin for agent/file_safety.py:is_write_approval_required (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/file_safety.py"
    PY_FN = "is_write_approval_required"
    C_FN = "is_write_approval_required"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
