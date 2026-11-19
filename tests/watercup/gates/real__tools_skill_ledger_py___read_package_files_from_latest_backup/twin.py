"""twin for tools/skill_ledger.py:_read_package_files_from_latest_backup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_ledger.py"
    PY_FN = "_read_package_files_from_latest_backup"
    C_FN = "_read_package_files_from_latest_backup"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
