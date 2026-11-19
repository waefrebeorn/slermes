"""twin for gateway/platforms/yuanbao.py:_open_dm_opted_in (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "_open_dm_opted_in"
    C_FN = "gateway_dm_opted_in"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
