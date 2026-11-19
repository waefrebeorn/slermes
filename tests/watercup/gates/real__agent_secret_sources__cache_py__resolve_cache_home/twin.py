"""twin for agent/secret_sources/_cache.py:resolve_cache_home (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/_cache.py"
    PY_FN = "resolve_cache_home"
    C_FN = "resolve_cache_home"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
