"""twin for gateway/platforms/yuanbao.py:sanitize_markdown_table (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "sanitize_markdown_table"
    C_FN = "sanitize_markdown_table"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
