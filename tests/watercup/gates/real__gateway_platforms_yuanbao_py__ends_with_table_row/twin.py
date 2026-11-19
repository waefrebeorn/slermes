"""twin for gateway/platforms/yuanbao.py:ends_with_table_row (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "ends_with_table_row"
    C_FN = "yuanbao_md_ends_with_table_row"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
