"""twin for tools/web_tools.py:_rescue_extract (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/web_tools.py"
    PY_FN = "_rescue_extract"
    C_FN = "_rescue_extract"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
