"""twin for hermes_cli/kanban.py:_cmd_boards_export (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban.py"
    PY_FN = "_cmd_boards_export"
    C_FN = "_cmd_boards_export"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
