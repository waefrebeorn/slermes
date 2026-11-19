"""twin for hermes_cli/kanban_transfer.py:_read_board_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_transfer.py"
    PY_FN = "_read_board_metadata"
    C_FN = "_read_board_metadata"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
