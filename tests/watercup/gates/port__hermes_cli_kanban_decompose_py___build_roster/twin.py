"""twin for hermes_cli/kanban_decompose.py:_build_roster (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_decompose.py"
    PY_FN = "_build_roster"
    C_FN = "kdb_build_roster"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
