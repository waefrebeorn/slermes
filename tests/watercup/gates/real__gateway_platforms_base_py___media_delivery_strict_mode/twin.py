"""twin for gateway/platforms/base.py:_media_delivery_strict_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_media_delivery_strict_mode"
    C_FN = "pb_media_delivery_strict_mode"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
