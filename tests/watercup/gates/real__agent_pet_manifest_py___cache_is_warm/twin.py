"""twin for agent/pet/manifest.py:_cache_is_warm (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/manifest.py"
    PY_FN = "_cache_is_warm"
    C_FN = "cache_is_warm"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
