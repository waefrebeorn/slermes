"""twin for tools/skill_ledger.py:_strip_archive_timestamp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_ledger.py"
    PY_FN = "_strip_archive_timestamp"
    C_FN = "_strip_archive_timestamp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
