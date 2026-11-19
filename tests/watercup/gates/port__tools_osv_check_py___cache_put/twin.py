"""twin for tools/osv_check.py:_cache_put (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/osv_check.py"
    PY_FN = "_cache_put"
    C_FN = "cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
