"""twin for gateway/platforms/base.py:_invalidate_pending_stt_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_invalidate_pending_stt_cache"
    C_FN = "gw_base__invalidate_pending_stt_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
