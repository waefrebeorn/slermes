"""twin for gateway/platforms/yuanbao.py:remove (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "remove"
    C_FN = "yb2_mw_remove"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
