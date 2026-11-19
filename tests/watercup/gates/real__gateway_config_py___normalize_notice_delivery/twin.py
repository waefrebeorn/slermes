"""twin for gateway/config.py:_normalize_notice_delivery (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"
    PY_FN = "_normalize_notice_delivery"
    C_FN = "_normalize_notice_delivery"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
