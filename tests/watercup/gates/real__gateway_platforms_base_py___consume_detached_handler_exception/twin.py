"""twin for gateway/platforms/base.py:_consume_detached_handler_exception (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_consume_detached_handler_exception"
    C_FN = "_consume_detached_handler_exception"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
