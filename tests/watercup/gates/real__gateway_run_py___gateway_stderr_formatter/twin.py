"""twin for gateway/run.py:_gateway_stderr_formatter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_gateway_stderr_formatter"
    C_FN = "_gateway_stderr_formatter"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
