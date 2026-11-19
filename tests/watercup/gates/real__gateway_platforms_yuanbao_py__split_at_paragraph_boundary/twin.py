"""twin for gateway/platforms/yuanbao.py:split_at_paragraph_boundary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "split_at_paragraph_boundary"
    C_FN = "split_at_paragraph_boundary"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
