"""twin for gateway/runtime_footer.py:build_footer_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/runtime_footer.py"
    PY_FN = "build_footer_line"
    C_FN = "build_footer_line"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
