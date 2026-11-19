"""twin for gateway/platforms/base.py:_write_runtime_status_safe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_write_runtime_status_safe"
    C_FN = "gw_base__write_runtime_status_safe"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
