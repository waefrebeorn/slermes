"""twin for agent/credential_pool.py:has_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "has_credentials"
    C_FN = "cpl_has_credentials"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
