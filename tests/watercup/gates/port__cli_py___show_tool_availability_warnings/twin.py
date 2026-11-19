"""twin for cli.py:_show_tool_availability_warnings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_show_tool_availability_warnings"
    C_FN = "_show_tool_availability_warnings"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
