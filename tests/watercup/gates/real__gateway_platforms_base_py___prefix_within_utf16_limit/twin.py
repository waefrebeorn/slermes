"""twin for gateway/platforms/base.py:_prefix_within_utf16_limit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_prefix_within_utf16_limit"
    C_FN = "_prefix_within_utf16_limit"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
