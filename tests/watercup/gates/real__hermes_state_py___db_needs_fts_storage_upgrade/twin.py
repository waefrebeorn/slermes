"""twin for hermes_state.py:_db_needs_fts_storage_upgrade (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_db_needs_fts_storage_upgrade"
    C_FN = "_db_needs_fts_storage_upgrade"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
