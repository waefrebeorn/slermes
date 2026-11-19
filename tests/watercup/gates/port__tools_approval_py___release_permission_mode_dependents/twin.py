"""twin for tools/approval.py:_release_permission_mode_dependents (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_release_permission_mode_dependents"
    C_FN = "apr2_release_permission_mode_dependents"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
