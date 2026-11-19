"""twin for hermes_cli/kanban_db.py:_mark_scratch_tip_shown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_mark_scratch_tip_shown"
    C_FN = "kdb_mark_scratch_tip_shown"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
