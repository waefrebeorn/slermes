"""twin for gateway/platforms/base.py:_media_delivery_allowed_roots (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_media_delivery_allowed_roots"
    C_FN = "_media_delivery_allowed_roots"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
