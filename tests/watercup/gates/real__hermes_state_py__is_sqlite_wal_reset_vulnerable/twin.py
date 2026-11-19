"""twin for hermes_state.py:is_sqlite_wal_reset_vulnerable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "is_sqlite_wal_reset_vulnerable"
    C_FN = "is_sqlite_wal_reset_vulnerable"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
