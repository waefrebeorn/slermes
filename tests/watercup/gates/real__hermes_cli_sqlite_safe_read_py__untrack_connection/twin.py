"""twin for hermes_cli/sqlite_safe_read.py:untrack_connection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sqlite_safe_read.py"
    PY_FN = "untrack_connection"
    C_FN = "sqlite_safe_untrack_connection"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
