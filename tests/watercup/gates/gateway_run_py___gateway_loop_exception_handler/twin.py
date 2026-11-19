"""twin for gateway/run.py:_gateway_loop_exception_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_gateway_loop_exception_handler"
    C_FN = "gw_gateway_loop_exception_handler_is_transient"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
