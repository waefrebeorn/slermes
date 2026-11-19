"""twin for hermes_cli/kanban_transfer.py:_relocate_imported_rows (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_transfer.py"
    PY_FN = "_relocate_imported_rows"
    C_FN = "_relocate_imported_rows"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
