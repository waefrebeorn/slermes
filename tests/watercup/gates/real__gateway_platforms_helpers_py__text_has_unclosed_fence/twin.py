"""twin for gateway/platforms/helpers.py:text_has_unclosed_fence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/helpers.py"
    PY_FN = "text_has_unclosed_fence"
    C_FN = "helpers_md_text_has_unclosed_fence"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
