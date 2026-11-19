"""twin for hermes_cli/backup.py:_prune_pre_update_backups (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/backup.py"
    PY_FN = "_prune_pre_update_backups"
    C_FN = "_prune_pre_update_backups"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
