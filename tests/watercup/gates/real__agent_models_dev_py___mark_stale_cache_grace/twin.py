"""twin for agent/models_dev.py:_mark_stale_cache_grace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_mark_stale_cache_grace"
    C_FN = "mdv_mark_stale_cache_grace"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
