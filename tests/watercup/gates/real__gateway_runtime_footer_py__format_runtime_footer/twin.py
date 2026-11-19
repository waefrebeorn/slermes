"""twin for gateway/runtime_footer.py:format_runtime_footer (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/runtime_footer.py"
    PY_FN = "format_runtime_footer"
    C_FN = "format_runtime_footer"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
