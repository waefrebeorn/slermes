"""twin for agent/models_dev.py:_confirm_cache_not_modified (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_confirm_cache_not_modified"
    C_FN = "mdv_confirm_cache_not_modified"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
