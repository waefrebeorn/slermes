"""twin for tools/apply_layout_tool.py:apply_layout_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/apply_layout_tool.py"
    PY_FN = "apply_layout_tool"
    C_FN = "apply_layout_tool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
