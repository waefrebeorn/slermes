"""twin for agent/lsp/workspace.py:is_inside_workspace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/workspace.py"
    PY_FN = "is_inside_workspace"
    C_FN = "lsw_is_inside_workspace"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
