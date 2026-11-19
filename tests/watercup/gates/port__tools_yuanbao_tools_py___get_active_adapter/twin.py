"""twin for tools/yuanbao_tools.py:_get_active_adapter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/yuanbao_tools.py"
    PY_FN = "_get_active_adapter"
    C_FN = "_get_active_adapter"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
