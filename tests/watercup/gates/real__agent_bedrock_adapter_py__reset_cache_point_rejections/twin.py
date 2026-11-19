"""twin for agent/bedrock_adapter.py:reset_cache_point_rejections (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "reset_cache_point_rejections"
    C_FN = "reset_cache_point_rejections"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
