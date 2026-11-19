"""twin for tools/delegate_tool.py:_retain_recent_subagent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_retain_recent_subagent"
    C_FN = "dlg_retain_recent_subagent"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
