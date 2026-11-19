"""twin for tools/delegate_tool.py:_trim_summary_with_footer (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_trim_summary_with_footer"
    C_FN = "_trim_summary_with_footer"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
