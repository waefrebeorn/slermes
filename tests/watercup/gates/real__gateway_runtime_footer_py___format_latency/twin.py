"""twin for gateway/runtime_footer.py:_format_latency (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/runtime_footer.py"
    PY_FN = "_format_latency"
    C_FN = "_format_latency"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
