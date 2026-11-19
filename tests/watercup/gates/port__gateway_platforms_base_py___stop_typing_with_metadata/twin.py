"""twin for gateway/platforms/base.py:_stop_typing_with_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_stop_typing_with_metadata"
    C_FN = "gw_base__stop_typing_with_metadata"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
