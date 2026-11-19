"""twin for agent/credential_pool.py:_acquire_lease_under_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_acquire_lease_under_lock"
    C_FN = "_acquire_lease_under_lock"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
