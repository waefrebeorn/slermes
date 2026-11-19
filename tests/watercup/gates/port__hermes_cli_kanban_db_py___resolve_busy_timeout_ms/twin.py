"""twin for hermes_cli/kanban_db.py:_resolve_busy_timeout_ms (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_resolve_busy_timeout_ms"
    C_FN = "kdb_now"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
