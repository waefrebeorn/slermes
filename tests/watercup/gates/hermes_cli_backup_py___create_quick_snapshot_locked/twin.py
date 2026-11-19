"""twin for hermes_cli/backup.py:_create_quick_snapshot_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/backup.py"
    PY_FN = "_create_quick_snapshot_locked"
    C_FN = "hermes_cli_backup_u_create_quick_snapshot_locked"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
