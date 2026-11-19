"""twin for hermes_cli/kanban_db.py:_retag_legacy_worker_sessions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_retag_legacy_worker_sessions"
    C_FN = "kdb_retag_legacy_worker_sessions"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
