"""twin for tools/preview_tool.py:_handle_preview (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/preview_tool.py"
    PY_FN = "_handle_preview"
    C_FN = "_handle_preview"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
