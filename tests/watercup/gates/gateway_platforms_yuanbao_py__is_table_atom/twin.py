"""twin for gateway/platforms/yuanbao.py:is_table_atom (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "is_table_atom"
    C_FN = "yuanbao_md_is_table_atom"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
