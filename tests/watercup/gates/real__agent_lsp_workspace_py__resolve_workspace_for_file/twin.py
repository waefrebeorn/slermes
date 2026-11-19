"""twin for agent/lsp/workspace.py:resolve_workspace_for_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lsp/workspace.py"
    PY_FN = "resolve_workspace_for_file"
    C_FN = "resolve_workspace_for_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
