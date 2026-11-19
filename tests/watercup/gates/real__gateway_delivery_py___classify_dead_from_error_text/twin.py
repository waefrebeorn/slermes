"""twin for gateway/delivery.py:_classify_dead_from_error_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery.py"
    PY_FN = "_classify_dead_from_error_text"
    C_FN = "_classify_dead_from_error_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
