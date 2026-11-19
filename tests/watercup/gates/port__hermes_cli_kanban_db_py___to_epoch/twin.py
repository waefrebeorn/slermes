"""twin for hermes_cli/kanban_db.py:_to_epoch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_to_epoch"
    C_FN = "kb_to_epoch"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
