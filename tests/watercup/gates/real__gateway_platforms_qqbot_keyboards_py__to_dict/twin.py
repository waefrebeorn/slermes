"""twin for gateway/platforms/qqbot/keyboards.py:to_dict (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/keyboards.py"
    PY_FN = "to_dict"
    C_FN = "to_dict"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
