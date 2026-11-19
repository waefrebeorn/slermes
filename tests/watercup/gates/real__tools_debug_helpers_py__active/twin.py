"""twin for tools/debug_helpers.py:active (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/debug_helpers.py"
    PY_FN = "active"
    C_FN = "dbh_active"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
