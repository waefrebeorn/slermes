"""twin for agent/credential_pool.py:resolve_runtime_pool_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "resolve_runtime_pool_key"
    C_FN = "resolve_runtime_pool_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
