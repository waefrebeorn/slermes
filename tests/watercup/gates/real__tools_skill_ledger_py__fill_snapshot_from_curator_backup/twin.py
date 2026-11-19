"""twin for tools/skill_ledger.py:fill_snapshot_from_curator_backup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_ledger.py"
    PY_FN = "fill_snapshot_from_curator_backup"
    C_FN = "fill_snapshot_from_curator_backup"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
