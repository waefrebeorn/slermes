"""twin for hermes_state.py:_exclusive_repair_db_guard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_exclusive_repair_db_guard"
    C_FN = "_exclusive_repair_db_guard"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
