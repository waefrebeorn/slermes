"""twin for tools/delegate_tool.py:_build_child_agent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_build_child_agent"
    C_FN = "_build_child_agent"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
