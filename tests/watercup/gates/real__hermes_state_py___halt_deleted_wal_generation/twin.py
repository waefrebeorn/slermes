"""twin for hermes_state.py:_halt_deleted_wal_generation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_halt_deleted_wal_generation"
    C_FN = "_halt_deleted_wal_generation"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
