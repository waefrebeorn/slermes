"""twin for agent/credential_pool.py:_normalize_pool_priorities (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_normalize_pool_priorities"
    C_FN = "cpl_normalize_pool_priorities"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
