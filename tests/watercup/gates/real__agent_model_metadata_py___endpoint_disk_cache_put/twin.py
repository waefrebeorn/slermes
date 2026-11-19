"""twin for agent/model_metadata.py:_endpoint_disk_cache_put (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_endpoint_disk_cache_put"
    C_FN = "_endpoint_disk_cache_put"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
