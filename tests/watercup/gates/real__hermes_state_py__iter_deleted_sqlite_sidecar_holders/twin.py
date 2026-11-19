"""twin for hermes_state.py:iter_deleted_sqlite_sidecar_holders (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "iter_deleted_sqlite_sidecar_holders"
    C_FN = "iter_deleted_sqlite_sidecar_holders"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
