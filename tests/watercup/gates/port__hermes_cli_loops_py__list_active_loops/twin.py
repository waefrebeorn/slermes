"""twin for hermes_cli/loops.py:list_active_loops (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/loops.py"
    PY_FN = "list_active_loops"
    C_FN = "loops_list_active_loops"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
