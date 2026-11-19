"""twin for gateway/platforms/qqbot/keyboards.py:send (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/keyboards.py"
    PY_FN = "send"
    C_FN = "send"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
