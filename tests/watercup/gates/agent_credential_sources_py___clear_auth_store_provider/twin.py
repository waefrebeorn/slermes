"""twin for agent/credential_sources.py:_clear_auth_store_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_sources.py"
    PY_FN = "_clear_auth_store_provider"
    C_FN = "crs_clear_auth_store_provider"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
