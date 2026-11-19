"""twin for tools/delegate_tool.py:_unregister_subagent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_unregister_subagent"
    C_FN = "delegate_unregister_subagent"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
