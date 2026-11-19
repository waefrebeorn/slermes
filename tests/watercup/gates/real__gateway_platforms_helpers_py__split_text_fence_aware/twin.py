"""twin for gateway/platforms/helpers.py:split_text_fence_aware (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/helpers.py"
    PY_FN = "split_text_fence_aware"
    C_FN = "split_text_fence_aware"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
