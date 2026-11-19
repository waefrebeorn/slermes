"""twin for gateway/platforms/base.py:_notify_media_delivery_failure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_notify_media_delivery_failure"
    C_FN = "stts_notify_media_delivery_failure"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
