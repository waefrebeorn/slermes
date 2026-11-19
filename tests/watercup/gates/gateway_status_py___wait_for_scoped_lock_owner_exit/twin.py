"""twin for gateway/status.py:_wait_for_scoped_lock_owner_exit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "_wait_for_scoped_lock_owner_exit"
    C_FN = "gstat_u_wait_for_scoped_lock_owner_exit"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
