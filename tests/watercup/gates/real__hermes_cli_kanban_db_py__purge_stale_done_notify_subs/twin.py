"""twin for hermes_cli/kanban_db.py:purge_stale_done_notify_subs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "purge_stale_done_notify_subs"
    C_FN = "purge_stale_done_notify_subs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
