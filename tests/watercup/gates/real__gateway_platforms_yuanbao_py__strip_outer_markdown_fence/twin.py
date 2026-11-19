"""twin for gateway/platforms/yuanbao.py:strip_outer_markdown_fence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "strip_outer_markdown_fence"
    C_FN = "strip_outer_markdown_fence"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
