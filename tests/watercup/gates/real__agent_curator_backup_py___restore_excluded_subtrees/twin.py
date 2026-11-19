"""twin for agent/curator_backup.py:_restore_excluded_subtrees (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator_backup.py"
    PY_FN = "_restore_excluded_subtrees"
    C_FN = "_restore_excluded_subtrees"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
