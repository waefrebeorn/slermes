"""twin for hermes_cli/session_lost_and_found.py:_copy_direct_tables (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_lost_and_found.py"
    PY_FN = "_copy_direct_tables"
    C_FN = "_copy_direct_tables"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
