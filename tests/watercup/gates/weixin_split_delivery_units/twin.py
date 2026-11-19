"""twin for gateway/platforms/weixin.py:_split_delivery_units_for_weixin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/weixin.py"
    PY_FN = "_split_delivery_units_for_weixin"
    C_FN = "_split_delivery_units_for_weixin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
