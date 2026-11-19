"""twin for gateway/platforms/whatsapp_common.py:_get_wsecret (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_common.py"
    PY_FN = "_get_wsecret"
    C_FN = "whatsapp_common_get_wsecret"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
