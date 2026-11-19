"""twin for agent/curator_backup.py:list_backups (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator_backup.py"
    PY_FN = "list_backups"
    C_FN = "list_backups"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
