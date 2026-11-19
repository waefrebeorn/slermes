"""twin for gateway/platforms/webhook.py:_render_delivery_extra (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/webhook.py"
    PY_FN = "_render_delivery_extra"
    C_FN = "_render_delivery_extra"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
